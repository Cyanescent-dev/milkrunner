#!/usr/bin/env bash
# Build libprojectM v4.1.6 (with Milk Runner's patch) for Linux into external/projectm-install.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="${ROOT_DIR}/external/projectm-4.1.6"
BUILD_DIR="${ROOT_DIR}/external/projectm-build-linux"
INSTALL_PREFIX="${ROOT_DIR}/external/projectm-install"
JOBS="${JOBS:-$(nproc)}"

"${ROOT_DIR}/scripts/fetch_projectm.sh"

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DBUILD_TESTING=OFF \
    -DENABLE_PLAYLIST=OFF \
    -DBUILD_SHARED_LIBS=ON \
    -DENABLE_SDL_UI=OFF
cmake --build "${BUILD_DIR}" -j "${JOBS}"
cmake --install "${BUILD_DIR}"
echo "libprojectM installed into ${INSTALL_PREFIX}"
