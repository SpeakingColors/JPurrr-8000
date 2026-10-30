#!/bin/bash
# Build the unsigned macOS installer (.pkg) from a finished build. Run it on macOS.
#   scripts/package_macos.sh <artefacts dir> <version> <out dir> [manual.pdf]
# <artefacts dir> is build/plugin/JPurrr8000_artefacts/Release (has VST3/ and Standalone/).
# Writes <out dir>/JPurrr-8000-<version>-macOS.pkg
# The bundles are signed ad hoc (no Apple developer account), the package is not signed or notarized.
set -euo pipefail
[ $# -ge 3 ] || { echo "usage: $0 <artefacts dir> <version> <out dir> [manual.pdf]" >&2; exit 2; }
ART=$1; VER=$2; OUT=$3; MANUAL=${4:-}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
NUMVER=${VER%%-*}

VST3="$ART/VST3/JPurrr-8000.vst3"
APP="$ART/Standalone/JPurrr-8000.app"
[ -d "$VST3" ] || { echo "Error: $VST3 not found" >&2; exit 1; }
[ -d "$APP" ] || { echo "Error: $APP not found" >&2; exit 1; }
[ -f "$VST3/Contents/MacOS/JPurrr-8000" ] || { echo "Error: no binary inside the VST3 bundle" >&2; exit 1; }

mkdir -p "$OUT"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# Both architectures must be inside (universal build).
for bin in "$VST3/Contents/MacOS/JPurrr-8000" "$APP/Contents/MacOS/JPurrr-8000"; do
    archs=$(lipo -archs "$bin")
    case "$archs" in
        *arm64*x86_64*|*x86_64*arm64*) ;;
        *) echo "Error: $bin is not universal (has: $archs)" >&2; exit 1 ;;
    esac
done

# Stage copies, sign ad hoc (arm64 needs a valid signature; this is not notarization).
mkdir -p "$WORK/vst3root" "$WORK/approot/Applications" "$WORK/approot/Library/Application Support/Studio 87/JPurrr-8000"
cp -R "$VST3" "$WORK/vst3root/"
cp -R "$APP" "$WORK/approot/Applications/"
SUP="$WORK/approot/Library/Application Support/Studio 87/JPurrr-8000"
cp "$ROOT/installer/macos/uninstall.command" "$SUP/uninstall.command"
chmod 755 "$SUP/uninstall.command"
cp "$ROOT/LICENSE" "$SUP/LICENSE"
cp "$ROOT/installer/macos/README.txt" "$SUP/README.txt"
[ -z "$MANUAL" ] || cp "$MANUAL" "$SUP/JPurrr-8000-manual.pdf"
# Icons. The finished .icns (16 px to 1024 px with @2x, tools/make_icons.py) goes into both bundles, before signing,
# because signing seals the Resources folder. JUCE already points the app's Info.plist at Icon.icns; the VST3 bundle
# gets the same file and the CFBundleIconFile key here if it has none.
add_icon() { # bundle folder
    local b=$1 plist="$1/Contents/Info.plist" name
    name=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIconFile" "$plist" 2>/dev/null || true)
    if [ -z "$name" ]; then
        name=JPurrr-8000
        /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string $name" "$plist"
    fi
    case "$name" in *.icns) ;; *) name=$name.icns ;; esac
    mkdir -p "$b/Contents/Resources"
    cp "$ROOT/installer/macos/jpurrr-8000.icns" "$b/Contents/Resources/$name"
}
add_icon "$WORK/vst3root/JPurrr-8000.vst3"
add_icon "$WORK/approot/Applications/JPurrr-8000.app"
codesign --force --deep --sign - "$WORK/vst3root/JPurrr-8000.vst3"
codesign --force --deep --sign - "$WORK/approot/Applications/JPurrr-8000.app"
codesign --verify --deep --strict "$WORK/vst3root/JPurrr-8000.vst3"
codesign --verify --deep --strict "$WORK/approot/Applications/JPurrr-8000.app"

# Component packages. Bundles must not be relocatable, or the installer may "update"
# a copy it finds somewhere else on the disk instead of installing to the fixed place.
make_component() { # root, install location, identifier, output
    local root=$1 loc=$2 id=$3 out=$4 plist="$WORK/$3.plist" i=0
    pkgbuild --analyze --root "$root" "$plist" >/dev/null
    while /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$plist" >/dev/null 2>&1; do
        i=$((i + 1))
    done
    pkgbuild --root "$root" --component-plist "$plist" --install-location "$loc" \
        --identifier "$id" --version "$NUMVER" "$out" >/dev/null
}
make_component "$WORK/vst3root" "/Library/Audio/Plug-Ins/VST3" music.studio87.jpurrr8000.vst3 "$WORK/vst3.pkg"
make_component "$WORK/approot" "/" music.studio87.jpurrr8000.app "$WORK/app.pkg"

mkdir -p "$WORK/res"
cp "$ROOT/installer/macos/welcome.html" "$WORK/res/welcome.html"
cp "$ROOT/LICENSE" "$WORK/res/license.txt"
sed -e "s/@VERSION@/$VER/g" -e "s/@NUMVERSION@/$NUMVER/g" "$ROOT/installer/macos/distribution.xml.in" > "$WORK/distribution.xml"

productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/res" --package-path "$WORK" \
    "$OUT/JPurrr-8000-$VER-macOS.pkg" >/dev/null
echo "$OUT/JPurrr-8000-$VER-macOS.pkg"
