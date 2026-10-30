; Inno Setup script for JPurrr-8000 (Windows, 64 bit, per machine, not signed).
; Build on a machine with Inno Setup 6.3 or newer:
;   iscc /DAppVersion=0.2.0 /DSourceVst3=C:\path\JPurrr-8000.vst3 /DSourceExe=C:\path\JPurrr-8000.exe ^
;        /DRepoRoot=C:\path\repo /DOutputDir=C:\path\out installer\windows\JPurrr-8000.iss
; Optional: /DManualPdf=C:\path\JPurrr-8000-manual.pdf
;
; What it installs:
;   VST3 plugin   C:\Program Files\Common Files\VST3\JPurrr-8000.vst3
;   Standalone    C:\Program Files\Studio 87\JPurrr-8000\JPurrr-8000.exe (and the manual)
; Keep AppId and the folders the same in every version: the next setup then upgrades in place.
; Presets and settings live in the user profile and are never removed by the uninstaller.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef RepoRoot
  #define RepoRoot "..\.."
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef SourceVst3
  #error SourceVst3 is not set (path of the built JPurrr-8000.vst3 folder)
#endif
#ifndef SourceExe
  #error SourceExe is not set (path of the built standalone JPurrr-8000.exe)
#endif
; Windows file version needs four numbers; "0.2.0-rc1" becomes "0.2.0.0".
#define NumVersion Copy(AppVersion, 1, Pos("-", AppVersion + "-") - 1) + ".0"

[Setup]
AppId={{6F1A3C52-7B0E-4D8A-9E64-2C5B8A17D3F0}
AppName=JPurrr-8000
AppVersion={#AppVersion}
AppVerName=JPurrr-8000 {#AppVersion}
AppPublisher=Studio 87
AppPublisherURL=https://github.com/SpeakingColors/JPurrr-8000
AppSupportURL=https://github.com/SpeakingColors/JPurrr-8000/issues
AppUpdatesURL=https://github.com/SpeakingColors/JPurrr-8000/releases
VersionInfoVersion={#NumVersion}
VersionInfoDescription=JPurrr-8000 synthesizer setup
DefaultDirName={autopf}\Studio 87\JPurrr-8000
DefaultGroupName=Studio 87
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#RepoRoot}\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=JPurrr-8000-{#AppVersion}-Windows-x64-Setup
SetupIconFile={#RepoRoot}\installer\windows\jpurrr-8000.ico
UninstallDisplayIcon={app}\JPurrr-8000.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
UsePreviousAppDir=yes

[Messages]
SelectDirLabel3=The standalone app and the manual go into this folder. The VST3 plugin always goes into the common VST3 folder (C:\Program Files\Common Files\VST3), where DAWs look for it.
FinishedLabel=JPurrr-8000 is installed. Rescan plugins in your DAW if it does not show JPurrr-8000 yet. Close your DAW before installing an update: it keeps the plugin file locked while it runs.

[Types]
Name: "full"; Description: "Plugin and standalone app"
Name: "plugin"; Description: "Plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Tasks]
Name: "desktopicon"; Description: "Put an icon on the desktop"; Flags: unchecked; Components: standalone

[Components]
Name: "vst3"; Description: "VST3 plugin (for your DAW)"; Types: full plugin custom; Flags: fixed
Name: "standalone"; Description: "Standalone app and manual"; Types: full custom

[Dirs]
; The VST3 folder shows the JPurrr-8000 icon in Explorer: Plugin.ico and desktop.ini sit in it (VST3 spec, Plug-in Format,
; "Bundle"), and the folder itself needs the System attribute for Windows to read desktop.ini.
Name: "{commoncf64}\VST3\JPurrr-8000.vst3"; Attribs: system; Components: vst3

[Files]
Source: "{#SourceVst3}\*"; DestDir: "{commoncf64}\VST3\JPurrr-8000.vst3"; Excludes: "desktop.ini"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#SourceVst3}\desktop.ini"; DestDir: "{commoncf64}\VST3\JPurrr-8000.vst3"; Attribs: hidden system; Components: vst3; Flags: ignoreversion
; The icon for the shortcuts and the uninstall entry. Always installed, so it also works when only the plugin is chosen.
Source: "{#RepoRoot}\installer\windows\jpurrr-8000.ico"; DestDir: "{app}"; DestName: "JPurrr-8000.ico"; Flags: ignoreversion
Source: "{#SourceExe}"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#RepoRoot}\LICENSE"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
#ifdef ManualPdf
Source: "{#ManualPdf}"; DestDir: "{app}"; DestName: "JPurrr-8000-manual.pdf"; Components: standalone; Flags: ignoreversion
#endif

[Icons]
Name: "{group}\JPurrr-8000"; Filename: "{app}\JPurrr-8000.exe"; IconFilename: "{app}\JPurrr-8000.ico"; Components: standalone
Name: "{autodesktop}\JPurrr-8000"; Filename: "{app}\JPurrr-8000.exe"; IconFilename: "{app}\JPurrr-8000.ico"; Tasks: desktopicon; Components: standalone
#ifdef ManualPdf
Name: "{group}\JPurrr-8000 manual"; Filename: "{app}\JPurrr-8000-manual.pdf"; Components: standalone
#endif
Name: "{group}\Uninstall JPurrr-8000"; Filename: "{uninstallexe}"; IconFilename: "{app}\JPurrr-8000.ico"

[Run]
Filename: "{app}\JPurrr-8000.exe"; Description: "Start the standalone app"; Flags: nowait postinstall skipifsilent unchecked; Components: standalone
