# Linux audio implementation notes

Repository inspection: GitHub main is still the original imported source (1ddfc75).
The verified Oracle Linux/preset-controls build uses b0bd132 on codex/preset-test-controls
(PR #3), including the Linux port. This feature branches from that working source;
it must not replace the deployment with the older main build.

AudioInput publishes interleaved float PCM through audioReady. AppController owns
source selection and gain/stereo normalization on the GUI thread. The framebuffer
renderer owns projectM and submits PCM on the render thread with its GL context.
Previously its single pending block overwrote earlier audio during slow rendering.
New inputs use bounded mailboxes; decoding/capture runs independently of rendering.
The renderer queue is bounded to one second; dropped frames are explicitly counted.
Decoded and submitted frame counts differ when processing falls behind: neither
screenshots nor source RMS alone establish projectM delivery.

File audio is decoded by FFmpeg and resampled/mixed to 44.1 kHz stereo float on a
worker thread, paced by a monotonic clock. It needs no audio device/server. A GUI
output sink remains optional. Linux system audio uses libpulse asynchronous monitor
capture (also compatible with pipewire-pulse); no synthetic fallback is permitted.
macOS system capture and other existing inputs remain intact. FFmpeg is optional on
macOS; explicitly enabling a missing dependency fails configuration.

## Build and usage

Ubuntu 24.04:

```sh
sudo apt install libavformat-dev libavcodec-dev libavutil-dev libswresample-dev libpulse-dev ffmpeg
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Linux file/monitor features default ON. Explicitly disable with
`MILK_RUNNER_ENABLE_FILE_AUDIO=OFF` / `MILK_RUNNER_ENABLE_LINUX_SYSTEM_AUDIO=OFF`
for minimal builds. Enabling missing libraries fails configuration. FFmpeg 5+
(channel-layout API) is required. On macOS, existing capture remains native;
file audio can be enabled with `-DMILK_RUNNER_ENABLE_FILE_AUDIO=ON` and FFmpeg
libraries (e.g. Homebrew ffmpeg). These dependencies are not mandatory on macOS.

```sh
./build/milk-runner-visualizer --audio-file /path/music.mp3 --audio-loop
./build/milk-runner-visualizer --audio-source system --audio-device default
./build/milk-runner-visualizer --list-audio-devices
```

Explicit monitor names select a particular sink monitor. Default follows the
server's default sink and reconnects every two seconds after server loss. Device
listing waits briefly and exits nonzero if no monitor is available. No demo
fallback occurs for explicit file/system selection. GUI selector includes File
Audio, file picker, stop/restart and loop toggle; toggling loop restarts playback.
System Audio offers default/monitor selection and Refresh monitors. File audio
visualizes decoded samples without requiring a sound server; optional speaker
output is separate. Arbitrary seeking is not implemented; Restart starts at zero.

For automated tests include `--preset FILE --preset-interval 120
--automation-status evidence/audio.json --run-seconds 70` (on one command line).
`--run-seconds` gracefully exits Qt, releasing worker threads and decoder/capture
resources. Status JSON is atomically written every second and on clean exit.
`audio` includes format, source/status/error, decoded/output frames, RMS/peak,
non-silent frames, position, loop count and buffer drops. `audio_delivery` counts
actual PCM submitted to projectM, non-silent submitted frames, submission RMS/peak,
render frames/elapsed/average FPS and render-queue drops. Source counters apply to
the current source instance; renderer counters apply to the application lifetime.
`process` includes current RSS, peak RSS and cumulative CPU time. Differences in
frame counters can include queued audio at snapshot time or reported drops; do
not infer loss solely from one intermediate sample. RMS/peak source values are
before controller gain; submission values are after gain/limiting.

Two one-second queues bound memory and playback latency. If the GUI or rendering
stalls longer than a second, older pending PCM is discarded and counted, keeping
playback tied to elapsed time rather than slowing the song to render FPS. The
projectM adapter feeds all retained PCM in API-sized chunks with its GL context.

## Tests

Audio tests generate small real WAV/MP3/FLAC fixtures with ffmpeg, cover mono/stereo,
48 kHz conversion, looping/EOF, restart, invalid/missing files, 2 FPS GUI processing
and >1 second stalls/buffer drops. Monitor integration requires a reachable server;
absent prerequisites return CTest SKIP (77). Linux recovery tests launch an isolated
null-sink server and cover late availability, default sink switching, disappearance,
reconnection and shutdown. They do not alter the user's real server.

The optional Oracle harness `tests/run_oracle_audio_acceptance.py` is launched via
Mycelium compute_desktop_launch, not SSH. It runs 70 seconds for file playback
(with PULSE_SERVER deliberately unavailable), or 25 seconds of real MP3 -> ffmpeg
Pulse sink -> monitor capture. It records once-per-second history and cleans up
its children. Screenshot capture remains the generic Mycelium tool.

## Verification scope

Oracle ARM64/Qt 6.4 and macOS ARM64/Qt 6.11 both compile. macOS FFmpeg file tests
run with the feature enabled; Linux-only capture tests SKIP there. Existing native
macOS capture implementation is unchanged and compiled, but interactive macOS
ScreenCaptureKit permissions/live capture were not retested in this work. No CI
configuration exists in the repository; CTest remains the reproducible test entry.
Arbitrary seeking/pause-with-position preservation and instantaneous min/max FPS
are not implemented. Restart, real-time playback and average rendering FPS are.

FFmpeg sample conversion follows the [libswresample API](https://ffmpeg.org/doxygen/6.1/group__lswr.html).
