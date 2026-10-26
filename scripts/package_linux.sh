#!/bin/sh
# Build the Linux release tarball from a finished build.
#   scripts/package_linux.sh <artefacts dir> <version> <out dir> [manual.pdf]
# <artefacts dir> is build/plugin/JPurrr8000_artefacts/Release (has VST3/ and Standalone/).
# Writes <out dir>/JPurrr-8000-<version>-Linux-x86_64.tar.gz
set -eu
[ $# -ge 3 ] || { echo "usage: $0 <artefacts dir> <version> <out dir> [manual.pdf]" >&2; exit 2; }
ART=$1; VER=$2; OUT=$3; MANUAL=${4:-}
ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
NAME="JPurrr-8000-$VER-Linux-x86_64"

[ -d "$ART/VST3/JPurrr-8000.vst3" ] || { echo "Error: $ART/VST3/JPurrr-8000.vst3 not found" >&2; exit 1; }
[ -f "$ART/Standalone/JPurrr-8000" ] || { echo "Error: $ART/Standalone/JPurrr-8000 not found" >&2; exit 1; }
SO=$(find "$ART/VST3/JPurrr-8000.vst3/Contents" -name '*.so' | head -n 1)
[ -n "$SO" ] || { echo "Error: no .so inside the VST3 bundle" >&2; exit 1; }

mkdir -p "$OUT"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
D="$STAGE/$NAME"
mkdir -p "$D"
cp -R "$ART/VST3/JPurrr-8000.vst3" "$D/"
cp "$ART/Standalone/JPurrr-8000" "$D/JPurrr-8000"
chmod 755 "$D/JPurrr-8000"
cp "$ROOT/installer/linux/install.sh" "$D/install.sh"
chmod 755 "$D/install.sh"
cp "$ROOT/installer/linux/jpurrr-8000.png" "$ROOT/installer/linux/jpurrr-8000.svg" "$D/"
cp "$ROOT/LICENSE" "$D/"
[ -z "$MANUAL" ] || cp "$MANUAL" "$D/JPurrr-8000-manual.pdf"
cat > "$D/README.txt" <<EOF
JPurrr-8000 $VER for Linux (x86_64)

Easy way (no root, nothing outside your home folder):
  ./install.sh
It copies the plugin to ~/.vst3, the standalone app to ~/.local/bin and adds a menu entry.
Rescan plugins in your DAW afterwards.

All users:    sudo ./install.sh --system     (not for Bazzite and other immutable systems)
Remove again: ./install.sh --uninstall       (add --system if you installed it that way)
See first:    ./install.sh --dry-run

Just copy it yourself:
  Copy the folder JPurrr-8000.vst3 to ~/.vst3/ (create the folder if needed).

The binaries need the usual desktop libraries (ALSA, X11, FreeType, fontconfig, OpenGL),
which any desktop Linux has. It is built on Ubuntu 24.04; a distribution with an older glibc than that may refuse to load it.
Presets and settings are kept in your home folder and are never removed by the uninstaller.

Free and open source, AGPL-3.0-or-later (see LICENSE).
Not affiliated with or endorsed by Roland or Arturia.
https://github.com/SpeakingColors/JPurrr-8000
EOF
tar --sort=name --owner=0 --group=0 --numeric-owner -C "$STAGE" -czf "$OUT/$NAME.tar.gz" "$NAME"
echo "$OUT/$NAME.tar.gz"
