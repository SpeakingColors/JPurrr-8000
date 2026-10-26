#!/bin/sh
# JPurrr-8000 installer for Linux. Plain POSIX sh, no sudo, no network.
#
#   ./install.sh                 install for the current user (default)
#   ./install.sh --system        install for all users (run it as root yourself)
#   ./install.sh --uninstall     remove what this script installed
#   ./install.sh --dry-run       show what would happen, change nothing
#   ./install.sh --from DIR      take the files from DIR (default: the folder of this script)
#
# User install:
#   ~/.vst3/JPurrr-8000.vst3
#   ~/.local/bin/JPurrr-8000                                  (standalone app)
#   ~/.local/share/applications/jpurrr-8000.desktop
#   ~/.local/share/icons/hicolor/256x256/apps/jpurrr-8000.png
# System install (--system, as root):
#   /usr/lib/vst3/JPurrr-8000.vst3, /usr/local/bin/JPurrr-8000,
#   /usr/share/applications/..., /usr/share/icons/hicolor/...
#   DESTDIR is honoured for packaging and tests.
#
# Running it twice is safe: the old copy is replaced. Your presets and settings are never touched.

set -eu

NAME="JPurrr-8000"
VST3_DIR_NAME="$NAME.vst3"
DESKTOP_NAME="jpurrr-8000.desktop"
ICON_NAME="jpurrr-8000.png"

MODE=user
ACTION=install
DRY=0
FROM=""

say() { printf '%s\n' "$*"; }
die() { printf 'Error: %s\n' "$*" >&2; exit 1; }

usage() {
    sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --user) MODE=user ;;
        --system) MODE=system ;;
        --uninstall) ACTION=uninstall ;;
        --dry-run|-n) DRY=1 ;;
        --from)
            [ $# -ge 2 ] || die "--from needs a folder"
            FROM=$2; shift ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown option: $1 (try --help)" ;;
    esac
    shift
done

SCRIPT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
[ -n "$FROM" ] || FROM=$SCRIPT_DIR

if [ "$MODE" = system ]; then
    if [ "$(id -u)" -ne 0 ] && [ -z "${DESTDIR:-}" ]; then
        die "--system needs root. Run it as root yourself (for example: sudo ./install.sh --system). On immutable systems such as Bazzite or Fedora Atomic /usr is read only: use the default user install instead."
    fi
    ROOT=${DESTDIR:-}
    VST3_DEST="$ROOT/usr/lib/vst3"
    BIN_DEST="$ROOT/usr/local/bin"
    APP_DEST="$ROOT/usr/share/applications"
    ICON_DEST="$ROOT/usr/share/icons/hicolor/256x256/apps"
    BIN_REAL="/usr/local/bin/$NAME"
else
    [ -n "${HOME:-}" ] || die "HOME is not set"
    DATA=${XDG_DATA_HOME:-$HOME/.local/share}
    VST3_DEST="$HOME/.vst3"
    BIN_DEST="$HOME/.local/bin"
    APP_DEST="$DATA/applications"
    ICON_DEST="$DATA/icons/hicolor/256x256/apps"
    BIN_REAL="$BIN_DEST/$NAME"
fi

run() {
    if [ "$DRY" -eq 1 ]; then
        say "  would run: $*"
    else
        "$@"
    fi
}

refresh_caches() {
    [ "$DRY" -eq 1 ] && return 0
    if [ "$MODE" = user ]; then
        command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APP_DEST" >/dev/null 2>&1 || true
        command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t "${DATA}/icons/hicolor" >/dev/null 2>&1 || true
    elif [ -z "${DESTDIR:-}" ]; then
        command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database /usr/share/applications >/dev/null 2>&1 || true
        command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
    fi
    return 0
}

# Find a file in the source folder, either flat (release tarball) or in a build tree.
find_src() {
    for cand in "$@"; do
        if [ -e "$cand" ]; then printf '%s\n' "$cand"; return 0; fi
    done
    return 1
}

if [ "$ACTION" = uninstall ]; then
    say "Removing $NAME ($MODE install)"
    removed=0
    for p in "$VST3_DEST/$VST3_DIR_NAME" "$BIN_DEST/$NAME" "$APP_DEST/$DESKTOP_NAME" "$ICON_DEST/$ICON_NAME"; do
        if [ -e "$p" ] || [ -L "$p" ]; then
            say "  remove $p"
            run rm -rf -- "$p"
            removed=$((removed + 1))
        fi
    done
    refresh_caches
    if [ "$removed" -eq 0 ]; then
        say "Nothing to remove: no $NAME files found in the $MODE locations."
    else
        say "Done. Your presets and settings were not touched."
    fi
    exit 0
fi

VST3_SRC=$(find_src "$FROM/$VST3_DIR_NAME" "$FROM/VST3/$VST3_DIR_NAME") || die "cannot find $VST3_DIR_NAME in $FROM"
[ -d "$VST3_SRC" ] || die "$VST3_SRC is not a folder"
BIN_SRC=$(find_src "$FROM/$NAME" "$FROM/Standalone/$NAME") || BIN_SRC=""
if [ -n "$BIN_SRC" ] && [ ! -f "$BIN_SRC" ]; then BIN_SRC=""; fi
ICON_SRC=$(find_src "$FROM/$ICON_NAME" "$SCRIPT_DIR/$ICON_NAME") || ICON_SRC=""

say "Installing $NAME ($MODE install)"
[ "$DRY" -eq 1 ] && say "(dry run: nothing is changed)"

# 1. VST3: copy to a temporary name first, then swap, so a failed copy never leaves half a plugin.
say "  VST3 plugin   -> $VST3_DEST/$VST3_DIR_NAME"
run mkdir -p "$VST3_DEST"
run rm -rf -- "$VST3_DEST/.$VST3_DIR_NAME.new"
run cp -R "$VST3_SRC" "$VST3_DEST/.$VST3_DIR_NAME.new"
run rm -rf -- "$VST3_DEST/$VST3_DIR_NAME"
run mv "$VST3_DEST/.$VST3_DIR_NAME.new" "$VST3_DEST/$VST3_DIR_NAME"

# 2. Standalone app, desktop entry, icon.
if [ -n "$BIN_SRC" ]; then
    say "  Standalone    -> $BIN_DEST/$NAME"
    run mkdir -p "$BIN_DEST"
    run cp "$BIN_SRC" "$BIN_DEST/.$NAME.new"
    run chmod 755 "$BIN_DEST/.$NAME.new"
    run mv -f "$BIN_DEST/.$NAME.new" "$BIN_DEST/$NAME"

    if [ -n "$ICON_SRC" ]; then
        say "  Icon          -> $ICON_DEST/$ICON_NAME"
        run mkdir -p "$ICON_DEST"
        run cp "$ICON_SRC" "$ICON_DEST/$ICON_NAME"
        run chmod 644 "$ICON_DEST/$ICON_NAME"
    else
        say "  (no icon file found, the menu entry will use a generic icon)"
    fi

    say "  Menu entry    -> $APP_DEST/$DESKTOP_NAME"
    run mkdir -p "$APP_DEST"
    if [ "$DRY" -eq 0 ]; then
        cat > "$APP_DEST/$DESKTOP_NAME" <<EOF
[Desktop Entry]
Type=Application
Name=JPurrr-8000
GenericName=Synthesizer
Comment=Free JP-8000 style synth with a mix ready FX chain
Exec=$BIN_REAL
Icon=jpurrr-8000
Terminal=false
Categories=AudioVideo;Audio;Midi;Music;
Keywords=synth;synthesizer;vst3;supersaw;trance;
StartupWMClass=JPurrr-8000
EOF
        chmod 644 "$APP_DEST/$DESKTOP_NAME"
    else
        say "  would write $APP_DEST/$DESKTOP_NAME"
    fi
else
    say "  (no standalone app found in $FROM, only the VST3 is installed)"
fi

refresh_caches

say ""
say "Done. Rescan plugins in your DAW if it does not show JPurrr-8000 yet."
if [ "$MODE" = user ] && [ -n "$BIN_SRC" ]; then
    case ":${PATH:-}:" in
        *":$BIN_DEST:"*) : ;;
        *) say "Note: $BIN_DEST is not in your PATH. The menu entry works anyway; to start the app from a terminal use $BIN_REAL." ;;
    esac
fi
say "To remove it again: $0 --uninstall$([ "$MODE" = system ] && printf ' --system')"
