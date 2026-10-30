#!/bin/bash
# Check that a built or installed bundle really carries the app icon (used by the CI and release workflows).
#   scripts/check_icons.sh mac-bundle <JPurrr-8000.app or JPurrr-8000.vst3>   needs macOS (plutil, iconutil)
#   scripts/check_icons.sh linux-exe <JPurrr-8000 standalone binary>          the window icon PNG is inside
# Windows has its own check: scripts/check_icons.ps1
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
[ $# -eq 2 ] || { echo "usage: $0 mac-bundle|linux-exe <path>" >&2; exit 2; }

case "$1" in
    mac-bundle)
        b=$2
        [ -d "$b" ] || { echo "Error: $b not found" >&2; exit 1; }
        plist="$b/Contents/Info.plist"
        name=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIconFile" "$plist" 2>/dev/null || true)
        [ -n "$name" ] || { echo "Error: $plist has no CFBundleIconFile" >&2; exit 1; }
        case "$name" in *.icns) ;; *) name=$name.icns ;; esac
        icns="$b/Contents/Resources/$name"
        [ -f "$icns" ] || { echo "Error: CFBundleIconFile is $name but $icns does not exist" >&2; ls -l "$b/Contents/Resources" >&2 || true; exit 1; }
        tmp=$(mktemp -d)
        trap 'rm -rf "$tmp"' EXIT
        iconutil -c iconset "$icns" -o "$tmp/x.iconset" || { echo "Error: iconutil cannot read $icns" >&2; exit 1; }
        n=$(find "$tmp/x.iconset" -name '*.png' | wc -l | tr -d ' ')
        [ "$n" -ge 10 ] || { echo "Error: $icns has only $n sizes" >&2; exit 1; }
        echo "OK: $b has $name with $n sizes"
        ;;
    linux-exe)
        exe=$2
        [ -f "$exe" ] || { echo "Error: $exe not found" >&2; exit 1; }
        python3 - "$exe" "$ROOT/assets/icon/jpurrr-8000-256.png" <<'PY'
import sys
exe = open(sys.argv[1], "rb").read()
png = open(sys.argv[2], "rb").read()
if png not in exe:
    sys.exit("Error: the window icon is not inside " + sys.argv[1])
print("OK: window icon is inside", sys.argv[1])
PY
        ;;
    *) echo "unknown check: $1" >&2; exit 2 ;;
esac
