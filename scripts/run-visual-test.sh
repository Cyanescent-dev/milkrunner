#!/bin/sh
# Linux compute recipe: real renderer, caller-selected preset/audio, fresh companion evidence.
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
export LD_LIBRARY_PATH="$PWD/external/projectm-install/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LIBGL_ALWAYS_SOFTWARE=1 QSG_RENDER_LOOP=basic QSG_NO_VSYNC=1 QT_QPA_PLATFORM=xcb
if [ -n "${MYCELIUM_VISUAL_STATUS:-}" ]; then
    set -- "$@" --automation-status "$MYCELIUM_VISUAL_STATUS" --readiness-timeout 120 \
        --run-seconds "${MYCELIUM_VISUAL_DURATION_SECONDS:?Missing bounded test runtime}"
fi
exec ./build/milk-runner-visualizer "$@"
