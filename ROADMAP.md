# Roadmap

Rough order. Nothing here is promised for a date.

## Done in 0.1.0
- Native JUCE window built from the approved mockup (all pages, dialogs, tooltips, live displays).
- Sound engine: Super Saw and the other oscillators, filter, envelopes, LFOs, arpeggiator, key modes, 51 factory sounds.
- FX: tone, delay, 13 multi-FX types, the MIX READY chain and the MASTER page.
- Presets: own JSON format, JP-8080 and JP-8000 SysEx import and export.
- MIDI: CC map (JP-8080 factory numbers), RPN, own NRPN scheme (see docs/midi-and-sysex.md).
- Installers for Linux, Windows and macOS, PDF manual.

## Done since 0.1.0
- Dual and Split with a different sound per part: UPPER and LOWER cards (EDIT PARTS on the SOUND page), PANEL SELECT edits one part or both, both sounds are saved with the session.

## Next
- The effects section (bass, treble, multi-FX, delay) is one effect for both parts and follows the UPPER sound.
- Check the SysEx exports and the enum orders (OSC1 waveform, Multi-FX type, Delay type) on real hardware.
- Voice modulator and external input processing.
- RPS recording and ROM patterns, Motion recording.
- Preview keyboard and PREVIEW switch in the standalone.
- Measure the sound against hardware recordings; tune the chorus, phaser and flanger designs.
- Other formats (CLAP, AU) and a Flatpak.
