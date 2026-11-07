JPurrr-8000 for macOS, drag and drop (Apple silicon and Intel)
==============================================================

1. Close your DAW.
2. Drag JPurrr-8000.vst3 onto the folder shortcut in this window.
   macOS asks for your password: that folder is for all users.
3. Rescan plugins in your DAW.

Where it goes: /Library/Audio/Plug-Ins/VST3/JPurrr-8000.vst3
To remove it later: delete that file. Your presets and settings are kept.

This disk image is not signed or notarized by Apple (that needs a paid
developer account), so a copy downloaded in the browser can be blocked the
first time. It does not mean the file is infected. If macOS blocks the
plugin when your DAW loads it, run this in Terminal once:

  xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/JPurrr-8000.vst3

and rescan. No warning at all: download the file in Terminal with curl
instead of the browser. The commands are on this page:
https://github.com/SpeakingColors/JPurrr-8000/blob/main/docs/macos-install.md

Only do this for files from the official release page. The standalone app
and the uninstaller are in the .pkg installer.
