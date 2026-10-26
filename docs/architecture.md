# Architecture

```
data/params.json      parameter contract, dumped from the mockup (tools/dump_mockup.js)
data/factory.json     factory presets, dumped from the mockup
core/                 pure C++17, no JUCE, tested without a host
  params_gen.*        generated table (tools/gen_params.py): id, type, range, default, option text
  synth.*  fxchain.*  audio engines (synth voices + on-board effects; mix-ready FX chain)
  sysex.* midi_map.*  Roland style SysEx import/export, MIDI CC / NRPN map
  preset_io.*         own preset format (JSON) and bank files
plugin/               JUCE: VST3 + Standalone, native editor ported from the mockup
tests/                core tests (sh tests/run.sh, sanitizers)
tools/                generators and dump scripts
docs/mockups/         the approved mockup, the single source of the layout
```

## Rules

- The mockup is the plugin. The editor is a native JUCE port of the mockup layout. The parameter list window (EditorStub) is a development stand-in and never ships.
- Parameters are integers in the units of `kParams` (same as the mockup). The processor hands the engines one `int[kNumParams]` per block.
- Nothing on the audio thread waits: no locks, no allocation, no file or network access.
- Unknown SysEx is never guessed. Anything unverified is listed in ROADMAP.md.
