JPurrr-8000 for Windows
=======================

1. Close your DAW.
2. Double click JPurrr-8000-<version>-Windows-x64-Setup.exe.
3. Rescan plugins in your DAW.

Windows SmartScreen may say "Windows protected your PC" because the setup is not
signed (a code signing certificate costs money and this project is free).
Click "More info", then "Run anyway". You can check the file first against
SHA256SUMS.txt from the release page (PowerShell: Get-FileHash <file>).

Where things go
  VST3 plugin   C:\Program Files\Common Files\VST3\JPurrr-8000.vst3
  Standalone    C:\Program Files\Studio 87\JPurrr-8000\
Uninstall: Settings, Apps, JPurrr-8000. Your presets and settings are kept.

Just copy it yourself
  Copy the JPurrr-8000.vst3 folder to C:\Program Files\Common Files\VST3\ .
