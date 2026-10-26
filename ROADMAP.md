# Roadmap

Rough order. Nothing here is promised for a date.

## 1. Window
- Port the approved mockup natively in JUCE: same layout, controls, displays, colours and tooltips.
- Live displays fed by the real audio (the mockup already shows how they should move).

## 2. Sound engine
- Super Saw oscillator and the other waveforms.
- Real unison with a measured spread law, filter, envelopes, LFOs, arpeggiator, key modes (single, dual, split).
- Dual: two patches layered, one per part. Split: a split point, each part with its own patch. Part selection through the PART and PANEL SELECT controls.

## 3. FX
- Port the MIX READY chain and the MASTER page processing.
- Oversampling on the drive and clipper stages.
- Per sound type defaults (lead, pad, bass, pluck, chord, FX), tuned by ear and by measurement.
- Trance gate: depth in dB, decay curve and swell between steps (in the mockup already).

## 4. Patches and banks
- Import and export of JP-8000 and JP-8080 patches and sound banks over SysEx, from the documented parameter address map and checksum rule.
- Check against real files and hardware before release; unknown parts of the format are left out until verified.
- Our own preset format (JSON text) stays as the plain exchange route.

## 5. Release
- Windows installer, macOS package, Linux Flatpak and plain VST3.
- PDF manual with a visual tour of the interface.
