#!/bin/sh
# Build the one-file download for a release: all installers, manual, INSTALL.txt, checksums.
#   scripts/make_release_zip.sh <dist dir> <tag> [out dir]
# <dist dir> holds the three installers and JPurrr-8000-manual.pdf (as the release workflow collects them).
# Writes <out dir>/JPurrr-8000-v<version>.zip (out dir defaults to <dist dir>). The zip has one top folder,
# JPurrr-8000-v<version>/, as the SupurrSaw zip does. In it: INSTALL.txt, LICENSE, SHA256SUMS, the manual, and the
# folders linux, macos, windows.
# Set SKIP_MACOS=true to build without the macOS package (the macOS build was skipped).
set -eu
[ $# -ge 2 ] || { echo "usage: $0 <dist dir> <tag> [out dir]" >&2; exit 2; }
DIST=$1; TAG=$2; OUT=${3:-$1}
VER=${TAG#v}
ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
printf '%s' "$TAG" | grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+(-[A-Za-z0-9.]+)?$' || { echo "Error: tag '$TAG' is not like v1.2.3" >&2; exit 1; }
NAME="JPurrr-8000-$VER"
WIN="$NAME-Windows-x64-Setup.exe"
MAC="$NAME-macOS.pkg"
LIN="$NAME-Linux-x86_64.tar.gz"
MANUAL="JPurrr-8000-manual.pdf"

need="$WIN $LIN $MANUAL"
[ "${SKIP_MACOS:-false}" = true ] || need="$need $MAC"
for f in $need; do
    [ -f "$DIST/$f" ] || { echo "Error: $DIST/$f not found" >&2; exit 1; }
done
[ -f "$ROOT/installer/INSTALL.txt.in" ] || { echo "Error: installer/INSTALL.txt.in not found" >&2; exit 1; }
[ -f "$ROOT/LICENSE" ] || { echo "Error: LICENSE not found" >&2; exit 1; }
command -v zip >/dev/null 2>&1 || { echo "Error: the zip program is not installed" >&2; exit 1; }

if command -v sha256sum >/dev/null 2>&1; then
    sum() { sha256sum "$@"; }
else
    sum() { shasum -a 256 "$@"; }
fi

mkdir -p "$OUT"
OUT=$(CDPATH='' cd -- "$OUT" && pwd)
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
D="$STAGE/JPurrr-8000-$TAG"
mkdir -p "$D/windows" "$D/linux"
cp "$DIST/$WIN" "$D/windows/"
cp "$DIST/$LIN" "$D/linux/"
if [ "${SKIP_MACOS:-false}" != true ]; then
    mkdir -p "$D/macos"
    cp "$DIST/$MAC" "$D/macos/"
fi
cp "$DIST/$MANUAL" "$D/"
cp "$ROOT/LICENSE" "$D/LICENSE"
sed -e "s/@VERSION@/$VER/g" -e "s/@TAG@/$TAG/g" "$ROOT/installer/INSTALL.txt.in" > "$D/INSTALL.txt"
if grep -q '@[A-Z]*@' "$D/INSTALL.txt"; then echo "Error: INSTALL.txt still has a placeholder" >&2; exit 1; fi
if [ "${SKIP_MACOS:-false}" = true ]; then
    printf '\nNOTE: the macOS package is not part of this release (that build was skipped).\n' >> "$D/INSTALL.txt"
fi

# Checksums of every file in the zip (paths relative to the top folder, so "shasum -a 256 -c" works there).
(
    cd "$D"
    find . -type f ! -name SHA256SUMS | sed 's|^\./||' | LC_ALL=C sort | while IFS= read -r f; do
        sum "$f"
    done > "$STAGE/sums.txt"
)
mv "$STAGE/sums.txt" "$D/SHA256SUMS"

ZIP="$OUT/JPurrr-8000-$TAG.zip"
rm -f "$ZIP"
(cd "$STAGE" && zip -X -q -r "$ZIP" "JPurrr-8000-$TAG")
echo "Wrote $ZIP"
