#!/bin/sh
# Write the release page text to stdout.
#   scripts/release_notes.sh <tag> [prerelease]     (second argument "true" marks a test build)
set -eu
TAG=$1; PRE=${2:-false}; VER=${TAG#v}
if [ "$PRE" = true ]; then
cat <<EOF
**Test build.** This version has not been checked on real machines yet. It may be rough. Reports are welcome on the issues page.

EOF
fi
cat <<EOF
# JPurrr-8000 $VER

A free and open source synth in the spirit of the late 90s JP series (VST3 and standalone), with its own mix ready FX chain.

## Downloads

| System | File |
|---|---|
| Windows 10 and 11, 64 bit | \`JPurrr-8000-$VER-Windows-x64-Setup.exe\` |
| macOS 11 or newer, Apple silicon and Intel | \`JPurrr-8000-$VER-macOS.pkg\` |
| Linux, x86_64 | \`JPurrr-8000-$VER-Linux-x86_64.tar.gz\` |
| Manual (PDF) | \`JPurrr-8000-manual.pdf\` |
| Checksums | \`SHA256SUMS.txt\` |

## Install

- **Windows:** close your DAW, run the Setup. It puts the VST3 in \`C:\\Program Files\\Common Files\\VST3\` and the standalone app in \`C:\\Program Files\\Studio 87\\JPurrr-8000\`. The setup is not signed, so SmartScreen may say "Windows protected your PC": click **More info**, then **Run anyway**.
- **macOS:** close your DAW. The .pkg is not signed or notarized by Apple (their program costs 99 US dollars a year, and this project is free), so a file downloaded in the browser is blocked the first time. It does not mean the file is infected: check it with \`SHA256SUMS.txt\`. Easiest, no warning: in Terminal run \`curl -L -o ~/Downloads/JPurrr-8000.pkg https://github.com/SpeakingColors/JPurrr-8000/releases/download/$TAG/JPurrr-8000-$VER-macOS.pkg\` and then \`open ~/Downloads/JPurrr-8000.pkg\`. If you used the browser: **System Settings, Privacy & Security, Open Anyway**, or \`xattr -d com.apple.quarantine <file>.pkg\` in Terminal. On macOS 14 and older right click the file, **Open**, **Open**. Full steps: https://github.com/SpeakingColors/JPurrr-8000/blob/main/docs/macos-install.md. The VST3 goes to \`/Library/Audio/Plug-Ins/VST3\`, the app to \`/Applications\`.
- **Linux:** unpack the tar.gz and run \`./install.sh\`. It installs for your user only (\`~/.vst3\`, \`~/.local/bin\`), no root needed, and \`./install.sh --uninstall\` removes it again. Or copy the \`JPurrr-8000.vst3\` folder to \`~/.vst3/\` yourself.

Rescan plugins in your DAW afterwards. Your presets and settings are never removed by an uninstall.

## Check your download

\`SHA256SUMS.txt\` lists the SHA-256 of every file. Linux and macOS: \`shasum -a 256 -c SHA256SUMS.txt\`. Windows PowerShell: \`Get-FileHash <file>\`.

## Licence

AGPL-3.0-or-later. Source code: https://github.com/SpeakingColors/JPurrr-8000

Not affiliated with or endorsed by Roland or Arturia. All names belong to their owners.
EOF
