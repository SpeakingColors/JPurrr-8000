# MIDI and SysEx in JPurrr-8000

What the plugin can read and write, how sure we are, and what is left out. Short version: **everything in the JP-8080 owner's manual is implemented; the few places where the manual is silent are marked below and need one check on real hardware.**

## What you can do

- **Open** `.json` / `.jpurrr` presets and banks (our own format), `.syx` dumps from a JP-8000 or JP-8080 (one patch, a bank, or a full-memory dump) and `.mid` files that hold such a dump.
- **Save** a preset or a bank as `.json` (lossless, same text the design mockup exports) or as `.syx` (Roland JP-8080 patch data, only what the Roland map carries).
- **Control** the sound from a MIDI keyboard or controller: the factory CC numbers of the JP-8080 knobs, modulation lever, volume, sustain, portamento, pitch bend range (RPN), plus our own NRPN numbers for every other control.

Files from the mockup and files from the plugin are the same text: paste one into the other.

## Status of the SysEx map

**Verified (manual plus a real instrument dump)**

- Framing: `F0 41 <dev> 00 06 12 <addr x4> <data> <sum> F7` (DT1) and `... 11 ... <size x4> ...` (RQ1). Manufacturer 41h, model ID 00 06 (JP-8080 manual pp. 194, 197, 199).
- Device ID 10h to 1Fh (the DEVICE ID setting 17 to 32 minus 1). Default 10h.
- Checksum: `(128 - (sum of address and data bytes mod 128)) mod 128` (p. 206). The three worked examples of the manual are unit tests (checksums 4Fh, 40h, 44h; two example messages in the manual print a size byte as 6Dh and 6Fh, typos: its own sum needs 78h).
- Addresses are 7 bit numbers: `01 78` means 248 bytes (p. 200).
- One patch is 248 bytes (p. 203). User patch memory starts at `02 00 00 00` (A11) and steps by `00 00 02 00` per patch up to `02 01 7E 00` (B88) (p. 204). The temporary patches of the current performance are at `01 00 40 00` (upper) and `01 00 42 00` (lower); a patch inside a user performance is at `03 <perf> 40 00` / `42 00` (pp. 202, 205).
- The instrument sends each patch as two DT1 messages, 242 bytes at the start and the last 6 bytes at +`01 72`. Our exporter does the same and the importer joins any split (a real full-memory dump decodes into all 128 user patches and 128 performance patches).
- Patch name: 16 ASCII characters, 20h to 7Dh, padded with spaces.
- Every sound parameter in the table below, including the JP-8080 bytes `01 6F` to `01 77`: address and range come from the manual (p. 203) and a real JP-8080 bulk dump has those bytes in the same place with values inside the documented ranges. Non-zero values for patch gain and unison detune appear in that dump; the unison switch, envelope type, external trigger, OSC2 external input and voice modulator send are 0 in all 128 patches we could check, so only their address and range are confirmed, not an "on" example.
- Test: 128 patches from a real JP-8080 dump were read and written back. Offsets 0 to 73 (name and all sound parameters) come back byte for byte, except OSC2 fine tune, which has 101 steps on the instrument and 128 on our knob (plus or minus 1). Offsets 239 to 247 come back byte for byte.
- Morph and velocity depth words (offsets 74 to 150, 153, 156 to 234) are written as "no effect" (00 7F); we do not have these controls, so patches that use them on the instrument lose those assignments here. The three "active indicator" bytes (236 to 238) are written as 0 (the manual says the instrument never receives them).

**Approximated (marked in the code, listed as warnings when they apply)**

- **OSC2 WIDE.** The manual says RANGE = -WIDE / +WIDE (range byte 0 / 50) hands FINE/WIDE the job of moving OSC2 over -4..0 or 0..+4 octaves, but gives no step table. We map the fine byte 0..100 linearly onto the four octaves. Our WIDE switch with the range knob stands for octaves in the mockup (1/6 octave per range step).
- **Enumerations the SysEx table abbreviates with "...".** The manual's address table gives only the first and last entries for OSC1 waveform, Multi-FX type and Delay type. We use the order of the panel parameter list (pp. 182 and 183), which has the same first and last entries and the same count. Affected values: OSC1 waveform 2 and 3 (NOISE and FEEDBACK OSC; the Quick Start on p. 25 confirms the button order SUPER SAW, TRI MOD, NOISE, FEEDBACK OSC), Multi-FX types 1 to 11, Delay types 1 to 3. One community tool lists OSC1 values 2 and 3 the other way round; its notes are not from the manual and a real dump could not settle it. **One A/B check on a JP-8080 or JP-8000 settles all of these** (store one patch with FEEDBACK OSC, one with a flanger and one with delay type MONO SHORT, send them, look at the display). If the order differs, change the tables in `core/src/sysex.cpp`.
- **OSC2 waveform** follows the explicit table of the manual (PULSE, TRI, SAW, NOISE), which differs from the panel order (SQR, SAW, TRI, NOISE): SAW and TRI swap. This one is verified by the manual, not assumed.
- The `.json` to `.syx` direction drops everything JPurrr adds (FX page, trance gate, arpeggiator and RPS, key mode, voices, unison on all voices). The `.syx` to `.json` direction leaves those at their defaults.

**Left out on purpose**

- **JP-8000 export.** The JP-8000 manual could not be fetched in this build environment (roland.com is not reachable), so its model ID is not confirmed from a primary source. The JP-8080 manual says the JP-8000 shares this map and that a few parameters exist only on the JP-8000; none of them occur in the patch table. **Import** accepts any dump with model ID 00 06 (JP-8000 dumps of 239 bytes without the JP-8080 bytes load too); **export** is JP-8080 only and returns a clear message for the JP-8000.
- Performances (key mode, split, part detune, voice modulator settings), system settings, motion control data and Tx/Rx settings: read as "not a patch" and skipped (counted in the result). The patches inside user performances are imported.
- Roland NRPN numbers. See below.
- RPN 0,1 master fine tuning (the instrument adds it per part to the master tune; no matching control here).
- Program change and bank select (50h/51h/52h) are the host's job.

## The parameter table (JP-8080 manual p. 203, "4-3. Patch")

Offsets are the manual's address read as a 7 bit number: `01 18` is byte 152. Values are the manual's range; ours are the integers of the preset format. "Bipolar" means the byte is the value plus 64 (-64..+63 as 0..127).

| Our id | Patch address (offset) | Meaning and scaling | Notes |
|---|---|---|---|
| `l1w` | 00 00 00 10 | LFO 1 waveform TRI SAW SQR S/H |  |
| `l1r` | 00 00 00 11 | 0..127 |  |
| `l1f` | 00 00 00 12 | 0..127 |  |
| `m2r` | 00 00 00 13 | LFO 2 rate 0..127 |  |
| `m2s` | 00 00 00 14 | LFO 2 depth select PITCH FILTER AMP |  |
| `ring` | 00 00 00 15 | OFF ON |  |
| `ox` | 00 00 00 16 | X-MOD depth 0..127 |  |
| `ob` | 00 00 00 17 | OSC balance -64..+63 as 0..127 |  |
| `od` | 00 00 00 18 | LFO 1 and env destination OSC1+2 OSC2 X-MOD |  |
| `ol` | 00 00 00 19 | OSC LFO 1 depth -64..+63 as 0..127 |  |
| `m2dp` | 00 00 00 1A | pitch LFO 2 depth -64..+63 as 0..127 |  |
| `pd` | 00 00 00 1B | pitch env depth -64..+63 as 0..127 |  |
| `pa` | 00 00 00 1C | pitch env attack |  |
| `pdd` | 00 00 00 1D | pitch env decay |  |
| `o1w` | 00 00 00 1E | OSC1 waveform SUPER SAW, TRI MOD, NOISE, FEEDBACK OSC, SQR, SAW, TRI (values 2 and 3 assumed from the panel order) | order assumed |
| `o1c1` | 00 00 00 1F | OSC1 control 1 |  |
| `o1c2` | 00 00 00 20 | OSC1 control 2 |  |
| `o2w` | 00 00 00 21 | OSC2 waveform: wire PULSE TRI SAW NOISE, ours SQR SAW TRI NOISE (SAW and TRI swap) |  |
| `sync` | 00 00 00 22 | OFF ON |  |
| `o2r` | 00 00 00 23 | range byte 0 = -WIDE, 1..49 = -24..+24, 50 = +WIDE; fine byte 0..100 = -50..+50 cent (WIDE: octaves, approximated) |  |
| `o2c1` | 00 00 00 25 | OSC2 control 1 |  |
| `o2c2` | 00 00 00 26 | OSC2 control 2 |  |
| `ft` | 00 00 00 27 | filter type HPF BPF LPF |  |
| `f24` | 00 00 00 28 | slope 0 = 12 dB, 1 = 24 dB |  |
| `fc` | 00 00 00 29 | cutoff |  |
| `fr` | 00 00 00 2A | resonance |  |
| `fk` | 00 00 00 2B | key follow -64..+63 as 0..127 |  |
| `fl` | 00 00 00 2C | filter LFO 1 depth -64..+63 as 0..127 |  |
| `m2df` | 00 00 00 2D | filter LFO 2 depth -64..+63 as 0..127 |  |
| `fe` | 00 00 00 2E | filter env depth -64..+63 as 0..127 |  |
| `fea` | 00 00 00 2F | filter env attack |  |
| `fed` | 00 00 00 30 | filter env decay |  |
| `fes` | 00 00 00 31 | filter env sustain |  |
| `fer` | 00 00 00 32 | filter env release |  |
| `av` | 00 00 00 33 | amp level |  |
| `al` | 00 00 00 34 | amp LFO 1 depth (pan position in MANUAL pan) -64..+63 as 0..127 |  |
| `m2da` | 00 00 00 35 | amp LFO 2 depth -64..+63 as 0..127 |  |
| `aea` | 00 00 00 36 | amp env attack |  |
| `aed` | 00 00 00 37 | amp env decay |  |
| `aes` | 00 00 00 38 | amp env sustain |  |
| `aer` | 00 00 00 39 | amp env release |  |
| `ap` | 00 00 00 3A | pan OFF AUTO MANUAL |  |
| `tb` | 00 00 00 3B | tone bass -64..+63 as 0..127 |  |
| `tt` | 00 00 00 3C | tone treble -64..+63 as 0..127 |  |
| `xt` | 00 00 00 3D | multi-FX type 0..12 (middle values assumed from the panel list order) | order assumed |
| `xl` | 00 00 00 3E | multi-FX level |  |
| `dty` | 00 00 00 3F | delay type 0..4 (middle values assumed from the panel list order) | order assumed |
| `dt` | 00 00 00 40 | delay time |  |
| `dfb` | 00 00 00 41 | delay feedback |  |
| `dl` | 00 00 00 42 | delay level |  |
| `bu` | 00 00 00 43 | bend range up 0..24 |  |
| `bd` | 00 00 00 44 | bend range down, byte = magnitude (ours is -24..0) |  |
| `pon` | 00 00 00 45 | portamento OFF ON |  |
| `pt` | 00 00 00 46 | portamento time |  |
| `mono` | 00 00 00 47 | OFF ON |  |
| `leg` | 00 00 00 48 | OFF ON |  |
| `os` | 00 00 00 49 | OSC shift -2..+2 octaves as 0..4 |  |
| `mba` | 00 00 01 18 | morph bend assign OFF ON |  |
| `vel` | 00 00 01 1B | velocity switch OFF ON |  |
| `es` | 00 00 01 6F | envelope type in solo STANDARD ANALOG | JP-8080 only byte |
| `o2ext` | 00 00 01 71 | OSC2 external input OFF ON | JP-8080 only byte |
| `vsend` | 00 00 01 72 | voice modulator send OFF ON | JP-8080 only byte |
| `uni` | 00 00 01 73 | unison OFF ON | JP-8080 only byte |
| `ud` | 00 00 01 74 | unison detune 0..50 cent | JP-8080 only byte |
| `pg` | 00 00 01 75 | patch gain 0, +6, +12 dB | JP-8080 only byte |
| `ets` | 00 00 01 76 | external trigger OFF ON | JP-8080 only byte |
| `etd` | 00 00 01 77 | external trigger destination FILTER AMP FILTER&AMP | JP-8080 only byte |

OSC2 range byte 0 is -WIDE, 1 to 49 is -24 to +24 semitones, 50 is +WIDE. The fine byte 0 to 100 is -50 to +50 cent.

## MIDI control map

All numbers below are from the manual: CC list pp. 192 to 193, factory knob assignments p. 188 (Transmit/Receive Setting List, MODE2 column, which is also the factory setting of the Tx/Rx settings).

| CC | Does |
|---|---|
| 1 | Modulation lever (the LFO 2 depth source) |
| 7 | Plugin master volume. On the instrument CC 7 is the AMP LEVEL knob and the part volume; here AMP LEVEL has no CC so a volume message never fights the patch level |
| 10 | Pan position, only while PAN is MANUAL (the instrument's pan is per part; this is the nearest control we have) |
| 64 | Sustain pedal |
| 65 | Portamento switch |
| 120 | All sound off |
| 121 | Reset all controllers |
| 123, 124, 125 | All notes off |
| 126, 127 | Mono (MONO on) and Poly (MONO off), each with all notes off |

The knobs (factory assignment, p. 188):

| CC | Moves (our id) | Name in the manual |
|---|---|---|
| 4 | `o1c1` | FOOT-TYPE (CC04): OSC 1 control 1 |
| 5 | `pt` | PORTA-TIME (CC05): portamento time |
| 8 | `ob` | BALANCE (CC08): OSC balance |
| 12 | `dt` | EFFECT-CTL1 (CC12): delay time |
| 13 | `dfb` | EFFECT-CTL2 (CC13): delay feedback |
| 16 | `l1r` | GENERAL1 (CC16): LFO 1 rate |
| 17 | `m2r` | GENERAL2 (CC17): LFO 2 rate |
| 18 | `ol` | GENERAL3 (CC18): OSC LFO 1 depth |
| 19 | `fl` | GENERAL4 (CC19): filter LFO 1 depth |
| 20 | `l1f` | CC20: LFO 1 fade |
| 21 | `o2r` | CC21: OSC 2 range |
| 22 | `m2dp` | CC22: LFO 2 depth pitch |
| 23 | `m2df` | CC23: LFO 2 depth filter |
| 24 | `m2da` | CC24: LFO 2 depth amp |
| 25 | `pd` | CC25: pitch envelope depth |
| 26 | `pa` | CC26: pitch envelope attack |
| 27 | `pdd` | CC27: pitch envelope decay |
| 28 | `fes` | CC28: filter envelope sustain |
| 29 | `fer` | CC29: filter envelope release |
| 30 | `fk` | CC30: key follow |
| 31 | `aes` | CC31: amp envelope sustain |
| 70 | `ox` | SOUND-CTL1 (CC70): X-MOD depth |
| 71 | `fr` | SOUND-CTL2 (CC71): resonance |
| 72 | `aer` | SOUND-CTL3 (CC72): amp envelope release |
| 73 | `aea` | SOUND-CTL4 (CC73): amp envelope attack |
| 74 | `fc` | SOUND-CTL5 (CC74): cutoff |
| 75 | `aed` | SOUND-CTL6 (CC75): amp envelope decay |
| 76 | `o1c2` | SOUND-CTL7 (CC76): OSC 1 control 2 |
| 77 | `o2f` | SOUND-CTL8 (CC77): OSC 2 fine / wide |
| 78 | `o2c1` | SOUND-CTL9 (CC78): OSC 2 control 1 |
| 79 | `o2c2` | SOUND-CTL10 (CC79): OSC 2 control 2 |
| 80 | `al` | GENERAL5 (CC80): amp LFO 1 depth |
| 81 | `fe` | GENERAL6 (CC81): filter envelope depth |
| 82 | `fea` | GENERAL7 (CC82): filter envelope attack |
| 83 | `fed` | GENERAL8 (CC83): filter envelope decay |
| 92 | `tb` | TREMOLO (CC92): tone control bass |
| 93 | `xl` | CHORUS (CC93): multi-FX level |
| 94 | `dl` | CELESTE (CC94): delay level |
| 95 | `tt` | PHASER (CC95): tone control treble |

A value from 0 to 127 is scaled over the whole range of the control, so bipolar controls (-64 to +63) and short ones (OSC2 range -24 to +24) both reach their ends.

- **RPN 0,0** (pitch bend sensitivity, 0 to 24 semitones) sets BEND UP and BEND DOWN together; the LSB is ignored, as in the manual (p. 192). **RPN 0,2** (coarse tuning, 28h to 58h) sets part transpose. RPN 127,127 (null) ends data entry.
- **NRPN.** The JP-8080 manual lists no NRPN numbers for its sound parameters (its knobs send CCs or SysEx, set in the Tx/Rx settings), so there is nothing to copy and we did not invent Roland numbers. JPurrr has its own scheme, **not Roland**: NRPN number = parameter index in `data/params.json` order (MSB = index / 128, LSB = index mod 128), momentary buttons excluded. Data entry works with CC 6 alone (7 bit) or with CC 6 and CC 38 (14 bit). Stable for preset format 1.
- Pitch bend messages and aftertouch are not parameters here; the synth handles them (`midiBendToParam` gives the 14 bit bend as the live bend control).

## For the plugin code

```cpp
#include "jpurrr/preset_io.h"   // also pulls in patch.h and sysex.h
#include "jpurrr/midi_map.h"

// open any supported file
LoadResult r = loadPresetFile(bytes, size, fileName);   // ok, patches, slotNames, bankName, message, warnings
// r.message is ready for a dialog; r.warnings may list approximations.

// save
std::string json = savePatchAsJson(patch);                       // .json
std::string bank = saveBankAsJson(patches, "My bank");
SysexExportResult s = savePatchAsSysex(patch, sysexDeviceFromParam(did), slotIndex /*0 = A11*/);
SysexExportResult b = saveBankAsSysex(patches, 0x10);            // s.ok, s.bytes (the .syx file), s.error, s.warnings
std::string fileName = safeFileName(patch.name, "json");

// MIDI input (message thread: midiMapInit() once; audio thread: one MidiMapper per channel, or one for omni)
MidiMapper mapper;
MidiMapResult m = mapper.controlChange(cc, value, currentParamValues);
for (int i = 0; i < m.count; ++i) setParam(m.updates[i].param, m.updates[i].value);
if (m.sustain >= 0) ...; if (m.allNotesOff) ...;
```

- Loading runs on the message thread (it allocates). Sending a bank to hardware needs at least 20 ms between messages (`SysexExportResult::messages` has them one by one).
- A SysEx import gives every patch the type "Lead" and no genres; the FX page parameters stay at their global defaults. `loadPresetFile` does not apply sound-type FX defaults because they live in the mockup; call your type-default function on the loaded patches if you want them.

## Sources

- Roland JP-8080 Owner's Manual (JP-8080_e3, 220 pages): MIDI Implementation pp. 191 to 207, parameter list pp. 182 and 183, Transmit/Receive Setting List p. 188, Quick Start p. 25, OSC2 range and wide pp. 70 to 71. Read from the copy in the `mlazarev/midi` repository (the PDF is Roland's; it is not in this repository).
- A real JP-8080 full-memory bulk dump (`examples/wc_olo_garb_jp8080.syx` in `github.com/mlazarev/midi`, MIT licence, community patches) used locally to check addresses, packet split, ranges and neutral morph values. Not copied into this repository.
- The `SYSEX_STRUCTURE.txt` of the same repository was read and cross-checked. It is partly wrong (it reads the 7 bit addresses as ordinary hex, so its offsets and patch stride are off, and its effect lists do not match the manual); where it disagreed with the manual, the manual won.
- Not found: the JP-8000 owner's manual (roland.com is blocked from the build machine). The owner has the PDFs; if the JP-8000 manual's patch table differs from this one, tell us.
- Tests: `tests/test_io.cpp`, with a hand made patch `tests/data/hand_patch.syx` written by `tests/data/make_hand_patch.py` directly from the manual table (it shares no code with the exporter).
