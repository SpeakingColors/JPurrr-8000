#!/bin/sh
# Print the project version from CMakeLists.txt. With a tag argument, fail if they differ.
#   scripts/check_version.sh            prints 0.1.0
#   scripts/check_version.sh v0.1.0     ok; v0.1.0-rc1 also ok (suffix is ignored); v0.2.0 fails
set -eu
ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
VER=$(sed -n 's/^[[:space:]]*project([^)]*VERSION[[:space:]]\{1,\}\([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -n 1)
[ -n "$VER" ] || { echo "Error: no project(... VERSION x.y.z) in CMakeLists.txt" >&2; exit 1; }
if [ $# -ge 1 ]; then
    TAG=${1#v}
    CORE=${TAG%%-*}
    case "$CORE" in
        [0-9]*.[0-9]*.[0-9]*) ;;
        *) echo "Error: tag '$1' is not like v1.2.3 or v1.2.3-rc1" >&2; exit 1 ;;
    esac
    if [ "$CORE" != "$VER" ]; then
        echo "Error: tag $1 does not match CMakeLists.txt project VERSION $VER. Change the version in CMakeLists.txt first." >&2
        exit 1
    fi
fi
printf '%s\n' "$VER"
