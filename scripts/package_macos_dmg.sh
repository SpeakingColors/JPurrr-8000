#!/bin/bash
# Build the drag and drop macOS disk image (.dmg) from a finished build. Run it on macOS.
#   scripts/package_macos_dmg.sh <artefacts dir> <version> <out dir>
# <artefacts dir> is build/plugin/JPurrr8000_artefacts/Release (has VST3/).
# Writes <out dir>/JPurrr-8000-<version>-macOS.dmg
# The disk image has the plugin, a shortcut to the system VST3 folder and a short READ ME.
# The plugin is signed ad hoc (no Apple developer account); the image is not signed or notarized,
# so this does NOT remove the macOS download warning (see docs/macos-install.md).
set -euo pipefail
[ $# -ge 3 ] || { echo "usage: $0 <artefacts dir> <version> <out dir>" >&2; exit 2; }
ART=$1; VER=$2; OUT=$3
ROOT=$(cd "$(dirname "$0")/.." && pwd)

VST3="$ART/VST3/JPurrr-8000.vst3"
[ -d "$VST3" ] || { echo "Error: $VST3 not found" >&2; exit 1; }
[ -f "$VST3/Contents/MacOS/JPurrr-8000" ] || { echo "Error: no binary inside the VST3 bundle" >&2; exit 1; }

archs=$(lipo -archs "$VST3/Contents/MacOS/JPurrr-8000")
case "$archs" in
    *arm64*x86_64*|*x86_64*arm64*) ;;
    *) echo "Error: the plugin is not universal (has: $archs)" >&2; exit 1 ;;
esac

mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
STAGE="$WORK/JPurrr-8000"
mkdir -p "$STAGE"
cp -R "$VST3" "$STAGE/"

# Icon (same step as the .pkg): the .icns goes in before signing, because signing seals the Resources folder.
b="$STAGE/JPurrr-8000.vst3"
plist="$b/Contents/Info.plist"
name=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIconFile" "$plist" 2>/dev/null || true)
if [ -z "$name" ]; then
    name=JPurrr-8000
    /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string $name" "$plist"
fi
case "$name" in *.icns) ;; *) name=$name.icns ;; esac
mkdir -p "$b/Contents/Resources"
cp "$ROOT/installer/macos/jpurrr-8000.icns" "$b/Contents/Resources/$name"

# Ad hoc signature: Apple silicon needs a valid one to load a plugin. This is not notarization.
codesign --force --deep --sign - "$b"
codesign --verify --deep --strict "$b"

# Drop target: a link to the system VST3 folder (macOS asks for the admin password when you drop on it).
ln -s /Library/Audio/Plug-Ins/VST3 "$STAGE/Drop JPurrr-8000.vst3 here (VST3 folder)"
cp "$ROOT/installer/macos/dmg-README.txt" "$STAGE/READ ME FIRST.txt"
cp "$ROOT/LICENSE" "$STAGE/LICENSE"

DMG="$OUT/JPurrr-8000-$VER-macOS.dmg"
rm -f "$DMG"
hdiutil create -volname "JPurrr-8000" -srcfolder "$STAGE" -fs HFS+ -format UDZO -ov "$DMG" >/dev/null
hdiutil verify "$DMG" >/dev/null
echo "$DMG"
