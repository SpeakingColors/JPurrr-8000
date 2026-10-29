# Roadmap

Rough order. Nothing here is promised for a date.

## Done in 0.1.0
- Native JUCE window built from the approved mockup (all pages, dialogs, tooltips, live displays).
- Sound engine: Super Saw and the other oscillators, filter, envelopes, LFOs, arpeggiator, key modes, 51 factory sounds.
- FX: tone, delay, 13 multi-FX types, the MIX READY chain and the MASTER page.
- Presets: own JSON format, JP-8080 and JP-8000 SysEx import and export.
- MIDI: CC map (JP-8080 factory numbers), RPN, own NRPN scheme (see docs/midi-and-sysex.md).
- Installers for Linux, Windows and macOS, PDF manual.

## Done in 0.2.0
- Dual and Split with a different sound per part: UPPER and LOWER cards (EDIT PARTS on the SOUND page), PANEL SELECT edits one part or both, both sounds are saved with the session.
- Keyboard strip with BEND and MOD wheels (mouse and computer keys), PREVIEW switch, split marker and part tints on the keys.
- Your own FX block presets: SAVE, rename and delete on every block, listed under YOUR PRESETS.
- A window height for each page (with or without the keyboard strip and the PARTS band).
- The arpeggiator is checked end to end through the plugin (button, strip and MIDI notes, host tempo, MODE, RANGE, HOLD); the TEMPO knob sets the speed when the host gives no tempo (Standalone).

## Next
- Check the three enum orders (OSC1 waveform, Multi-FX type, Delay type) and the SysEx exports on real hardware.
- An export dialog that asks about the format, the device ID and the memory slot; Arturia export.
- Check the engine against the JP-8000 manual: delay times, effects per part (the effects section is still one effect for both parts and follows the UPPER sound), arpeggiator beat patterns beyond the first six.
- Voice modulator and external input processing.
- RPS recording and ROM patterns, Motion recording.
- Measure the sound against hardware recordings; tune the chorus, phaser and flanger designs.
- Other formats (CLAP, AU) and a Flatpak.
