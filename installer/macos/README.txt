JPurrr-8000 for macOS (Apple silicon and Intel, one installer)
==============================================================

1. Close your DAW.
2. Open JPurrr-8000-<version>-macOS.pkg and follow the steps.
3. Rescan plugins in your DAW.

The installer is not signed or notarized by Apple (that needs a paid developer
account). macOS will warn you the first time:

  macOS 14 and older: right click the .pkg, choose Open, then Open again.
  macOS 15 and newer: double click the .pkg, close the warning, open
  System Settings, Privacy & Security, scroll down to the message about
  JPurrr-8000 and click "Open Anyway".

If you copy the plugin by hand instead, remove the download mark:
  xattr -dr com.apple.quarantine ~/Downloads/JPurrr-8000.vst3

Where things go
  VST3 plugin   /Library/Audio/Plug-Ins/VST3/JPurrr-8000.vst3
  Standalone    /Applications/JPurrr-8000.app
  Manual and uninstall.command   /Library/Application Support/Studio 87/JPurrr-8000/

Uninstall: run uninstall.command from the folder above (double click it).
Your presets and settings are kept.
