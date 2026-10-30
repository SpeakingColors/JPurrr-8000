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
- Arpeggiator: checked end to end through the plugin (button, strip and MIDI notes, host tempo, MODE, RANGE, HOLD). Fixed: a step could fire three times in a row and skip notes of the pattern; a new chord starts at the beginning of the pattern; the TEMPO knob sets the speed when the host gives no tempo (Standalone).

## Done in 0.2.1
- An Export dialog that asks about the format (JSON text, JP-8080 or JP-8000 SysEx), one sound or the whole bank, the device ID (17 to 32) and the destination (edit buffer or a user slot A11 to B88), and lists the warnings. It replaces the old rule of putting 8000 in the file name.
- Engine fixes from the check against the JP-8000 manual (docs/engine-check-jp8000.md): delay sync past the limits, key follow at -64, mod lever LFO is a triangle, Auto Pan ignores LFO 1 Fade, only the lower part arpeggiates in Split.
- Arpeggiator steps no longer skip notes; the TEMPO knob works in the Standalone app.
- Chopper Cat plays at a normal level across the keyboard.

## Next
- Check the three enum orders (OSC1 waveform, Multi-FX type, Delay type) and the SysEx exports on real hardware.
- Arturia export.
- Engine check against the JP-8000 manual is done (docs/engine-check-jp8000.md). Still open from it: effects per part (the effects section is still one effect for both parts and follows the UPPER sound), arpeggiator beat patterns beyond the first six (the manuals print only their names), velocity behaviour, a listening check of the "not confirmed" rows.
- Voice modulator and external input processing.
- RPS recording and ROM patterns, Motion recording.
- Measure the sound against hardware recordings; tune the chorus, phaser and flanger designs.
- Other formats (CLAP, AU) and a Flatpak.
