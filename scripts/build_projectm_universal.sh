#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="${ROOT_DIR}/external/projectm-4.1.6"
BUILD_DIR="${ROOT_DIR}/external/projectm-build-universal"
INSTALL_PREFIX="${ROOT_DIR}/external/projectm-install"
DEPLOYMENT_TARGET="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"
ARCHITECTURES="${CMAKE_OSX_ARCHITECTURES:-arm64;x86_64}"

usage() {
    cat <<EOF
Build libprojectM as a universal macOS binary (arm64 and x86_64).

Usage:
  scripts/build_projectm_universal.sh [options]

Options:
  --arch <archs>       Target architectures (default: "arm64;x86_64")
  --target <version>   macOS deployment target (default: "11.0")
  --clean              Clean build directory before configuring
  -h, --help           Show this help.
EOF
}

CLEAN=0
while (($#)); do
    case "$1" in
        --arch)
            ARCHITECTURES="$2"
            shift
            ;;
        --target)
            DEPLOYMENT_TARGET="$2"
            shift
            ;;
        --clean)
            CLEAN=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "$1" >&2
            exit 1
            ;;
    esac
    shift
done

log() {
    printf '\033[1;34m==>\033[0m %s\n' "$*"
}

die() {
    printf '\033[1;31merror:\033[0m %s\n' "$*" >&2
    exit 1
}

if [[ ! -d "${SOURCE_DIR}" ]]; then
    log "projectM source not found; fetching v4.1.6 and applying patches"
    "${ROOT_DIR}/scripts/fetch_projectm.sh"
fi

if [[ "${CLEAN}" -eq 1 && -d "${BUILD_DIR}" ]]; then
    log "Cleaning build directory ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

log "Configuring libprojectM for architectures: ${ARCHITECTURES} (macOS ${DEPLOYMENT_TARGET}+)"
cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DCMAKE_OSX_ARCHITECTURES="${ARCHITECTURES}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}" \
    -DBUILD_TESTING=OFF \
    -DENABLE_PLAYLIST=OFF \
    -DBUILD_SHARED_LIBS=ON \
    -DENABLE_SDL_UI=OFF

log "Compiling libprojectM"
cmake --build "${BUILD_DIR}"

log "Installing libprojectM into ${INSTALL_PREFIX}"
cmake --install "${BUILD_DIR}"

log "Verifying installed library architectures"
INSTALLED_LIB="${INSTALL_PREFIX}/lib/libprojectM-4.4.1.6.dylib"
if [[ -f "${INSTALLED_LIB}" ]]; then
    lipo -info "${INSTALLED_LIB}"
else
    die "Installed library not found at ${INSTALLED_LIB}"
fi

log "libprojectM universal build and installation complete!"
