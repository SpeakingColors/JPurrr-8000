# JPurrr-8000

Nice twist on a trance classic, a nod to the famous JP-8000.

A free and open source VST3 synth and standalone app for Linux, Windows and macOS, in the spirit of the late 90s JP-8000 and JP-8080.

![The JPurrr-8000 Sound page](docs/screenshot.png)

## Status

First test release. The synth engine, the FX chain, the MASTER section, factory sounds (51), the MIDI map and JP-8000/JP-8080 SysEx patch import and export are in. The window is built from the approved design mockup ([docs/mockups/JPurrr-8000-mockup-v3.2.html](docs/mockups/JPurrr-8000-mockup-v3.2.html)). Not done yet: voice modulator, external input, RPS recording, Motion. See [ROADMAP.md](ROADMAP.md) for the full list.

## The idea

- **Sound page:** the JP-8000 front panel experience: Super Saw and the other oscillators, filter, LFOs, amp, arpeggiator, effects, key modes, patch select display.
- **FX page:** our own twist. A mix ready chain (trance gate, drive stack, low end, M/S EQ, transient, tamer, glue, widener, auto pan) and a MASTER page (opto leveller, OTT style squeeze, a four band multiband compressor and limiter, clipper and limiter), so every sound comes out sounding finished. Every block has its own preset menu.
- Live displays that react to the sound going through.
- Presets: load and save our own `.json` format, and import JP-8080 SysEx patches and banks (`.syx`); export JP-8080 and JP-8000 SysEx. MIDI CC and NRPN control is documented in [docs/midi-and-sysex.md](docs/midi-and-sysex.md).

Works 100% offline.

## Install

Download the files from the [latest release](https://github.com/SpeakingColors/JPurrr-8000/releases/latest). Close your DAW first and rescan plugins afterwards. Presets and settings are never removed by an uninstall. Every release has a `SHA256SUMS.txt` to check your download.

**Linux (x86_64):** unpack `JPurrr-8000-<version>-Linux-x86_64.tar.gz` and run `./install.sh`. It installs for your user only, no root needed: the plugin to `~/.vst3`, the standalone app to `~/.local/bin`, plus a menu entry. `./install.sh --dry-run` shows what it would do, `./install.sh --uninstall` removes it again, `sudo ./install.sh --system` installs for all users (not on immutable systems such as Bazzite).

**Windows 10 and 11 (64 bit):** run `JPurrr-8000-<version>-Windows-x64-Setup.exe`. The plugin goes to `C:\Program Files\Common Files\VST3`, the standalone app to `C:\Program Files\Studio 87\JPurrr-8000`. The setup is not signed, so SmartScreen may show "Windows protected your PC": click More info, then Run anyway. Uninstall in Settings, Apps.

**macOS (Apple silicon and Intel):** open `JPurrr-8000-<version>-macOS.pkg`. It is not signed or notarized, so macOS warns the first time. macOS 14 and older: right click the file, Open, Open. macOS 15 and newer: double click it, then System Settings, Privacy & Security, Open Anyway. The plugin goes to `/Library/Audio/Plug-Ins/VST3`, the app to `/Applications`. The uninstaller is `uninstall.command` in `/Library/Application Support/Studio 87/JPurrr-8000`.

**Just copy the plugin:** the VST3 is a folder. Copy `JPurrr-8000.vst3` to `~/.vst3/` (Linux), `C:\Program Files\Common Files\VST3\` (Windows) or `/Library/Audio/Plug-Ins/VST3/` (macOS; run `xattr -dr com.apple.quarantine JPurrr-8000.vst3` on it first).

## Manual

The user manual is [docs/manual/JPurrr-8000-manual.pdf](https://github.com/SpeakingColors/JPurrr-8000/blob/main/docs/manual/JPurrr-8000-manual.pdf) (source: [docs/manual/manual.md](docs/manual/manual.md)). Rebuild it with `python3 scripts/build_manual.py` (needs the Python packages `markdown`, `playwright` and `pypdf`, and `playwright install chromium`). The screenshots in `docs/manual/img/` are taken from the real plugin editor with the `jpurrr_snapshot` tool.

## Build from source

Needs CMake 3.22 or newer and a C++17 compiler. The plugin build fetches JUCE by itself (pinned in `plugin/CMakeLists.txt`).

```sh
# core tests with sanitizers (Linux, macOS)
sh tests/run.sh

# plugin: VST3 and standalone
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJPURRR_BUILD_PLUGIN=ON -DJPURRR_BUILD_TESTS=OFF
cmake --build build --parallel 2
# result: build/plugin/JPurrr8000_artefacts/Release/{VST3,Standalone}
```

On Linux install these packages first (Debian and Ubuntu names): `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev libxext-dev`. On Windows use `-A x64` and `--config Release`. For a universal macOS build add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.

Releases are built by GitHub Actions: pushing a tag like `v0.1.0` (it must match the version in `CMakeLists.txt`) builds all three systems, makes the installers and publishes the release. `scripts/` and `installer/` hold the packaging.

## Licence

AGPL-3.0-or-later. See [LICENSE](LICENSE).

Not affiliated with or endorsed by Roland or Arturia. All names belong to their owners.

## Support

Free to use. If you like it, you can leave a tip on [Ko-fi](https://ko-fi.com/studio87).
