# Engine check against the Roland JP-8000 owner's manual

What was checked: the sound engine in `core/` and the parameter contract `data/params.json` against the printed JP-8000 Owner's Manual (page numbers below are the printed page numbers). The JP-8080 manual was used only where the JP-8000 manual is silent and the JP-8080 text says the same thing (marked "8080"). Only facts printed in the manuals count. Where the manual prints no number the row says **not confirmed**; nothing was guessed.

Status words:

- **matches**: the core does what the manual prints.
- **fixed**: it did not, it does now (a unit test covers it).
- **not confirmed**: the manual prints no value or rule, so the core's own choice stays and needs a listening check on a real JP-8000.
- **differs by design**: JPurrr-8000 deliberately goes beyond or away from the JP-8000 (JP-8080 features, plugin needs). Not changed.

Count over the 100 rows of the tables below: 57 matches, 6 fixed, 24 not confirmed, 13 differs by design.

## Fixes (6)

| # | What | Manual | Test |
|---|---|---|---|
| 1 | Synced delay time is limited to 1250 ms (mono types) or 625 ms (panning types); a longer time is halved. It used to fold only above 2.4 s. | p.65, p.87 | `soundfx_delay_sync_halves_past_the_maximum` |
| 2 | KEY FOLLOW negative side: -64 is -100 percent (it was -200). +32 = +100, +64 = +200 stay. | p.61 | `synth_manual_key_follow_scale` |
| 3 | The modulation lever (LFO 2) is a triangle wave (it was a sine). | p.73 | `synth_manual_lfo2_is_a_triangle` |
| 4 | LFO 1 FADE has no effect on Auto Pan (it faded the panning in). | p.62 | `synth_manual_auto_pan_ignores_lfo1_fade` |
| 5 | In SPLIT only the Lower part is arpeggiated (ARP DEST is for DUAL only; it also applied in SPLIT). | p.75, p.76, p.114 | `synth_manual_split_arpeggiates_only_the_lower_part` |
| 6 | Standard settings of the individual trigger: destination FILTER (was FILTER & AMP) and source channel 15 (was 1). Individual trigger itself is not in the engine yet, so nothing audible changes. | p.92 | `params_individual_trigger_standard_settings` |

Only fix 6 touches `data/params.json` (two default values, no id, range, label, tip or option list changed, so the mockup tooltips are unchanged). The mockup is the source of `params.json` (`tools/dump_mockup.js`): a new dump from the mockup would bring the two old defaults back, so change them in the mockup too when it is next edited. Old saves are not affected: stored values are explicit, and fixes 2 to 5 change only how a stored value sounds.

## Delay (p.65, p.86 to 87, p.113 to 114)

| Area | Parameter | Manual says (page) | Core does | Status |
|---|---|---|---|---|
| Delay | Types | 5: PANNING L>R, R>L, SHORT, MONO SHORT, MONO LONG (p.65, p.113) | Same five, same order (`dty`) | matches |
| Delay | Time at knob fully right | MONO LONG 1250 ms, PANNING L>R and R>L 625 ms, PANNING SHORT and MONO SHORT 156 ms (p.65) | 1250 / 625 / 156 ms (`SoundFx::delayTimeMs`) | matches |
| Delay | SHORT types | PANNING SHORT = 1/4 of L>R (625/4 = 156.25), MONO SHORT same; MONO LONG = twice the panning time (p.65) | 156 ms, 1250 ms | matches |
| Delay | TIME, FEEDBACK, LEVEL ranges | 0 to 127 each (p.113) | 0 to 127 each | matches |
| Delay | TIME curve and minimum | "longer to the right" only (p.65) | linear in ms, 1 ms floor | not confirmed |
| Delay | FEEDBACK amount law | "more feedback, longer tail" only (p.65) | 0 to 0.88 loop gain, damped repeats | not confirmed |
| Delay | Panning behaviour | echoes go left/right in the printed order; SHORT types are as PANNING SHORT (p.65) | ping pong pair for the three panning types, one centre line for the mono types | matches |
| Delay | Delay Sync values | OFF, 1/16, 1/8 (3), 1/16 (.), 1/8, 1/4 (3), 1/8 (.), 1/4, 1/2 (3), 1/4 (.), 1/2 (p.86, p.114) | 11 values, same order (`dsy`); beat lengths match the note names | matches |
| Delay | Sync longer than the maximum | mono delay at most 1250 ms, panning delay at most 625 ms; longer: halved. Printed tempi below which a mono delay halves: 1/8 = 24, 1/8 (.) = 36, 1/4 (3) = 32, 1/4 = 48, 1/4 (.) = 72, 1/2 (3) = 64, 1/2 = 96 (p.87) | `SoundFx::delaySyncMs`: halves (again if still too long) at those tempi; panning types use 625 | fixed (1) |
| Delay | Sync on the SHORT types | not printed whether the 1/4 factor of the SHORT types also applies under sync | SHORT types use the same limits as their group (panning 625, mono 1250) | not confirmed |
| Delay | Sync without MIDI clock | MIDI Sync on but no clock: as if the tempo were 20 (p.87) | the plugin follows the host tempo, or the TEMPO knob when there is none; floor 20 | differs by design |
| Delay | Panning delay in mono or PARALLEL OUT | panning effect cannot be obtained (p.65) | the delay is not changed in PARALLEL OUT | not confirmed (what the unit does instead is not printed) |
| Effects | Order | TONE CONTROL, DELAY, CHORUS, PAN (p.65 diagram) | tone, delay, multi-FX; pan is applied per voice before them | differs by design (pan sits before the effects) |
| Sync | Measure length | "M= 1 B= 4" in the display (p.78) | 4 beats | matches |

## Chorus, flanger, phaser (multi-FX) (p.64, p.86, p.113 to 114)

| Area | Parameter | Manual says (page) | Core does | Status |
|---|---|---|---|---|
| Multi-FX | Types | 12: SUPER CHORUS SLW, MID, FST, CLR, FLANGER SLOW, DEEP, FAST, DEEP PHASING SLW, JET PHASING, TWISTING, FREEZE PHASE 1, 2 (p.64) | the same 12 in the same order (`xt` 0 to 11) | matches |
| Multi-FX | Thirteenth type | none | DISTORTION (JP-8080) | differs by design |
| Multi-FX | LEVEL | 0 to 127 (p.113) | 0 to 127 (`xl`) | matches |
| Multi-FX | Character per type | SLW slow and deep, MID medium, FST fast and shallow, CLR transparent, FLANGER DEEP deep, FAST fast, JET like a jet, FREEZE a short delay with metallic resonance (p.64) | rate and depth ordered the same way (0.3, 0.7, 1.6 Hz for the three chorus speeds, and so on) | matches (qualitative) |
| Multi-FX | Rate and depth numbers | no numbers printed | own table (`soundfx_multi.cpp`) | not confirmed |
| Multi-FX | Chorus Sync values | OFF, 1/16 up to 2/1 (16 note values), 3 to 8 MEASURES, LFO 1 (24 values, p.86, p.114) | 24 values, same order (`csy`); 3 to 8 measures = 12 to 32 beats | matches |
| Multi-FX | Sync meaning | rate follows the tempo, "applied at intervals of the specified note value"; LFO 1: rate follows LFO 1 (p.86) | one modulation cycle per note value; LFO 1 option follows the LFO 1 rate | matches (one cycle per interval is the reading used) |
| Multi-FX | Tone control | BASS and TREBLE -64 to +63, centre = flat (p.64, p.113) | -64 to +63, 0 = exact bypass, plus or minus 12 dB | matches (range), dB and corner frequencies not confirmed |

## LFO 1 and LFO 2 (p.63, p.73, p.87)

| Area | Parameter | Manual says (page) | Core does | Status |
|---|---|---|---|---|
| LFO 1 | Waveforms | TRI, SAW, SQR, S/H (p.63) | same list (`l1w`) | matches |
| LFO 1 | SAW direction | falls to its minimum, jumps back to the maximum; negative depth inverts (p.63) | falling saw, negative depth inverts | matches |
| LFO 1 | RATE, FADE | 0 to 127 (p.113) | 0 to 127 | matches |
| LFO 1 | Rate in Hz | none printed | 0.05 to 50 Hz, exponential | not confirmed |
| LFO 1 | Fade time and curve | from key press until full depth; longer to the right (p.63) | 0 to 8 s, quadratic, restarts with each note | not confirmed (the time values); the behaviour matches |
| LFO 1 | Fade and Auto Pan | no effect on Auto Pan (p.62) | none now | fixed (4) |
| LFO 1 | Phase restart on key | not printed | restarts at each note | not confirmed |
| LFO 1 | Sync values | OFF, 16 note values, 3 to 8 MEASURES (23 values, p.87, p.114) | 23 values (`lsy`) | matches |
| LFO 1 | Depth knobs | OSC, FILTER, AMP: -64 to +63, centre = none, left inverts (p.58, p.61, p.62) | same | matches |
| LFO 1 | Size of the effect at full depth | none printed | own (for example plus or minus 12 semitones on pitch) | not confirmed |
| LFO 1 | Pan modes | OFF = level, AUTO PAN, MANUAL PAN; MANUAL: right of centre pans right (p.62) | same | matches |
| LFO 2 | Waveform | triangle (p.73) | triangle | fixed (3) |
| LFO 2 | Depths | PITCH, FILTER, AMP, each -64 to +63, one common RATE 0 to 127, only acts while the lever is pushed (p.73, p.74, p.113) | same | matches |
| LFO 2 | Rate in Hz and depth size | none printed | own | not confirmed |
| Lever | Modulation | CC 1 (p.117) | CC 1 moves the lever | matches |
| Lever | Aftertouch | the factory setting sends channel pressure to RIBBON UP, not to the lever (p.118) | channel pressure also moves the lever | differs by design |

## Envelopes (p.59, p.61, p.62)

| Area | Parameter | Manual says (page) | Core does | Status |
|---|---|---|---|---|
| Envelopes | Pitch env A, D | 0 to 127 (p.112) | 0 to 127 (`pa`, `pdd`) | matches |
| Envelopes | Filter and amp env A, D, S, R | 0 to 127 (p.112) | 0 to 127 | matches |
| Envelopes | Times in seconds | none printed | A 2 ms to 3 s, D 10 ms to 4 s, R 10 ms to 5 s | not confirmed |
| Envelopes | Depth knobs | pitch, filter: -64 to +63, centre = no effect, left inverts (p.59, p.61) | same | matches |
| Envelopes | Click on fast attack or release | allowed, "useful for an organ click" (p.110) | 2 ms and 10 ms floors plus a short declick | not confirmed (how short the unit goes) |

## Portamento, mono, bend range, shifts (p.70 to 72, p.113)

| Area | Parameter | Manual says (page) | Core does | Status |
|---|---|---|---|---|
| Portamento | Switch and TIME | OFF/ON, 0 to 127; longer to the right (p.70, p.71) | same | matches |
| Portamento | Time law | none printed | 0 to 2.5 s, quadratic | not confirmed |
| Portamento | In poly mode | "effective when playing in Mono" (p.70) | glides in poly too | not confirmed |
| Mono, Legato | Behaviour | MONO: every note has an attack; LEGATO: no new attack when notes overlap (p.70) | same | matches |
| Mono, Legato | FEEDBACK OSC | only one note at a time (p.56) | forced mono | matches |
| Bend range | UP and DOWN | 0 to 24 semitones each, 2 octaves at most; standard settings -2 / +2 (p.72, p.92, p.113) | `bu` 0 to 24, `bd` -24 to 0 (SysEx stores the magnitude), defaults +2 / -2 | matches |
| Shifts | OSC SHIFT | -2 to +2 octaves per patch (p.71) | -2 to +2 (`os`) | matches |
| Shifts | KEYBOARD SHIFT | -2 to +2 octaves, common; changes the keyboard pitch, not notes received at MIDI IN (p.71) | -2 to +2 (`ksh`), applies to every note | differs by design (the plugin has no hardware keyboard) |
| Transpose | PART TRANSPOSE | -24 to +24 semitones per part (p.72, p.86) | -24 to +24 (`ptr`) | matches |
| Velocity | Behaviour | no effect unless VELOCITY is on and parameters are assigned (p.68, p.69) | loudness always follows velocity (exponent 0.6); the VELOCITY switch only adds a cutoff change | differs by design (needs your decision, see below) |
| Velocity, Ribbon | Assign depths | -127 to +127, -100 to +100 or -50 to +50 per parameter (p.113) | not in the core (the ribbon is handled in the editor) | not confirmed |

## Filter (p.59 to 61, p.103, p.112)

| Area | Parameter | Manual says (page) | Core does | Status |
|---|---|---|---|---|
| Filter | Types | HPF, BPF, LPF (p.59, p.112) | same list (`ft`) | matches |
| Filter | Slopes | -12 dB/oct and -24 dB/oct (p.60) | 12 dB state variable, 24 dB ladder | matches |
| Filter | CUTOFF, RESONANCE | 0 to 127; raised all the way the filter oscillates and gives a sine (p.56, p.60) | 0 to 127, resonance rings on its own at the top | matches (24 dB), the 12 dB slope is not confirmed |
| Filter | Cutoff steps | with KEY FOLLOW +32 one cutoff step is one semitone of the oscillation pitch (p.103) | 20 Hz to 20 kHz over 127 steps = 0.94 semitone per step | not confirmed (the base frequency is not printed; changing the curve would shift every sound) |
| Filter | KEY FOLLOW range | -64 to +63 (p.112) | -64 to +63 | matches |
| Filter | KEY FOLLOW scale | +64 = +200 percent, +32 = +100 percent, 0 = fixed, -64 = -100 percent (p.61 graph) | now exactly that | fixed (2) |
| Filter | ENV DEPTH, LFO DEPTH | -64 to +63, centre = none, left inverts (p.61) | same | matches |
| Filter | ENV and cutoff interplay | a positive envelope shows more with a low cutoff (p.61) | the envelope adds octaves to the cutoff | matches |

## Arpeggiator (p.75 to 77, p.86, p.114)

| Area | Parameter | Manual says (page) | Core does | Status |
|---|---|---|---|---|
| Arp | TEMPO | 20 to 250 (p.114) | `atp` 20 to 250; a host tempo is used as it comes | matches |
| Arp | MODE | UP, DOWN, UP&DOWN, RANDOM, RPS (p.75) | same list (`amd`) | matches |
| Arp | RPS | plays recorded patterns (p.76) | plays UP (no pattern memory yet) | not confirmed (feature not built, see ROADMAP) |
| Arp | RANGE | 1 to 4 octaves (p.75) | same | matches |
| Arp | HOLD | keeps playing after release; a new chord replaces it (p.75) | same | matches |
| Arp | UP&DOWN end notes | not printed whether the top and bottom note repeat | no repeat | not confirmed |
| Arp | ARP DEST | LOWER&UPPER, LOWER, UPPER, only in DUAL (p.76, p.114) | DUAL only | matches |
| Arp | Parts in SPLIT | only the Lower part (p.75) | only the Lower part | fixed (5) |
| Arp | Beat pattern list | 90 names: 1/4, 1/6, 1/8, 1/12, 1/16, 1/32, PORTA-A1 to 11, PORTA-B1 to 15, SEQUENCE-A1 to 7, B1 to 5, C1 to 2, D1 to 8, ECHO1 to 3, MUTE1 to 16, STRUMMING1 to 8, REFRAIN1 to 2, PERCUSSION1 to 4, WALKING BASS, HARP, RANDOM (p.76, p.86, p.114) | the same 90 names in the same order (`apt`), default 1/16 as on p.92 | matches |
| Arp | Step lengths of the first six | the names only | 1 beat, 2/3, 1/2, 1/3, 1/4, 1/8 | not confirmed (read from the names) |
| Arp | Step lengths, accents and shuffle of the other 84 | "the accent locations and note length will change" (p.76); no step data in either manual | played as 1/16 | not confirmed (nothing printed to implement; left as is) |
| Arp | Gate length | not printed | 60 percent of the step | not confirmed |

## Voices, parts, oscillators (p.55 to 58, p.85, p.92, p.110)

| Area | Parameter | Manual says (page) | Core does | Status |
|---|---|---|---|---|
| Voices | Polyphony | 8 notes (p.110) | 10 by default, `vc` 8 to 128 | differs by design (JP-8080 has 10) |
| Voices | VOICE ASSIGN | 6-2, 5-3, 4-4, 3-5, 2-6, standard 4-4 (p.85, p.114, p.92) | JP-8080 list 8-2 to 2-8, default 5-5 (`vas`) | differs by design |
| Parts | PART DETUNE | -50 to +50; the pitch difference between Upper and Lower, + raises Upper and lowers Lower (p.85) | -50 to +50, each part moves half of it in opposite directions | matches (this reading) |
| Parts | SPLIT POINT | C-1 to G9, standard C4 (p.85, p.92) | 0 to 127, default 60 = C4 (C4 = note 60, p.107) | matches |
| Parts | OUTPUT ASSIGN | MIX OUT, PARALLEL OUT: Lower left, Upper right (p.85) | same | matches |
| Parts | PART MIDI CH | 1 to 16, OFF (p.86) | 0 = OFF, 1 to 16 (`pch`) | matches |
| Parts | MASTER TUNE | 427.5 to 452.9 Hz (p.90) | `mtu` 4275 to 4529 in 0.1 Hz, default 440.0 | matches |
| OSC 1 | Waveforms and CTRL 1 / CTRL 2 | SUPER SAW (DETUNE, MIX), TRIANGLE MOD (OFFSET, LFO 1 DEPTH), NOISE (CUTOFF FREQ, RESONANCE), FEEDBACK OSC (HARMONICS, FEEDBACK AMOUNT), SQR (PW, PWM DEPTH), SAW and TRI (SHAPE, LFO 1 DEPTH) (p.55, p.56, p.112) | 7 waveforms in this order; each control does what its label says | matches |
| OSC 1 | Super Saw numbers | none printed (only "more detune, more spread; more mix, louder side saws") | Szabo 2010 measurements | not confirmed |
| OSC 1 | X-MOD with SUPER SAW or NOISE | no effect (p.58) | no effect | matches |
| OSC 2 | Waveforms | SQR (PWM), SAW, TRI (p.57, p.112) | the same plus NOISE (JP-8080) | differs by design |
| OSC 2 | RANGE and FINE | -24 to +24 semitones; FINE plus or minus 50 cent; at -WIDE / +WIDE the FINE/WIDE knob moves -4 to 0 or 0 to +4 octaves (p.57) | `o2r` -24 to +24, `o2f` +-50 cent over 128 steps (the unit has 101), WIDE is a separate switch in whole octaves | differs by design (the WIDE steps are not printed) |
| OSC | OSC BALANCE | -64 (OSC 1) to +63 (OSC 2), centre = equal (p.58) | same | matches |
| OSC | LFO 1 & ENV DESTINATION | OSC 1+2, OSC 2, X-MOD DEPTH (p.58) | same | matches |
| OSC | RING, SYNC | OFF/ON (p.57, p.58) | same | matches |
| Unison | UNISON DETUNE | 0 to 50 cent between the highest and lowest voice (8080 manual) | 0 to 50, spread across the stack as printed | matches |
| Patch | PATCH GAIN | 0, +6, +12 dB (8080 manual) | x1, x2, x4 | matches |

## Parameter ranges against the MIDI implementation (p.112 to 124) and standard settings (p.92)

| Area | Parameter | Manual says (page) | Core does | Status |
|---|---|---|---|---|
| Patch | All 0 to 127 knobs, bipolar -64 to +63 knobs, switches | p.112, p.123 | same ranges in `params.json` for every parameter the JP-8000 has | matches |
| Patch | OSC2 FINE/WIDE | 0 to 64h = -50 to +50 cent (101 steps) (p.123) | 128 steps, see above | differs by design |
| System | Device ID, LCD contrast, pattern quantize, motion restart | 17 to 32, 1 to 8, OFF/BEAT/MEASURE, OFF/ON (p.115) | same ranges (`did`, `lcc`, `ptq`, `mrs`) | matches |
| System | Tx/Rx ProgChg SW | p.115 lists "ON, PC, BANK SEL + PC"; p.90 and the address map (p.121) list OFF, PC, BANK SEL + PC | first value is labelled ON | not confirmed (the manual contradicts itself; left as is) |
| System | Remote Ctrl CH | 1 to 16, ALL, OFF (p.115) | ALL, 1 to 16 (`rkc`, JP-8080 style, no OFF) | differs by design |
| Standard | Individual trigger | OFF, FILTER, source channel 15, source note ALL (p.92) | now the same | fixed (6) |
| Standard | Split point, arp beat pattern, bend range, arp dest, output assign | C4, 1/16, -2/+2, LOWER&UPPER, MIX OUT (p.92) | the same | matches |
| Standard | Part MIDI CH (Lower 2, Upper 1), pedal EXPRESSION, voice assign 4-4 | p.92 | one value per patch in the core; voice assign see above | differs by design |
| Standard | Patch knob and slider positions of PATCH: TEMP | shown as a drawing only (p.92) | `params.json` defaults come from the mockup | not confirmed (no numbers printed) |

## Needs your decision or a hardware check

1. **Velocity (p.68, p.69).** On the JP-8000 playing dynamics change nothing unless VELOCITY is on and parameters are assigned. The core always makes a harder note louder. Switching that off would make every MIDI keyboard play at one loudness, so it was left; tell me if you want the strict behaviour.
2. **Keyboard Shift (p.71)** on the unit does not shift notes arriving at MIDI IN; in the plugin it shifts everything. Left as is.
3. **Cutoff step (p.103):** one step is a semitone; the core has 0.94. The oscillating-filter trick on p.103 (a fifth with cutoff 71 against 64) comes out about 40 cent flat. A fix needs one measured point of the cutoff curve (the manual gives none).
4. A listening check on a real JP-8000 settles every row marked "not confirmed" with a musical number: LFO rates, fade, envelope and portamento times, chorus rates, delay feedback, arpeggiator gate.
5. **Arpeggiator beat patterns:** neither manual prints the step lists, so patterns 7 to 90 can only be added from a recording of the unit (or a published pattern table).
