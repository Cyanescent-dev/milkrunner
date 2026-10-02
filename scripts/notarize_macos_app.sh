#!/usr/bin/env bash
set -euo pipefail

APP_PATH=""
IDENTITY=""
NOTARY_PROFILE=""
ENTITLEMENTS=""
ZIP_PATH=""
SUBMIT=0

usage() {
    cat <<'EOF'
Sign a packaged Milk Runner app for Developer ID distribution and optionally notarize it.

Usage:
  scripts/notarize_macos_app.sh --app <Milk Runner Visualizer.app> --identity <Developer ID Application identity> [options]

Options:
  --app <path>                 Packaged arm64 or universal .app bundle.
  --identity <identity>        Developer ID Application certificate name or SHA-1 hash.
  --entitlements <path>        Optional, app-specific entitlements file.
  --notary-profile <profile>   Keychain profile already configured for notarytool.
  --submit                     Submit, wait for approval, staple, and validate.
  --zip <path>                 Submission ZIP path (default: sibling .zip file).
  -h, --help                   Show this help.

The script signs nested code from the inside out with hardened runtime and a
secure timestamp. It intentionally does not create a keychain profile or use
Apple account credentials directly.
EOF
}

die() { printf 'error: %s\n' "$*" >&2; exit 1; }
log() { printf '==> %s\n' "$*"; }

while (($#)); do
    case "$1" in
        --app) APP_PATH="$2"; shift ;;
        --identity) IDENTITY="$2"; shift ;;
        --entitlements) ENTITLEMENTS="$2"; shift ;;
        --notary-profile) NOTARY_PROFILE="$2"; shift ;;
        --zip) ZIP_PATH="$2"; shift ;;
        --submit) SUBMIT=1 ;;
        -h|--help) usage; exit 0 ;;
        *) die "Unknown option: $1" ;;
    esac
    shift
done

[[ -d "$APP_PATH" ]] || die "App bundle not found: $APP_PATH"
[[ -n "$IDENTITY" ]] || die "A Developer ID Application identity is required."
[[ -z "$ENTITLEMENTS" || -f "$ENTITLEMENTS" ]] || die "Entitlements file not found: $ENTITLEMENTS"
if [[ "$SUBMIT" -eq 1 ]]; then
    [[ -n "$NOTARY_PROFILE" ]] || die "--submit requires an existing --notary-profile."
fi

NESTED_SIGN_ARGS=(--force --sign "$IDENTITY" --options runtime --timestamp)
APP_SIGN_ARGS=(--force --sign "$IDENTITY" --options runtime --timestamp)
if [[ -n "$ENTITLEMENTS" ]]; then
    # Entitlements belong to the app executable; applying them to Qt and other
    # third-party nested code can invalidate an otherwise correct signature.
    APP_SIGN_ARGS+=(--entitlements "$ENTITLEMENTS")
fi

log "Signing nested Mach-O files and dylibs"
while IFS= read -r -d '' code_path; do
    codesign "${NESTED_SIGN_ARGS[@]}" "$code_path"
# Homebrew-provided dylibs are often not marked executable, even though they
# are Mach-O code.  Apple requires every bundled dylib to carry the Developer
# ID signature and secure timestamp, so include them explicitly.
done < <(find "$APP_PATH/Contents" -type f \( -perm -111 -o -name '*.dylib' \) -print0)

log "Signing nested bundles"
while IFS= read -r bundle_path; do
    codesign "${NESTED_SIGN_ARGS[@]}" "$bundle_path"
done < <(find "$APP_PATH/Contents" -depth -type d \( -name '*.framework' -o -name '*.app' -o -name '*.appex' -o -name '*.xpc' -o -name '*.bundle' -o -name '*.plugin' \))

log "Signing app bundle"
codesign "${APP_SIGN_ARGS[@]}" "$APP_PATH"
codesign --verify --deep --strict --verbose=2 "$APP_PATH"
codesign -dvvv --entitlements :- "$APP_PATH"

if [[ "$SUBMIT" -eq 0 ]]; then
    log "Signed and verified. Re-run with --submit and a notarytool keychain profile to notarize."
    exit 0
fi

if [[ -z "$ZIP_PATH" ]]; then
    ZIP_PATH="${APP_PATH%.app}.zip"
fi
log "Creating notarization archive: $ZIP_PATH"
rm -f "$ZIP_PATH"
ditto -c -k --keepParent "$APP_PATH" "$ZIP_PATH"

log "Submitting to Apple notarization service"
xcrun notarytool submit "$ZIP_PATH" --keychain-profile "$NOTARY_PROFILE" --wait
log "Stapling notarization ticket"
xcrun stapler staple "$APP_PATH"
xcrun stapler validate "$APP_PATH"
log "Notarized and stapled: $APP_PATH"
