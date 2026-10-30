# Every Studio 87 app, plugin and tool needs an icon on every platform

A program without an icon shows the generic blank icon in Launchpad, the Start menu or the Linux app menu. It looks like something unfinished or unsafe. Do all of this before the first release. JPurrr-8000 is the worked example (replace `jpurrr-8000` / `JPurrr-8000` with the name of the new program).

## 1. One drawing, all the files

1. Draw one square master (1024 px PNG and an SVG): the logo on a rounded tile that reads from 16 px to 1024 px. Look at it at 1024, 256, 64, 32 and 16 px. At 48 px and below use a bolder, simpler version (thicker lines, no fine detail, no thin border).
2. Put the sources in `assets/icon/` of the repo with a short README.
3. Copy `tools/make_icons.py` from the JPurrr-8000 repo and change the drawing function and colours. It needs only Python 3 and Pillow, writes everything below and has `--check` to see whether the committed files are up to date. Commit the generated files: CI must not need to regenerate them.

| File | Sizes | Used by |
| --- | --- | --- |
| `installer/windows/<name>.ico` | 16 24 32 48 64 128 256 (BMP up to 128, PNG at 256) | exe, Setup.exe, shortcuts, VST3 folder |
| `installer/macos/<name>.icns` | 16 to 1024 with @2x (types icp4 icp5 icp6 ic07 to ic14) | .app and .vst3 bundle |
| `installer/linux/icons/hicolor/<s>x<s>/apps/<name>.png` | 16 22 24 32 48 64 128 256 512 | menu entry, window |
| `installer/linux/icons/hicolor/scalable/apps/<name>.svg` | vector | menu entry |
| `assets/icon/<name>-macos-1024.png` | 1024, Apple grid (about 10 percent transparent margin, soft shadow) | JUCE `ICON_BIG` on macOS |

## 2. JUCE plugin (CMake)

In `juce_add_plugin(...)`:

```cmake
ICON_BIG   ${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon/<name>-1024.png      # macOS: the -macos-1024.png
ICON_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon/<name>-256.png       # not on macOS
```

JUCE then does this by itself:

- macOS: writes `Icon.icns` into `Contents/Resources` of the standalone .app and sets `CFBundleIconFile` (it makes the .icns from the PNG with only two sizes).
- Windows: links the icon into the exe and into the VST3 dll, and next to the VST3 binary writes `Plugin.ico` and `desktop.ini` and sets the System attribute on the VST3 folder, as the VST3 spec asks (Plug-in Format, Bundle).
- Linux: nothing. Icon and menu entry come from the installer (section 5). The standalone window icon is set in code: embed the 256 px PNG with `juce_add_binary_data(... HEADER_NAME IconData.h NAMESPACE IconData ...)` and in the editor call `DocumentWindow::setIcon` (Linux only, standalone only, see `JPurrrEditor::parentHierarchyChanged`).

To get the hand tuned .icns and .ico (all sizes) instead of JUCE's own, copy them over the file JUCE generated, right after `juce_add_plugin`:

```cmake
if(APPLE OR WIN32)
  get_target_property(GEN <target> JUCE_ICON_FILE)
  if(GEN)
    configure_file(<tuned .icns or .ico> ${GEN} COPYONLY)
  endif()
endif()
```

## 3. macOS packaging (`scripts/package_macos.sh`)

- Before `codesign` (signing seals Resources): copy the .icns into `Contents/Resources` of the .app and of the .vst3, and make sure `CFBundleIconFile` is in each `Info.plist`:
  `/usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string <name>" <bundle>/Contents/Info.plist`
- Ad hoc `codesign --force --deep --sign -` and `codesign --verify --deep --strict` must still pass.
- The .pkg file itself keeps the standard package icon (a custom file icon on a .pkg is not standard, skipped).
- If an old blank icon stays after a reinstall, macOS has cached it: `touch /Applications/<Name>.app` or restart the Dock (`killall Dock`).
- Check in CI: `scripts/check_icons.sh mac-bundle <bundle>` (icns exists, `CFBundleIconFile` names it, `iconutil -c iconset` can read it). Run it on the built bundles and on the installed ones after `installer -pkg`.

## 4. Windows installer (Inno Setup)

```ini
[Setup]
SetupIconFile={#RepoRoot}\installer\windows\<name>.ico
UninstallDisplayIcon={app}\<Name>.ico          ; a file, not the exe: the exe is optional in a plugin-only install

[Dirs]
Name: "{commoncf64}\VST3\<Name>.vst3"; Attribs: system

[Files]
Source: "{#SourceVst3}\*"; DestDir: "{commoncf64}\VST3\<Name>.vst3"; Excludes: "desktop.ini"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#SourceVst3}\desktop.ini"; DestDir: "{commoncf64}\VST3\<Name>.vst3"; Attribs: hidden system; Flags: ignoreversion
Source: "{#RepoRoot}\installer\windows\<name>.ico"; DestDir: "{app}"; DestName: "<Name>.ico"; Flags: ignoreversion

[Icons]
Name: "{group}\<Name>"; Filename: "{app}\<Name>.exe"; IconFilename: "{app}\<Name>.ico"
```

- `desktop.ini` content: `[.ShellClassInfo]`, `IconResource=Plugin.ico,0`, `IconFile=Plugin.ico`, `IconIndex=0`. `Plugin.ico` and `desktop.ini` sit in the VST3 bundle root, and the bundle folder needs the System (or Read-only) attribute, otherwise Explorer ignores desktop.ini. `scripts/package_windows.sh` creates both if the build did not.
- Check in CI (installed files after a silent install): `scripts/check_icons.ps1 -Files "<exe>;<vst3 dll>;<Setup.exe>" -Vst3Folder <installed .vst3 folder>`. It counts icon resources with `ExtractIconEx` and looks for `Plugin.ico` and `desktop.ini`.

## 5. Linux installer (`install.sh`)

- Install every PNG to `~/.local/share/icons/hicolor/<s>x<s>/apps/<name>.png` and the SVG to `.../hicolor/scalable/apps/<name>.svg` (system install: `/usr/share/icons/hicolor/...`).
- Write `~/.local/share/applications/<name>.desktop` with `Name`, `Comment`, `Exec`, `Icon=<name>` (no path, no extension), `Terminal=false`, `Categories=AudioVideo;Audio;Midi;Music;` and `StartupWMClass=<window class>` (for JUCE the product name).
- Run `gtk-update-icon-cache -q -t <icons>/hicolor` and `update-desktop-database <applications>` only if they exist, ignore their failure. `--uninstall` removes every file again.
- The release tarball must contain the `icons/hicolor` folder next to `install.sh`.
- shellcheck: `shellcheck -s sh installer/linux/install.sh`.

## 6. Web page and repo

- `docs/favicon.png` (64 px) and `docs/apple-touch-icon.png` (180 px), linked in `docs/index.html` with `<link rel="icon">` and `<link rel="apple-touch-icon">`.
- Mention `assets/icon` in the README and the manual.

## 7. Done when

- `python3 tools/make_icons.py --check` says up to date (CI runs it).
- CI is green with the icon checks of section 3 and 4 (the macOS and Windows jobs are the only real test, there is no Mac or Windows machine in the Claude sandbox).
- After a release, a person looks once at: Launchpad and Finder (macOS), Start menu, Explorer VST3 folder and Setup.exe (Windows), app menu and the window title bar (Linux). Mac and Windows icon display is not proven until someone has seen it.
