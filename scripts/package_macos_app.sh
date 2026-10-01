#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
DIST_DIR="${ROOT_DIR}/dist"
APP_NAME="Milk Runner Visualizer.app"
SOURCE_APP="${BUILD_DIR}/${APP_NAME}"
PROJECTM_LIB_DIR="${ROOT_DIR}/external/projectm-install/lib"
PROJECTM_DYLIB_PATH="$(find "${PROJECTM_LIB_DIR}" -maxdepth 1 -name 'libprojectM-4.4.dylib' -print -quit)"
if [[ -z "${PROJECTM_DYLIB_PATH}" ]]; then
    PROJECTM_DYLIB_PATH="$(find "${PROJECTM_LIB_DIR}" -maxdepth 1 -name 'libprojectM-*.dylib' -print -quit)"
fi
PROJECTM_DYLIB_NAME="$(basename "${PROJECTM_DYLIB_PATH}")"

find_macdeployqt() {
    if [[ -n "${QT_DEPLOY_TOOL:-}" && -x "${QT_DEPLOY_TOOL}" ]]; then
        printf '%s' "${QT_DEPLOY_TOOL}"
        return 0
    fi
    if command -v macdeployqt >/dev/null 2>&1; then
        command -v macdeployqt
        return 0
    fi
    if command -v brew >/dev/null 2>&1; then
        local brew_qtbase
        brew_qtbase="$(brew --prefix qtbase 2>/dev/null || true)"
        if [[ -n "${brew_qtbase}" && -x "${brew_qtbase}/bin/macdeployqt" ]]; then
            printf '%s' "${brew_qtbase}/bin/macdeployqt"
            return 0
        fi
        local brew_qt
        brew_qt="$(brew --prefix qt 2>/dev/null || true)"
        if [[ -n "${brew_qt}" && -x "${brew_qt}/bin/macdeployqt" ]]; then
            printf '%s' "${brew_qt}/bin/macdeployqt"
            return 0
        fi
    fi
    if [[ -x "/opt/local/libexec/qt6/bin/macdeployqt" ]]; then
        printf '%s' "/opt/local/libexec/qt6/bin/macdeployqt"
        return 0
    fi
    local qt_dir
    for qt_dir in ~/Qt/6*/*/bin /Applications/Qt/6*/*/bin; do
        if [[ -x "${qt_dir}/macdeployqt" ]]; then
            printf '%s' "${qt_dir}/macdeployqt"
            return 0
        fi
    done
    return 1
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

require_file() {
    [[ -e "$1" ]] || die "Missing required path: $1"
}

log "Building ${APP_NAME}"
cmake --build "${BUILD_DIR}"

require_file "${SOURCE_APP}"
require_file "${PROJECTM_DYLIB_PATH}"

QT_DEPLOY_TOOL="$(find_macdeployqt || true)"
if [[ -z "${QT_DEPLOY_TOOL}" ]]; then
    die "macdeployqt not found. Set QT_DEPLOY_TOOL environment variable to its path."
fi
log "Using macdeployqt: ${QT_DEPLOY_TOOL}"

timestamp="$(date +%Y%m%d-%H%M%S)"
PACKAGE_DIR="${DIST_DIR}/MilkRunnerVisualizer-${timestamp}"
PACKAGE_APP="${PACKAGE_DIR}/${APP_NAME}"
PACKAGE_FRAMEWORKS="${PACKAGE_APP}/Contents/Frameworks"
PACKAGE_BINARY="${PACKAGE_APP}/Contents/MacOS/Milk Runner Visualizer"

log "Creating package folder ${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}"
ditto "${SOURCE_APP}" "${PACKAGE_APP}"
mkdir -p "${PACKAGE_FRAMEWORKS}"

DEPLOY_LIBPATHS=()
if command -v brew >/dev/null 2>&1; then
    for f in qtbase qtdeclarative qtmultimedia qtsvg qtshadertools; do
        prefix="$(brew --prefix "$f" 2>/dev/null || true)"
        if [[ -n "$prefix" && -d "$prefix/lib" ]]; then
            DEPLOY_LIBPATHS+=("-libpath=${prefix}/lib")
        fi
    done
fi

log "Deploying Qt frameworks and plugins"
"${QT_DEPLOY_TOOL}" "${PACKAGE_APP}" -always-overwrite -qmldir="${ROOT_DIR}/src/ui/qml" ${DEPLOY_LIBPATHS[@]+"${DEPLOY_LIBPATHS[@]}"}

log "Bundling libprojectM"
# PROJECTM_DYLIB_PATH is normally the libprojectM-4.4.dylib symlink.  The
# versioned target is not copied alongside it, so preserve the public name but
# dereference it into a real bundled dylib.
cp -L "${PROJECTM_DYLIB_PATH}" "${PACKAGE_FRAMEWORKS}/"
install_name_tool \
    -change "@rpath/${PROJECTM_DYLIB_NAME}" "@executable_path/../Frameworks/${PROJECTM_DYLIB_NAME}" \
    "${PACKAGE_BINARY}"

if otool -l "${PACKAGE_BINARY}" | grep -Fq "${PROJECTM_LIB_DIR}"; then
    log "Removing local projectM rpath"
    install_name_tool -delete_rpath "${PROJECTM_LIB_DIR}" "${PACKAGE_BINARY}" || true
fi

log "Packaged app is intentionally unsigned; sign it with scripts/notarize_macos_app.sh"

log "Validating packaged executable architectures"
lipo -info "${PACKAGE_BINARY}"

log "Validating packaged dylib architectures"
for lib in "${PACKAGE_FRAMEWORKS}"/*.dylib; do
    if [[ -f "${lib}" ]]; then
        lipo -info "${lib}"
    fi
done

log "Validating packaged links"
linked_libs="$(otool -L "${PACKAGE_BINARY}")"
printf '%s\n' "${linked_libs}"

if printf '%s\n' "${linked_libs}" | grep -E '/opt/local|/opt/homebrew|external/projectm-install' >/dev/null; then
    die "Packaged executable still links to a local build dependency."
fi

if [[ ! -d "${PACKAGE_APP}/Contents/PlugIns" ]]; then
    warn "Qt PlugIns folder was not deployed."
fi

log "Checking packaged Mach-O files for local dependency paths"
local_dependency_hits="$(
    find "${PACKAGE_APP}/Contents" -type f -perm -111 | while IFS= read -r file; do
        if otool -L "${file}" >/dev/null 2>&1; then
            if otool -L "${file}" | grep -E '/opt/local|/opt/homebrew|external/projectm-install' >/dev/null; then
                printf '%s\n' "${file}"
                otool -L "${file}" | grep -E '/opt/local|/opt/homebrew|external/projectm-install'
            fi
        fi
    done
)"

if [[ -n "${local_dependency_hits}" ]]; then
    printf '%s\n' "${local_dependency_hits}" >&2
    die "Packaged app still contains local dependency references."
fi

log "Package ready: ${PACKAGE_APP}"
