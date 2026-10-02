#!/usr/bin/env bash
set -euo pipefail

WITH_PROJECTM=0
NO_UPDATE=0

usage() {
    cat <<'EOF'
Install macOS build dependencies for Milk Runner Visualizer.

Usage:
  scripts/install_build_deps_macos.sh [options]

Options:
  --with-projectm   Attempt to install Homebrew's projectM formula if available.
                    The app can still build without this; it will use DummyRenderer.
  --no-update       Skip `brew update` before installing packages.
  -h, --help        Show this help.

Installs:
  cmake ninja pkgconf qtbase qtdeclarative qtshadertools

On macOS 12 Monterey, Homebrew's current Qt route is blocked: the qt
meta-formula pulls qtmultimedia, which requires macOS 13, and the split Qt
packages pull molten-vk, which needs newer Metal SDK symbols than Xcode 14.0.1
provides. Use an alternate Qt source on Monterey, or upgrade macOS for Homebrew
Qt.
EOF
}

log() {
    printf '\033[1;34m==>\033[0m %s\n' "$*"
}

warn() {
    printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2
}

die() {
    printf '\033[1;31merror:\033[0m %s\n' "$*" >&2
    exit 1
}

while (($#)); do
    case "$1" in
        --with-projectm)
            WITH_PROJECTM=1
            ;;
        --no-update)
            NO_UPDATE=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "Unknown option: $1"
            ;;
    esac
    shift
done

if [[ "$(uname -s)" != "Darwin" ]]; then
    die "This helper is for macOS. Install CMake, Ninja, Qt 6, and pkg-config/pkgconf with your platform package manager."
fi

version_at_least() {
    local left="$1"
    local right="$2"
    [[ "$(printf '%s\n%s\n' "$right" "$left" | sort -V | head -n 1)" == "$right" ]]
}

macos_version="$(sw_vers -productVersion 2>/dev/null || echo "0.0")"
xcode_version="$(xcodebuild -version 2>/dev/null | awk '/^Xcode / { print $2; exit }')"

if [[ "$macos_version" == 12.* ]]; then
    cat >&2 <<EOF
This Mac is running macOS $macos_version with Xcode ${xcode_version:-not found}.

Homebrew's current Qt path is not viable on this Monterey setup:
  - brew install qt pulls qtmultimedia, which requires macOS 13 Ventura or newer.
  - brew install qtbase/qtdeclarative pulls molten-vk 1.4.x.
  - molten-vk 1.4.x uses newer Metal SDK symbols such as
    MTLLanguageVersion3_0/3_1, and the compatible Xcode release is not available
    for this macOS version.

Installed build tools that do not require Qt can still be used:
  brew install cmake ninja pkgconf

To finish the Qt install, choose one of these routes:
  1. Upgrade macOS to Ventura or newer, then rerun this script; or
  2. Stay on Monterey and use an alternate Qt source that supports macOS 12,
     such as MacPorts or an official/archived Qt 6 installer; or
  3. Change this project to a non-Qt MVP stack for Monterey.
EOF
    exit 2
fi

if ! command -v brew >/dev/null 2>&1; then
    cat >&2 <<'EOF'
Homebrew is required but was not found.

Install Homebrew from https://brew.sh, then rerun:

  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  scripts/install_build_deps_macos.sh
EOF
    exit 1
fi

BREW="$(command -v brew)"
eval "$("$BREW" shellenv)"
BREW="$(command -v brew)"

formula_installed() {
    "$BREW" list --formula --versions "$1" >/dev/null 2>&1
}

formula_prefix() {
    "$BREW" --prefix "$1" 2>/dev/null || true
}

formula_healthy() {
    local formula="$1"
    local prefix
    prefix="$(formula_prefix "$formula")"

    [[ -n "$prefix" && -d "$prefix" ]] || return 1

    case "$formula" in
        cmake)
            [[ -x "$prefix/bin/cmake" ]] || command -v cmake >/dev/null 2>&1
            ;;
        ninja)
            [[ -x "$prefix/bin/ninja" ]] || command -v ninja >/dev/null 2>&1
            ;;
        qtbase)
            [[ -f "$prefix/lib/cmake/Qt6/Qt6Config.cmake" ]]
            ;;
        qtdeclarative)
            [[ -f "$prefix/lib/cmake/Qt6Qml/Qt6QmlConfig.cmake" ]] &&
                [[ -f "$prefix/lib/cmake/Qt6Quick/Qt6QuickConfig.cmake" ]]
            ;;
        qtshadertools)
            [[ -f "$prefix/lib/cmake/Qt6ShaderTools/Qt6ShaderToolsConfig.cmake" ]]
            ;;
        qtmultimedia)
            [[ -f "$prefix/lib/cmake/Qt6Multimedia/Qt6MultimediaConfig.cmake" ]]
            ;;
        pkgconf)
            [[ -x "$prefix/bin/pkg-config" || -x "$prefix/bin/pkgconf" ]] ||
                command -v pkg-config >/dev/null 2>&1 ||
                command -v pkgconf >/dev/null 2>&1
            ;;
        projectm)
            [[ -d "$prefix" ]]
            ;;
        *)
            [[ -d "$prefix" ]]
            ;;
    esac
}

install_or_reinstall_formula() {
    local formula="$1"

    if formula_installed "$formula" && formula_healthy "$formula"; then
        log "$formula is already installed"
        return
    fi

    if formula_installed "$formula"; then
        warn "$formula appears installed but incomplete; reinstalling"
        "$BREW" reinstall "$formula"
    else
        log "Installing $formula"
        "$BREW" install "$formula"
    fi

    formula_healthy "$formula" || die "$formula installed, but expected files were not found"
}

if [[ "$NO_UPDATE" -eq 0 ]]; then
    log "Updating Homebrew metadata"
    "$BREW" update
fi

log "Installing required build dependencies"
install_or_reinstall_formula cmake
install_or_reinstall_formula ninja
install_or_reinstall_formula pkgconf
install_or_reinstall_formula qtbase
install_or_reinstall_formula qtdeclarative
install_or_reinstall_formula qtshadertools
install_or_reinstall_formula qtmultimedia

PROJECTM_PREFIX=""
if [[ "$WITH_PROJECTM" -eq 1 ]]; then
    log "Attempting optional libprojectM install"
    if "$BREW" info projectm >/dev/null 2>&1; then
        if install_or_reinstall_formula projectm; then
            PROJECTM_PREFIX="$(formula_prefix projectm)"
        fi
    else
        warn "Homebrew projectm formula is not available in this tap set. Build libprojectM separately and add its prefix to CMAKE_PREFIX_PATH."
    fi
fi

QTBASE_PREFIX="$(formula_prefix qtbase)"
QTDECLARATIVE_PREFIX="$(formula_prefix qtdeclarative)"
QTSHADERTOOLS_PREFIX="$(formula_prefix qtshadertools)"
QTMULTIMEDIA_PREFIX="$(formula_prefix qtmultimedia)"
[[ -n "$QTBASE_PREFIX" && -d "$QTBASE_PREFIX" ]] || die "Could not locate qtbase prefix after installation"
[[ -n "$QTDECLARATIVE_PREFIX" && -d "$QTDECLARATIVE_PREFIX" ]] || die "Could not locate qtdeclarative prefix after installation"
[[ -n "$QTSHADERTOOLS_PREFIX" && -d "$QTSHADERTOOLS_PREFIX" ]] || die "Could not locate qtshadertools prefix after installation"
[[ -n "$QTMULTIMEDIA_PREFIX" && -d "$QTMULTIMEDIA_PREFIX" ]] || die "Could not locate qtmultimedia prefix after installation"

CMAKE_PREFIX_PATH_VALUE="$QTBASE_PREFIX;$QTDECLARATIVE_PREFIX;$QTSHADERTOOLS_PREFIX;$QTMULTIMEDIA_PREFIX"
if [[ -n "$PROJECTM_PREFIX" ]]; then
    CMAKE_PREFIX_PATH_VALUE="$CMAKE_PREFIX_PATH_VALUE;$PROJECTM_PREFIX"
fi

log "Installed versions"
cmake --version | head -n 1 || true
ninja --version || true
printf 'qtbase prefix: %s\n' "$QTBASE_PREFIX"
printf 'qtdeclarative prefix: %s\n' "$QTDECLARATIVE_PREFIX"
printf 'qtshadertools prefix: %s\n' "$QTSHADERTOOLS_PREFIX"
printf 'qtmultimedia prefix: %s\n' "$QTMULTIMEDIA_PREFIX"
if [[ -n "$PROJECTM_PREFIX" ]]; then
    printf 'projectM prefix: %s\n' "$PROJECTM_PREFIX"
fi

cat <<EOF

Next configure command (defaults to Universal Binary arm64 + x86_64 when dependencies permit):

  cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH_VALUE"

Or explicitly for host-only architecture (e.g. arm64 or x86_64):

  cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH_VALUE" -DCMAKE_OSX_ARCHITECTURES="\$(uname -m)"

Then build, test, and package:

  cmake --build build
  ctest --test-dir build --output-on-failure
  scripts/package_macos_app.sh
EOF
