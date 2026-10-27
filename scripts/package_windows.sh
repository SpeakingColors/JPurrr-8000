#!/bin/bash
# Build the Windows installer (Inno Setup) from a finished build. Run it in Git Bash on Windows.
#   scripts/package_windows.sh <artefacts dir> <version> <out dir> [manual.pdf]
# <artefacts dir> is build/plugin/JPurrr8000_artefacts/Release (has VST3/ and Standalone/).
# Writes <out dir>/JPurrr-8000-<version>-Windows-x64-Setup.exe
set -euo pipefail
[ $# -ge 3 ] || { echo "usage: $0 <artefacts dir> <version> <out dir> [manual.pdf]" >&2; exit 2; }
ART=$1; VER=$2; OUT=$3; MANUAL=${4:-}
ROOT=$(cd "$(dirname "$0")/.." && pwd)

VST3="$ART/VST3/JPurrr-8000.vst3"
EXE="$ART/Standalone/JPurrr-8000.exe"
[ -d "$VST3" ] || { echo "Error: $VST3 not found" >&2; exit 1; }
[ -f "$VST3/Contents/x86_64-win/JPurrr-8000.vst3" ] || { echo "Error: no x86_64-win binary inside the VST3 bundle" >&2; exit 1; }
[ -f "$EXE" ] || { echo "Error: $EXE not found" >&2; exit 1; }

ISCC=""
for c in "$(command -v iscc || true)" "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" "/c/Program Files/Inno Setup 6/ISCC.exe"; do
    if [ -n "$c" ] && [ -f "$c" ]; then ISCC=$c; break; fi
done
[ -n "$ISCC" ] || { echo "Error: Inno Setup (ISCC.exe) not found. Install it with: choco install innosetup" >&2; exit 1; }

mkdir -p "$OUT"
win() { cygpath -w "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; }
args=("/DAppVersion=$VER" "/DSourceVst3=$(win "$VST3")" "/DSourceExe=$(win "$EXE")"
      "/DRepoRoot=$(cygpath -w "$ROOT")" "/DOutputDir=$(cygpath -w "$(cd "$OUT" && pwd)")")
[ -z "$MANUAL" ] || args+=("/DManualPdf=$(win "$MANUAL")")
# Git Bash would turn /DName=value into a path; switch that off for this call
MSYS2_ARG_CONV_EXCL="*" "$ISCC" "${args[@]}" "$(cygpath -w "$ROOT/installer/windows/JPurrr-8000.iss")"
F="$OUT/JPurrr-8000-$VER-Windows-x64-Setup.exe"
[ -f "$F" ] || { echo "Error: Inno Setup did not write $F" >&2; exit 1; }
echo "$F"
