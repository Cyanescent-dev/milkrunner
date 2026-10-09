#include "FileAudioInput.h"
#include <QFile>
#include <QFileInfo>
#include <chrono>
#ifdef MILK_RUNNER_HAS_FFMPEG
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}
#endif
FileAudioInput::FileAudioInput(QString path, bool loop, QObject* parent)
    : BufferedAudioInput(parent), path_(std::move(path)), loop_(loop)
{
}
FileAudioInput::~FileAudioInput()
{
    stop();
}
bool FileAudioInput::isSupported()
{
#ifdef MILK_RUNNER_HAS_FFMPEG
    return true;
#else
    return false;
#endif
}
bool FileAudioInput::start()
{
    stop();
    resetBuffer();
    QFile f(path_);
    if (!isSupported() || !QFileInfo(path_).isFile() || !f.open(QIODevice::ReadOnly))
    {
        report("error",
               "File audio unavailable: build lacks FFmpeg or file is unreadable: " + path_);
        emit errorOccurred(diagnostics()["error"].toString());
        return false;
    }
    {
        std::lock_guard lock(mutex_);
        metrics_["audio_source"] = "file";
        metrics_["filename"] = QFileInfo(path_).fileName();
        metrics_["loop_count"] = 0;
        metrics_["playback_position_seconds"] = 0;
    }
    stopping_ = false;
    drainTimer_.start();
    worker_ = std::thread([this] { decode(); });
    return true;
}
void FileAudioInput::stop()
{
    stopping_ = true;
    wake_.notify_all();
    if (worker_.joinable())
        worker_.join();
    drainTimer_.stop();
    setRunning(false);
    report("stopped");
}
void FileAudioInput::decode()
{
#ifdef MILK_RUNNER_HAS_FFMPEG
    AVFormatContext* format = avformat_alloc_context();
    AVCodecContext* codec = nullptr;
    SwrContext* resampler = nullptr;
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    auto cleanup = [&] {
        av_frame_free(&frame);
        av_packet_free(&packet);
        swr_free(&resampler);
        avcodec_free_context(&codec);
        avformat_close_input(&format);
    };
    auto error = [&](QString what, int code) {
        char text[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(code, text, sizeof(text));
        report("error", what + ": " + QString::fromUtf8(text));
    };
    if (!format || !packet || !frame)
    {
        report("error", "FFmpeg allocation failed");
        cleanup();
        return;
    }
    format->interrupt_callback = {
        [](void* p) { return static_cast<FileAudioInput*>(p)->stopping_.load() ? 1 : 0; }, this};
    int rc = avformat_open_input(&format, QFile::encodeName(path_).constData(), nullptr, nullptr);
    if (rc < 0)
    {
        error("Cannot open audio", rc);
        cleanup();
        return;
    }
    rc = avformat_find_stream_info(format, nullptr);
    if (rc < 0)
    {
        error("Cannot inspect audio", rc);
        cleanup();
        return;
    }
    const AVCodec* decoder = nullptr;
    int stream = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
    if (stream < 0)
    {
        error("No decodable audio stream", stream);
        cleanup();
        return;
    }
    codec = avcodec_alloc_context3(decoder);
    if (!codec)
    {
        report("error", "Decoder allocation failed");
        cleanup();
        return;
    }
    rc = avcodec_parameters_to_context(codec, format->streams[stream]->codecpar);
    if (rc >= 0)
        rc = avcodec_open2(codec, decoder, nullptr);
    if (rc < 0)
    {
        error("Cannot initialize decoder", rc);
        cleanup();
        return;
    }
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    rc = swr_alloc_set_opts2(&resampler, &stereo, AV_SAMPLE_FMT_FLT, 44100, &codec->ch_layout,
                             codec->sample_fmt, codec->sample_rate, 0, nullptr);
    if (rc >= 0)
        rc = swr_init(resampler);
    if (rc < 0)
    {
        error("Cannot initialize resampler", rc);
        cleanup();
        return;
    }
    {
        std::lock_guard lock(mutex_);
        metrics_["input_sample_rate"] = codec->sample_rate;
        metrics_["input_channels"] = codec->ch_layout.nb_channels;
    }
    using Clock = std::chrono::steady_clock;
    auto origin = Clock::now();
    qint64 delivered = 0, position = 0;
    int loops = 0;
    auto output = [&](const float* data, int count) {
        // Deliver 10 ms blocks at real-time pace, never tied to Qt/render ticks.
        for (int off = 0; off < count && !stopping_;)
        {
            int n = std::min(441, count - off);
            auto deadline = origin + std::chrono::nanoseconds(delivered * 1000000000LL / 44100);
            {
                std::unique_lock lock(waitMutex_);
                wake_.wait_until(lock, deadline, [this] { return stopping_.load(); });
            }
            if (stopping_)
                break;
            publish(data + off * 2, n);
            delivered += n;
            position += n;
            off += n;
            {
                std::lock_guard lock(mutex_);
                metrics_["playback_position_seconds"] = double(position) / 44100;
                metrics_["elapsed_audio_seconds"] = double(delivered) / 44100;
                metrics_["loop_count"] = loops;
            }
        }
    };
    auto convert = [&](AVFrame* decoded) {
        int capacity = decoded ? swr_get_out_samples(resampler, decoded->nb_samples)
                               : swr_get_out_samples(resampler, 0);
        if (capacity < 0 || capacity > 44100 * 10)
        {
            report("error", "Invalid decoded frame size");
            return false;
        }
        QVector<float> pcm(capacity * 2);
        uint8_t* out = reinterpret_cast<uint8_t*>(pcm.data());
        int n = swr_convert(resampler, &out, capacity,
                            decoded ? const_cast<const uint8_t**>(decoded->extended_data) : nullptr,
                            decoded ? decoded->nb_samples : 0);
        if (n < 0)
        {
            error("Audio conversion failed", n);
            return false;
        }
        output(pcm.constData(), n);
        return true;
    };
    bool healthy = true;
    while (!stopping_ && healthy)
    {
        rc = av_read_frame(format, packet);
        if (rc == AVERROR_EOF)
        {
            avcodec_send_packet(codec, nullptr);
            while (!stopping_ && (rc = avcodec_receive_frame(codec, frame)) >= 0)
            {
                healthy = convert(frame);
                av_frame_unref(frame);
                if (!healthy)
                    break;
            }
            if (healthy)
                healthy = convert(nullptr);
            if (!healthy || stopping_)
                break;
            if (!loop_)
            {
                report("ended");
                break;
            }
            if (position == 0)
            {
                report("error", "Audio stream decoded no samples");
                break;
            }
            rc = av_seek_frame(format, stream, 0, AVSEEK_FLAG_BACKWARD);
            if (rc < 0)
            {
                error("Cannot loop audio", rc);
                break;
            }
            avcodec_flush_buffers(codec);
            swr_close(resampler);
            rc = swr_init(resampler);
            if (rc < 0)
            {
                error("Cannot restart resampler", rc);
                break;
            }
            ++loops;
            position = 0;
            continue;
        }
        if (rc < 0)
        {
            if (!stopping_)
                error("Audio read failed", rc);
            break;
        }
        if (packet->stream_index != stream)
        {
            av_packet_unref(packet);
            continue;
        }
        rc = avcodec_send_packet(codec, packet);
        av_packet_unref(packet);
        if (rc < 0)
        {
            error("Decoder packet failure", rc);
            break;
        }
        while (!stopping_ && (rc = avcodec_receive_frame(codec, frame)) >= 0)
        {
            healthy = convert(frame);
            av_frame_unref(frame);
            if (!healthy)
                break;
        }
        if (rc != AVERROR(EAGAIN) && rc != AVERROR_EOF && rc < 0)
        {
            error("Decoder frame failure", rc);
            break;
        }
    }
    cleanup();
#endif
}
