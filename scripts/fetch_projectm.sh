#!/usr/bin/env bash
# Fetch upstream libprojectM v4.1.6 into external/projectm-4.1.6 and apply Milk Runner's patch
# (preserve the caller's framebuffer bindings so projectM can render into an external target).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="${ROOT_DIR}/external/projectm-4.1.6"
PATCH="${ROOT_DIR}/patches/projectm-framebuffer.patch"

if [[ -d "${SOURCE_DIR}" ]]; then
    echo "projectM source already present: ${SOURCE_DIR}"
    exit 0
fi

mkdir -p "${ROOT_DIR}/external"
git clone --depth 1 --branch v4.1.6 --recurse-submodules --shallow-submodules \
    https://github.com/projectM-visualizer/projectm.git "${SOURCE_DIR}"
git -C "${SOURCE_DIR}" apply "${PATCH}"
echo "projectM v4.1.6 fetched and patched."
