# macOS Developer ID release

Milk Runner must be packaged separately for `arm64` and `x86_64`, because every
embedded Qt framework, plugin, and dylib must contain the same architecture as
the app executable. The current Homebrew Qt installation is arm64-only; it
cannot produce a valid universal release on its own.

## Prerequisites

- An Apple Developer Program membership and a **Developer ID Application**
  certificate installed in Keychain.
- An arm64 Qt toolchain and a matching x86_64 Qt toolchain. `macdeployqt` must
  come from the same architecture/toolchain as the app being packaged.
- Universal `libprojectM`; verify with:

  ```sh
  lipo -archs external/projectm-install/lib/libprojectM-4.4.1.6.dylib
  ```

- A `notarytool` keychain profile, created by the release owner using an Apple
  ID app-specific password or App Store Connect API key.

## Build and package each architecture

Configure independent release build directories, pointing `CMAKE_PREFIX_PATH`
at the matching Qt installation. For example:

```sh
cmake -S . -B build-release-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
BUILD_DIR="$PWD/build-release-arm64" scripts/package_macos_app.sh

cmake -S . -B build-release-x86_64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_PREFIX_PATH=/path/to/x86_64/qt
BUILD_DIR="$PWD/build-release-x86_64" QT_DEPLOY_TOOL=/path/to/x86_64/macdeployqt scripts/package_macos_app.sh
```

The packaging script deliberately leaves the app unsigned. Confirm that each
bundle is thin in the expected architecture before continuing.

## Create and notarize the universal app

```sh
scripts/make_universal_bundle.sh --arm64 /path/to/arm64.app --x86 /path/to/x86_64.app \
  --out "dist/Milk Runner Visualizer-universal.app"

scripts/notarize_macos_app.sh \
  --app "dist/Milk Runner Visualizer-universal.app" \
  --identity "Developer ID Application: Your Name (TEAMID)" \
  --notary-profile milk-runner-notary \
  --submit
```

`make_universal_bundle.sh` rejects a bundle if any embedded Mach-O object is
missing its opposite-architecture counterpart. `notarize_macos_app.sh` signs
nested code first, enables hardened runtime, verifies the signature, submits,
waits for Apple, staples, and validates the ticket.
