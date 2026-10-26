#!/bin/sh
# Remove JPurrr-8000 from macOS. Double click it, or run it in Terminal.
# It asks for your password only when it needs to remove files from system folders.
# Your presets and settings (in your user Library) are never touched.

echo "Removing JPurrr-8000 ..."
status=0

remove() {
    # $1 = path. Try without sudo first, then with sudo (asks for the password).
    [ -e "$1" ] || return 0
    if rm -rf -- "$1" 2>/dev/null; then
        echo "  removed $1"
    elif sudo rm -rf -- "$1"; then
        echo "  removed $1 (with sudo)"
    else
        echo "  could not remove $1" >&2
        status=1
    fi
}

remove "/Library/Audio/Plug-Ins/VST3/JPurrr-8000.vst3"
remove "$HOME/Library/Audio/Plug-Ins/VST3/JPurrr-8000.vst3"
remove "/Applications/JPurrr-8000.app"
remove "$HOME/Applications/JPurrr-8000.app"
remove "/Library/Application Support/Studio 87/JPurrr-8000"
rmdir "/Library/Application Support/Studio 87" 2>/dev/null || true

# Forget the installer receipts so a later install starts clean.
for id in music.studio87.jpurrr8000.vst3 music.studio87.jpurrr8000.app; do
    if pkgutil --pkg-info "$id" >/dev/null 2>&1; then
        sudo pkgutil --forget "$id" >/dev/null 2>&1 || true
    fi
done

if [ "$status" -eq 0 ]; then
    echo "Done. Your presets and settings were not touched."
else
    echo "Finished with errors, see above." >&2
fi
exit "$status"
