# Architecture

Milk Runner Visualizer keeps app concerns separated so the frontend can move
from a macOS MVP toward desktop and mobile platforms without hard-wiring the UI
to a specific rendering or audio backend.

## Layers

- `src/render`: renderer abstraction, dummy renderer, optional libprojectM C API
  wrapper.
- `src/audio`: audio source abstraction, demo generator, Qt Multimedia capture.
- `src/presets`: preset metadata, recursive scanner, QML-facing library model.
- `src/playlists`: playlist JSON model and persistent playlist manager.
- `src/app`: application controller and settings persistence.
- `src/ui/qml`: Qt Quick views for the visualizer, library, playlist editor, and
  settings.

## Renderer Boundary

The rest of the app talks to `IVisualizerRenderer`, not directly to libprojectM.
`ProjectMRenderer` wraps the public libprojectM C API when available. The Qt UI
currently uses `DummyRenderer` and an animated QML view so playlist and settings
work can continue before the OpenGL render item is completed.

The next renderer step is to add a Qt Quick OpenGL item that:

- forces or requests the Qt Quick OpenGL backend where needed;
- creates or receives a current OpenGL context;
- initializes `ProjectMRenderer` on the render thread;
- forwards resize, preset, timing, FPS, and audio data;
- calls `renderFrame()` once per frame.

## Audio Boundary

`AudioInput` emits normalized floating-point PCM blocks. `QtAudioInput` attempts
portable microphone/input-device capture through Qt Multimedia. `DemoAudioInput`
produces synthetic stereo audio so rendering can be tested when live capture is
blocked or unavailable.

macOS generally does not expose system output capture to ordinary apps. Users
who need system-audio visualization will likely need a virtual audio device.

## Playlist Storage

Playlists are saved as JSON files under the platform app config folder. Paths
are stored relative to the active preset folder when possible. This keeps the
format readable while leaving room for later import/export workflows.

## Mobile Notes

Android and iOS will need platform-specific audio permission flows, storage
import handling, lifecycle handling, and rendering backend validation. The core
models avoid desktop-only APIs where practical, but the initial app target is
macOS desktop.
