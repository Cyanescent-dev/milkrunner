#include "MacosSystemAudioInput.h"

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreGraphics/CoreGraphics.h>

#if __has_include(<ScreenCaptureKit/ScreenCaptureKit.h>)
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#define MILK_RUNNER_HAS_SCK 1
#else
#define MILK_RUNNER_HAS_SCK 0
#endif

#include <QPointer>
#include <QMetaObject>
#include <vector>
#include <algorithm>

#if MILK_RUNNER_HAS_SCK
API_AVAILABLE(macos(13.0))
@interface SCKSystemAudioDelegate : NSObject <SCStreamOutput, SCStreamDelegate>
@property (nonatomic, assign) MacosSystemAudioInput *owner;
@end

@implementation SCKSystemAudioDelegate

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type {
    if (type != SCStreamOutputTypeAudio || !_owner) {
        return;
    }

    CMAudioFormatDescriptionRef formatDesc = CMSampleBufferGetFormatDescription(sampleBuffer);
    if (!formatDesc) {
        return;
    }

    const AudioStreamBasicDescription *asbd = CMAudioFormatDescriptionGetStreamBasicDescription(formatDesc);
    if (!asbd || asbd->mSampleRate <= 0) {
        return;
    }

    size_t bufferListSize = 0;
    CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer,
        &bufferListSize,
        NULL,
        0,
        NULL,
        NULL,
        0,
        NULL
    );

    if (bufferListSize == 0) {
        return;
    }

    std::vector<uint8_t> bufferListStorage(bufferListSize);
    AudioBufferList *bufferList = reinterpret_cast<AudioBufferList*>(bufferListStorage.data());
    CMBlockBufferRef blockBuffer = NULL;

    OSStatus status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer,
        NULL,
        bufferList,
        bufferListSize,
        kCFAllocatorDefault,
        kCFAllocatorDefault,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment,
        &blockBuffer
    );

    if (status != noErr || bufferList->mNumberBuffers == 0) {
        if (blockBuffer) {
            CFRelease(blockBuffer);
        }
        return;
    }

    QVector<float> pcm;
    const int sampleRate = static_cast<int>(asbd->mSampleRate);
    const int channels = std::max(1, static_cast<int>(asbd->mChannelsPerFrame));

    if (asbd->mFormatFlags & kAudioFormatFlagIsFloat) {
        if (asbd->mFormatFlags & kAudioFormatFlagIsNonInterleaved) {
            const int frames = bufferList->mBuffers[0].mDataByteSize / sizeof(float);
            if (frames > 0) {
                pcm.resize(frames * 2);
                const float *left = reinterpret_cast<const float*>(bufferList->mBuffers[0].mData);
                const float *right = (bufferList->mNumberBuffers > 1 && bufferList->mBuffers[1].mData)
                    ? reinterpret_cast<const float*>(bufferList->mBuffers[1].mData)
                    : left;

                for (int i = 0; i < frames; ++i) {
                    pcm[i * 2] = left[i];
                    pcm[i * 2 + 1] = right[i];
                }
            }
        } else {
            const int sampleCount = bufferList->mBuffers[0].mDataByteSize / sizeof(float);
            if (sampleCount > 0) {
                const float *samples = reinterpret_cast<const float*>(bufferList->mBuffers[0].mData);
                pcm.resize(sampleCount);
                std::copy(samples, samples + sampleCount, pcm.begin());
            }
        }
    } else if (asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger) {
        if (asbd->mFormatFlags & kAudioFormatFlagIsNonInterleaved) {
            const int frames = bufferList->mBuffers[0].mDataByteSize / sizeof(int16_t);
            if (frames > 0) {
                pcm.resize(frames * 2);
                const int16_t *left = reinterpret_cast<const int16_t*>(bufferList->mBuffers[0].mData);
                const int16_t *right = (bufferList->mNumberBuffers > 1 && bufferList->mBuffers[1].mData)
                    ? reinterpret_cast<const int16_t*>(bufferList->mBuffers[1].mData)
                    : left;

                for (int i = 0; i < frames; ++i) {
                    pcm[i * 2] = static_cast<float>(left[i]) / 32768.0f;
                    pcm[i * 2 + 1] = static_cast<float>(right[i]) / 32768.0f;
                }
            }
        } else {
            const int sampleCount = bufferList->mBuffers[0].mDataByteSize / sizeof(int16_t);
            if (sampleCount > 0) {
                const int16_t *samples = reinterpret_cast<const int16_t*>(bufferList->mBuffers[0].mData);
                pcm.resize(sampleCount);
                for (int i = 0; i < sampleCount; ++i) {
                    pcm[i] = static_cast<float>(samples[i]) / 32768.0f;
                }
            }
        }
    }

    if (blockBuffer) {
        CFRelease(blockBuffer);
    }

    if (!pcm.isEmpty() && _owner) {
        MacosSystemAudioInput *ownerPtr = _owner;
        QMetaObject::invokeMethod(ownerPtr, [ownerPtr, pcm, sampleRate]() {
            emit ownerPtr->audioReady(pcm, sampleRate, 2);
        }, Qt::QueuedConnection);
    }
}

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error {
    if (error && _owner) {
        NSString *desc = [error localizedDescription];
        QString errorMsg = QString::fromUtf8([desc UTF8String]);
        MacosSystemAudioInput *ownerPtr = _owner;
        QMetaObject::invokeMethod(ownerPtr, [ownerPtr, errorMsg]() {
            emit ownerPtr->errorOccurred(QStringLiteral("System audio capture stopped: %1").arg(errorMsg));
            ownerPtr->stop();
        }, Qt::QueuedConnection);
    }
}

@end
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
class MacosSystemAudioInputPrivate
{
public:
#if MILK_RUNNER_HAS_SCK
    SCStream *stream = nil;
    SCKSystemAudioDelegate *delegate = nil;
    dispatch_queue_t audioQueue = nil;
#endif
};
#pragma clang diagnostic pop

MacosSystemAudioInput::MacosSystemAudioInput(QObject* parent)
    : AudioInput(parent)
    , d_(std::make_unique<MacosSystemAudioInputPrivate>())
{
}

MacosSystemAudioInput::~MacosSystemAudioInput()
{
    stop();
}

bool MacosSystemAudioInput::isSupported()
{
#if MILK_RUNNER_HAS_SCK
    if (@available(macOS 13.0, *)) {
        return true;
    }
#endif
    return false;
}

bool MacosSystemAudioInput::start()
{
    if (isRunning()) {
        return true;
    }

#if MILK_RUNNER_HAS_SCK
    if (@available(macOS 13.0, *)) {
        if (!CGPreflightScreenCaptureAccess()) {
            CGRequestScreenCaptureAccess();
            if (!CGPreflightScreenCaptureAccess()) {
                emit errorOccurred(QStringLiteral("Screen and System Audio Recording permission is required. Please grant permission in System Settings > Privacy & Security > Screen & System Audio Recording."));
                return false;
            }
        }

        d_->audioQueue = dispatch_queue_create("org.milkrunner.systemaudio", DISPATCH_QUEUE_SERIAL);
        d_->delegate = [[SCKSystemAudioDelegate alloc] init];
        d_->delegate.owner = this;

        QPointer<MacosSystemAudioInput> self = this;

        [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
            if (!self) {
                return;
            }

            if (error || !content || content.displays.count == 0) {
                NSString *errorDesc = error ? [error localizedDescription] : @"No active display found";
                QString msg = QString::fromUtf8([errorDesc UTF8String]);
                QMetaObject::invokeMethod(self, [self, msg]() {
                    if (self) {
                        emit self->errorOccurred(QStringLiteral("Failed to initialize system audio capture: %1").arg(msg));
                    }
                }, Qt::QueuedConnection);
                return;
            }

            SCDisplay *display = content.displays.firstObject;
            SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]];

            SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
            config.capturesAudio = YES;
            config.sampleRate = 44100;
            config.channelCount = 2;
            config.width = 2;
            config.height = 2;
            config.minimumFrameInterval = CMTimeMake(1, 1);

            if ([config respondsToSelector:@selector(setExcludesCurrentProcessAudio:)]) {
                [config setExcludesCurrentProcessAudio:YES];
            }

            self->d_->stream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:self->d_->delegate];

            NSError *streamError = nil;
            [self->d_->stream addStreamOutput:self->d_->delegate type:SCStreamOutputTypeAudio sampleHandlerQueue:self->d_->audioQueue error:&streamError];

            if (streamError) {
                NSString *errString = [streamError localizedDescription];
                QString msg = QString::fromUtf8([errString UTF8String]);
                QMetaObject::invokeMethod(self, [self, msg]() {
                    if (self) {
                        emit self->errorOccurred(QStringLiteral("Could not add system audio output: %1").arg(msg));
                    }
                }, Qt::QueuedConnection);
                return;
            }

            [self->d_->stream startCaptureWithCompletionHandler:^(NSError *startError) {
                if (!self) {
                    return;
                }
                if (startError) {
                    NSString *startErr = [startError localizedDescription];
                    QString msg = QString::fromUtf8([startErr UTF8String]);
                    QMetaObject::invokeMethod(self, [self, msg]() {
                        if (self) {
                            emit self->errorOccurred(QStringLiteral("Could not start system audio capture: %1").arg(msg));
                        }
                    }, Qt::QueuedConnection);
                } else {
                    QMetaObject::invokeMethod(self, [self]() {
                        if (self) {
                            self->setRunning(true);
                        }
                    }, Qt::QueuedConnection);
                }
            }];
        }];

        setRunning(true);
        return true;
    }
#endif

    emit errorOccurred(QStringLiteral("System audio capture via ScreenCaptureKit requires macOS 13 or newer. On older versions, use a virtual audio device (e.g. BlackHole)."));
    return false;
}

void MacosSystemAudioInput::stop()
{
    setRunning(false);

#if MILK_RUNNER_HAS_SCK
    if (@available(macOS 13.0, *)) {
        if (d_->delegate) {
            d_->delegate.owner = nullptr;
        }
        if (d_->stream) {
            [d_->stream stopCaptureWithCompletionHandler:^(NSError * _Nullable) {}];
            d_->stream = nil;
        }
        d_->delegate = nil;
        d_->audioQueue = nil;
    }
#endif
}

QString MacosSystemAudioInput::name() const
{
    return QStringLiteral("System Audio (Desktop Output)");
}
