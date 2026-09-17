# Milk Runner Visualizer

Milk Runner Visualizer is a free/open-source desktop visualizer frontend for
MilkDrop-compatible preset files. It is designed around a separated renderer,
audio input, preset library, playlist, and Qt/QML user interface so the project
can grow toward macOS, Windows, Linux, Android, and iOS.

The current MVP uses a QML dummy visualizer so the app shell, preset scanning,
playlist editing, settings, and audio fallback can be developed without blocking
on graphics integration. A `ProjectMRenderer` abstraction is included and can be
built when libprojectM 4.x is available.

## Status

Working now:

- Qt/QML app shell with Visualizer, Preset Library, Playlists, and Settings
- Local preset folder import and recursive `.milk` scanning
- Named playlist creation, add/remove/reorder, shuffle/sequential mode
- Playlist JSON persistence in the app config folder
- Settings persistence for preset folder, FPS cap, render scale, and low-power mode
- Favourites in the in-app library and playlist entries
- Previous/next preset controls
- Fullscreen toggle
- Demo audio generator and portable Qt microphone capture abstraction
- Unit tests for playlist JSON and preset scanning

Still needs wiring:

- A Qt OpenGL render item that owns the current context and calls
  `ProjectMRenderer::renderFrame()`
- Live audio routing into libprojectM once the OpenGL render item is active
- Crash-safe/bad-preset isolation
- Metadata extraction beyond filename heuristics

## Dependencies

On macOS, install the build tools and Qt with Homebrew:

```bash
brew install cmake ninja pkgconf qtbase qtdeclarative qtshadertools qtmultimedia
```

Or use the helper script:

```bash
scripts/install_build_deps_macos.sh
```

On macOS 12 Monterey, prefer the MacPorts helper because current Homebrew Qt is
blocked by newer OS/SDK requirements:

```bash
scripts/install_build_deps_macos_macports.sh --install-macports
```

If MacPorts is already installed:

```bash
scripts/install_build_deps_macos_macports.sh
```

To also attempt Homebrew's optional projectM formula:

```bash
scripts/install_build_deps_macos.sh --with-projectm
```

On macOS 12 Monterey, Homebrew's current Qt route is blocked. The `qt`
meta-formula pulls `qtmultimedia`, which requires macOS 13 Ventura or newer, and
the split Qt packages pull `molten-vk`, which needs newer Metal SDK symbols than
Xcode 14.0.1 provides. To stay on Monterey, use an alternate Qt source such as
MacPorts or an official/archived Qt 6 installer, or switch the MVP to a non-Qt
stack. To keep using Homebrew Qt, upgrade macOS to Ventura or newer.

Optional libprojectM support requires a projectM 4.x development install that
exports the `projectM4` CMake package. A universal (`arm64` + `x86_64`) build of
libprojectM is included in `external/projectm-install` and can be rebuilt at any
time using:

```bash
scripts/build_projectm_universal.sh
```

## Universal Binary (Apple Silicon & Intel)

Milk Runner Visualizer targets macOS 11.0+ and defaults to building a **Universal Binary**
(`arm64;x86_64`) on Apple platforms so the application runs natively on both Apple Silicon
(M1/M2/M3/M4) and Intel Macs.

### Configure

To configure for a Universal Binary (default):

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qtbase);$(brew --prefix qtdeclarative);$(brew --prefix qtshadertools);$(brew --prefix qtmultimedia)"
```

To configure explicitly for a single architecture (e.g. `arm64` or `x86_64`):

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qtbase);$(brew --prefix qtdeclarative);$(brew --prefix qtshadertools);$(brew --prefix qtmultimedia)" \
  -DCMAKE_OSX_ARCHITECTURES="arm64"
```

With MacPorts Qt:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="/opt/local/libexec/qt6;/opt/local"
```

With libprojectM:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qtbase);$(brew --prefix qtdeclarative);$(brew --prefix qtshadertools);$(brew --prefix qtmultimedia);$(pwd)/external/projectm-install" \
  -DMILK_RUNNER_ENABLE_PROJECTM=ON
```

If libprojectM is not found, CMake keeps building the dummy-renderer MVP.

## Build

```bash
cmake --build build
```

Verify universal binary architecture:

```bash
lipo -info "build/Milk Runner Visualizer.app/Contents/MacOS/Milk Runner Visualizer"
```

## Package & Distribute

Package the application bundle with deployed frameworks and ad-hoc code signature:

```bash
scripts/package_macos_app.sh
```

If you built separate single-architecture `arm64` and `x86_64` bundles (e.g. from single-arch Qt environments), you can combine them into a single Universal Binary bundle:

```bash
scripts/make_universal_bundle.sh \
  --arm64 "build-arm64/Milk Runner Visualizer.app" \
  --x86 "build-x86_64/Milk Runner Visualizer.app" \
  --out "dist/MilkRunnerVisualizer-Universal/Milk Runner Visualizer.app"
```

## Run

```bash
open "build/Milk Runner Visualizer.app"
```

Or run the executable directly from the bundle:

```bash
"build/Milk Runner Visualizer.app/Contents/MacOS/Milk Runner Visualizer"
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Presets

The app does not bundle community preset packs by default. Use the Preset
Library tab to import a local folder containing `.milk` files. The tiny demo
preset in `resources/demo-presets` is original placeholder content for testing
and documentation only.

## Known Limitations

- The first UI renders an animated QML visual instead of libprojectM output.
- macOS system audio capture is not available through normal app APIs; the Qt
  audio path captures user-approved input devices such as microphones.
- Playlist paths are saved relative to the active preset folder when possible,
  but moving playlists between machines still needs import/export polish.
- Mobile builds are architectural targets, not configured build products yet.

## Licensing

Milk Runner Visualizer is MIT licensed. Qt and libprojectM have their own
licenses and distribution obligations. See `docs/LEGAL_NOTES.md` for the
project-specific branding and preset-pack policy.
