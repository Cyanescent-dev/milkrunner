#!/usr/bin/env bash
set -euo pipefail

ARM64_APP=""
X86_APP=""
OUTPUT_APP=""

usage() {
    cat <<'EOF'
Combine separate arm64 and x86_64 macOS .app bundles into a single Universal Binary .app.

Usage:
  scripts/make_universal_bundle.sh --arm64 <path/to/arm64.app> --x86 <path/to/x86_64.app> [options]

Options:
  --arm64 <path>    Path to the arm64 .app bundle
  --x86 <path>      Path to the x86_64 .app bundle
  --out <path>      Output path for the universal .app bundle
                    (default: dist/MilkRunnerVisualizer-Universal/Milk Runner Visualizer.app)
  -h, --help        Show this help.
EOF
}

while (($#)); do
    case "$1" in
        --arm64)
            ARM64_APP="$2"
            shift
            ;;
        --x86|--x86_64)
            X86_APP="$2"
            shift
            ;;
        --out)
            OUTPUT_APP="$2"
            shift
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

if [[ -z "${ARM64_APP}" || -z "${X86_APP}" ]]; then
    die "Both --arm64 and --x86 bundle paths must be specified. Run with --help for usage."
fi

if [[ ! -d "${ARM64_APP}" ]]; then
    die "arm64 app bundle not found: ${ARM64_APP}"
fi

if [[ ! -d "${X86_APP}" ]]; then
    die "x86_64 app bundle not found: ${X86_APP}"
fi

if [[ -z "${OUTPUT_APP}" ]]; then
    timestamp="$(date +%Y%m%d-%H%M%S)"
    OUTPUT_APP="dist/MilkRunnerVisualizer-Universal-${timestamp}/Milk Runner Visualizer.app"
fi

OUTPUT_DIR="$(dirname "${OUTPUT_APP}")"
mkdir -p "${OUTPUT_DIR}"

log "Copying arm64 bundle base to ${OUTPUT_APP}"
rm -rf "${OUTPUT_APP}"
ditto "${ARM64_APP}" "${OUTPUT_APP}"

log "Merging matching Mach-O binaries with lipo"
find "${OUTPUT_APP}/Contents" -type f | while IFS= read -r dest_file; do
    rel_path="${dest_file#"${OUTPUT_APP}"/}"
    src_x86_file="${X86_APP}/${rel_path}"

    # Every executable code object must exist in both architecture bundles.
    if file -b "${dest_file}" | grep -Fq "Mach-O"; then
        [[ -f "${src_x86_file}" ]] || die "Missing x86_64 Mach-O counterpart: ${rel_path}"
        file -b "${src_x86_file}" | grep -Fq "Mach-O" || die "x86_64 counterpart is not Mach-O: ${rel_path}"
        dest_archs="$(lipo -archs "${dest_file}" 2>/dev/null || true)"
        x86_archs="$(lipo -archs "${src_x86_file}" 2>/dev/null || true)"

        [[ "${dest_archs}" == *"arm64"* ]] || die "arm64 bundle has an unexpected architecture: ${rel_path} (${dest_archs})"
        [[ "${x86_archs}" == *"x86_64"* ]] || die "x86_64 bundle has an unexpected architecture: ${rel_path} (${x86_archs})"
        log "Lipo merging: ${rel_path}"
        temp_out="$(mktemp "${TMPDIR:-/tmp}/milkrunner-universal.XXXXXX")"
        lipo -create "${dest_file}" "${src_x86_file}" -output "${temp_out}"
        mv "${temp_out}" "${dest_file}"
    fi
done

log "Checking x86_64 bundle for code not present in arm64 bundle"
find "${X86_APP}/Contents" -type f | while IFS= read -r x86_file; do
    if file -b "${x86_file}" | grep -Fq "Mach-O"; then
        rel_path="${x86_file#"${X86_APP}"/}"
        [[ -f "${OUTPUT_APP}/${rel_path}" ]] || die "Missing arm64 Mach-O counterpart: ${rel_path}"
    fi
done

log "Verifying Universal Binary architectures"
find "${OUTPUT_APP}/Contents" -type f -perm -111 | while IFS= read -r file; do
    if file -b "${file}" | grep -Fq "Mach-O"; then
        printf '%s: ' "${file#"${OUTPUT_APP}"/}"
        lipo -info "${file}"
    fi
done

log "Universal bundle created successfully at: ${OUTPUT_APP}"
log "It is intentionally unsigned. Run scripts/notarize_macos_app.sh before distribution."
