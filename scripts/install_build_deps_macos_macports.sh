#!/usr/bin/env bash
set -euo pipefail

INSTALL_MACPORTS=0
WITH_QT_MULTIMEDIA=0
WITH_PROJECTM=0
NO_SELFUPDATE=0

MACPORTS_VERSION="2.12.5"
MACPORTS_MONTEREY_PKG="MacPorts-${MACPORTS_VERSION}-12-Monterey.pkg"
MACPORTS_MONTEREY_URL="https://github.com/macports/macports-base/releases/download/v${MACPORTS_VERSION}/${MACPORTS_MONTEREY_PKG}"

usage() {
    cat <<EOF
Install macOS build dependencies for Milk Runner Visualizer using MacPorts.

Usage:
  scripts/install_build_deps_macos_macports.sh [options]

Options:
  --install-macports   Download and install the MacPorts Monterey pkg if port(1)
                       is not already available. Requires admin privileges.
  --with-multimedia    Also install qt6-qtmultimedia for live audio capture.
                       The app can build without this and use demo audio.
  --with-projectm      Attempt to install a projectM/libprojectM port if found.
  --no-selfupdate      Skip 'sudo port selfupdate'.
  -h, --help           Show this help.

Installs:
  cmake ninja pkgconfig qt6-qtbase qt6-qtdeclarative qt6-qtshadertools

MacPorts installs Qt 6 under /opt/local/libexec/qt6. Use this configure command:

  cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="/opt/local/libexec/qt6;/opt/local"
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
        --install-macports)
            INSTALL_MACPORTS=1
            ;;
        --with-multimedia)
            WITH_QT_MULTIMEDIA=1
            ;;
        --with-projectm)
            WITH_PROJECTM=1
            ;;
        --no-selfupdate)
            NO_SELFUPDATE=1
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
    die "This helper is for macOS."
fi

macos_version="$(sw_vers -productVersion 2>/dev/null || echo "0.0")"
if [[ "$macos_version" != 12.* ]]; then
    warn "This script is tuned for macOS 12 Monterey; detected macOS $macos_version."
fi

ensure_macports() {
    if command -v port >/dev/null 2>&1; then
        return
    fi

    if [[ -x /opt/local/bin/port ]]; then
        export PATH="/opt/local/bin:/opt/local/sbin:$PATH"
        return
    fi

    if [[ "$INSTALL_MACPORTS" -ne 1 ]]; then
        cat >&2 <<EOF
MacPorts was not found.

Install the Monterey pkg from:
  $MACPORTS_MONTEREY_URL

Or rerun this script with:
  scripts/install_build_deps_macos_macports.sh --install-macports
EOF
        exit 2
    fi

    local pkg_path="/tmp/${MACPORTS_MONTEREY_PKG}"
    log "Downloading MacPorts ${MACPORTS_VERSION} Monterey pkg"
    curl -L --fail --output "$pkg_path" "$MACPORTS_MONTEREY_URL"

    log "Installing MacPorts pkg"
    sudo installer -pkg "$pkg_path" -target /

    export PATH="/opt/local/bin:/opt/local/sbin:$PATH"
    command -v port >/dev/null 2>&1 || die "MacPorts installer completed, but port(1) is still not available"
}

port_installed() {
    port installed "$1" 2>/dev/null | grep -q 'active'
}

install_port() {
    local name="$1"
    if port_installed "$name"; then
        log "$name is already installed"
        return
    fi
    log "Installing $name"
    sudo port install "$name"
}

ensure_macports

if [[ "$NO_SELFUPDATE" -eq 0 ]]; then
    log "Updating MacPorts ports tree"
    sudo port selfupdate
fi

install_port cmake
install_port ninja
install_port pkgconfig
install_port qt6-qtbase
install_port qt6-qtdeclarative
install_port qt6-qtshadertools

if [[ "$WITH_QT_MULTIMEDIA" -eq 1 ]]; then
    install_port qt6-qtmultimedia
fi

if [[ "$WITH_PROJECTM" -eq 1 ]]; then
    if port info projectM >/dev/null 2>&1; then
        install_port projectM
    elif port info libprojectM >/dev/null 2>&1; then
        install_port libprojectM
    else
        warn "No projectM/libprojectM MacPorts port was found. Build libprojectM separately later."
    fi
fi

cat <<'EOF'

MacPorts dependency install complete.

Configure with:

  cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="/opt/local/libexec/qt6;/opt/local"

Then build and test:

  cmake --build build
  ctest --test-dir build --output-on-failure
EOF
