<!--
Source of the JPurrr-8000 user manual. Build the PDF with: python3 scripts/build_manual.py
Image slots: ![caption](img/<name>.png). The script uses docs/manual/img/<name>.png when it exists
and falls back to the design mockup (docs/screenshot.png) when it does not.
Items marked (planned) are not in the plugin yet. Before a release, check each (planned) and each
line in "Known limits" against the real build and remove or keep the mark.
-->

# What JPurrr-8000 is

JPurrr-8000 is a free and open source synthesizer plugin (VST3) and standalone app for Linux, Windows and macOS. It follows the sound and the front panel of the late 90s JP-8000 and JP-8080: Super Saw oscillator, filter, LFOs, amp, arpeggiator, effects and the patch select display. On top of that it has its own FX page, a mix ready processing chain, so a sound comes out ready for a track.

- Works 100% offline. No account, no update check, nothing is locked.
- Stereo output, MIDI input. Use it as an instrument in your DAW.
- Free software under AGPL-3.0-or-later.

JPurrr-8000 is not a copy of a Roland product. It contains no Roland code, ROM or samples. It is not affiliated with or endorsed by Roland or Arturia. All names belong to their owners.

![The JPurrr-8000 window, Sound page](img/sound.png)

# Install

Close your DAW before you install, then rescan plugins afterwards. Presets and settings are kept when you uninstall.

## Linux

1. Download `JPurrr-8000-<version>-Linux-x86_64.tar.gz` from the releases page and unpack it.
2. Open a terminal in the unpacked folder and run `./install.sh`.

This installs for your user only, without root: the plugin goes to `~/.vst3`, the standalone app to `~/.local/bin`, and a menu entry is added. Run `./install.sh --dry-run` first to see what it would do. `./install.sh --uninstall` removes it again. `sudo ./install.sh --system` installs for all users (not possible on immutable systems such as Bazzite, where the user install is the right choice).

Just copy it yourself: put the folder `JPurrr-8000.vst3` into `~/.vst3/`.

## Windows

1. Download `JPurrr-8000-<version>-Windows-x64-Setup.exe` and run it.
2. If SmartScreen says "Windows protected your PC", click More info, then Run anyway. The setup is not signed, because a signing certificate costs money and this project is free. You can compare the file with `SHA256SUMS.txt` from the releases page.

The plugin goes to `C:\Program Files\Common Files\VST3`, the standalone app and this manual to `C:\Program Files\Studio 87\JPurrr-8000`. Uninstall in Settings, Apps. Installing a newer version over the old one works; close your DAW first because it locks the plugin file.

Just copy it yourself: put the folder `JPurrr-8000.vst3` into `C:\Program Files\Common Files\VST3`.

## macOS

1. Download `JPurrr-8000-<version>-macOS.pkg` (Apple silicon and Intel in one file) and open it.
2. The package is not signed or notarized by Apple, so macOS warns you the first time. On macOS 14 and older: right click the file, choose Open, then Open again. On macOS 15 and newer: double click it, close the warning, open System Settings, Privacy & Security, and click Open Anyway.

The plugin goes to `/Library/Audio/Plug-Ins/VST3`, the app to `/Applications`, and the manual and `uninstall.command` to `/Library/Application Support/Studio 87/JPurrr-8000`. If you copy the plugin by hand, run `xattr -dr com.apple.quarantine JPurrr-8000.vst3` on it first.

# First sound

1. Add JPurrr-8000 as an instrument on a MIDI track in your DAW (or start the standalone app and choose your audio and MIDI device in its options).
2. Play a note.
3. Click the preset name at the top to open the preset browser. The START HERE list has a few good first sounds. The arrows next to the name step through presets.
4. Turn the knobs. Hover a control for one second to see what it does.
5. Open the FX tab. MIX READY is on by default, so the sound already runs through the chain. Pick a SOUND TYPE (LEAD, PAD, BASS, PLUCK, CHORD or FX) and press APPLY TYPE to set every module for that kind of sound.

# The window

The top bar has the preset name with the arrows and the preset menu, the SOUND and FX tabs, and the buttons Guide, Settings and About & Support. The wooden side cheeks frame the panel.

- **Knobs and sliders:** drag up and down. Hold Shift for fine steps. The mouse wheel and the arrow keys work too. Double click (or press Enter) to reset to the default.
- **Bubbles:** hover a control for about one second to see its help bubble. Turn them off in Settings.
- **Size:** drag any edge or corner. The shape stays the same and everything scales. Double click a wooden cheek to return to 100%.
- **Value display:** while you turn a control, its name and value show in the patch display.

# The Sound page

The Sound page is the front panel. Values are whole numbers as on the original; many controls show their real unit (cent, Hz, ms) while you turn them.

## Display and patch select

The two line display shows the current sound. BANK buttons 1 to 8 and NUMBER buttons 1 to 8 pick a sound in the current group. GROUP DOWN and GROUP UP change the group (64 sounds each), DEC and INC step one sound. PERFORM / PATCH switches the display view. WRITE opens Save. EDIT then a BANK button opens a parameter page (see "Parameter pages"). EXIT closes it. SHIFT is the second function key.

## Oscillators

| Control | What it does |
|---|---|
| OSC 1 WAVEFORM | SUPER SAW (the 7 saw stack), TRIANGLE MOD, NOISE, FEEDBACK OSC, SQUARE (PWM), SAW, TRIANGLE. |
| OSC 1 CTRL 1, CTRL 2 | Depend on the waveform. For SUPER SAW: CTRL 1 spreads the six outer saws away from the centre, CTRL 2 sets how loud they are against the centre saw. |
| OSC 2 WAVEFORM | SQR (PWM), SAW, TRI, NOISE. RANGE sets the pitch against OSC 1 (-24 to +24 semitones), FINE tunes in cent. WIDE moves RANGE and FINE in whole octaves. PULSE WIDTH and PWM DEPTH are for SQR. |
| SYNC | Hard sync: OSC 2 restarts with OSC 1. |
| EXT | Use the external input as the OSC 2 source (see Known limits). |
| RING | Ring modulator: OSC 1 times OSC 2, for metallic and bell sounds. |
| OSC BALANCE | Mix between OSC 1 (left) and OSC 2 (right). |
| X-MOD DEPTH | OSC 2 modulates the pitch of OSC 1. Not with SUPER SAW or NOISE. |
| LFO1 DEPTH, LFO 1 + ENV DEST | How much LFO 1 moves the oscillators, and where: both, OSC 2 only, or the cross modulation depth. |
| PITCH ENV A, D, ENV DEPTH | Pitch envelope. A negative depth starts high and falls. |
| OSC SHIFT, KEYBOARD SHIFT | Move the oscillators or the keyboard by whole octaves with the -OCT and +OCT buttons. |

FEEDBACK OSC is mono only: it forces MONO or LEGATO and cannot be used with UNISON.

## Filter

TYPE: HPF, BPF or LPF. The 24 dB button switches between 12 and 24 dB per octave. CUTOFF FREQ, RESONANCE (at the top the filter rings and gives a sine tone) and KEY FOLLOW (+32 keeps the brightness equal over the keyboard). LFO1 DEPTH and ENV DEPTH move the cutoff; a negative ENV DEPTH inverts the envelope. The filter envelope has A, D, S and R.

## Amp

LEVEL of the patch, the amp envelope (A, D, S, R), and LFO1 DEPTH for tremolo. PAN: OFF is centre, AUTO sweeps with LFO 1, MANUAL uses the LFO1 DEPTH knob as the position.

## LFO 1 and LFO 2

LFO 1: WAVEFORM (TRI, SAW, SQR, S/H), RATE, FADE (time to reach full depth after a key press) and TEMPO SYNC to the host tempo. LFO 2 is moved by the mod lever: RATE, DEPTH, and DEPTH SELECT to send it to PITCH (vibrato), FILTER or AMP.

## Keyboard, controller and ribbon

- **PORTAMENTO** with TIME: pitch glides from note to note. **VELOCITY** switch: how hard you play changes the morph destinations.
- **MONO** and **LEGATO** (one note at a time; legato joins notes smoothly), **UNISON** with UNISON DETUNE (0 to 50 cent), **VOICES** (maximum number of voices, 8 up to 128; more voices use more CPU), **ENV TYPE IN SOLO** (ANALOG lets a new note continue from where the old envelope was), **PATCH GAIN** (0, +6 or +12 dB).
- **RIBBON / CTRL ASSIGN:** press it, then move a knob or slider. The ribbon and the control lever now move that parameter. **VELOCITY ASSIGN** does the same for velocity (morphing). BEND UP and BEND DOWN set the pitch bend range. On the ribbon, RELATIVE changes the value around where it is, and HOLD keeps it when you let go.

## Arpeggiator and RPS

ON / OFF, HOLD (keep playing after you let go), MODE (UP, DOWN, UP & DOWN, RANDOM, RPS), RANGE (1 to 4 octaves) and TEMPO. In the plugin the tempo follows the host. The beat pattern list (on the parameter pages) has 90 entries from 1/4 to 1/32 and the PORTA, SEQUENCE, ECHO, MUTE, STRUMMING, REFRAIN, PERCUSSION, WALKING BASS, HARP and RANDOM groups. REC and ROM PLAY are for the RPS (see Known limits). The two MOTION buttons play back recorded knob movements (planned).

## Key mode and panel select

KEY MODE: SINGLE, DUAL (two patches layered) or SPLIT (a split point, one patch per side). PANEL SELECT chooses which part the panel edits: LOWER, UPPER, BOTH (edits the two parts together) or VOICE MOD. VOICE ASSIGN shares the voices between the two parts (8-2 down to 2-8). OUTPUT ASSIGN mixes both parts, or sends lower left and upper right.

## Effects

The built in effects of the Sound page: BASS and TREBLE tone shelves, the MULTI-FX (twelve chorus, flanger and phaser types plus DISTORTION) with its LEVEL, and the DELAY with TIME, FEEDBACK, LEVEL and five types (panning left to right, right to left, short panning, mono short, mono long). TEMPO SYNC choices for chorus and delay are on the parameter pages.

## Parameter pages

Press EDIT, then a BANK button: PFM COMMON, VOICE MOD, PART, PATCH, MIDI, SETUP, INIT / UTIL or SCOPE. These hold the less often used settings, such as the split point, part transpose and detune, MIDI channels, tempo sync choices, master tune and LCD contrast. EXIT closes the page.

# The FX page

![The FX page, CHAIN](img/fx-chain.png)

The FX tab is our own twist: a processing chain that makes a sound ready for a mix. It has two pages, CHAIN and MASTER. Every module has an ON switch, and every block has its own preset menu. The MIX READY master switch turns the whole chain on or off; switch it off to hear the raw synth. The displays show the sound as it moves through the chain.

## MIX READY and the trance gate

- **MIX READY:** the master switch, SOUND TYPE (LEAD, PAD, BASS, PLUCK, CHORD, FX), APPLY TYPE (sets every module for that type) and OUT TRIM (-12 to +12 dB after the limiter).
- **TRANCE GATE:** a 16 step gate that chops the sound in time with the host tempo. Click or drag on the steps to draw a pattern (PATTERN then reads USER) or choose a built in pattern. CUT DEPTH is how low the sound falls between steps (0 dB to -60 dB, top is silent). SMOOTH softens the edges. DECAY lets each open step fall away like a pluck. SWELL lets the gap rise back into the next note. RATE is the step length (1/4 to 1/32, triplets included). POSITION: BEFORE FX lets delay tails ring between steps, AFTER FX chops the effects too.
- **RESPONSE:** a display of the tone shape of the whole chain.

## CHAIN page

| Module | What it does |
|---|---|
| DRIVE STACK | Three stages in a row: DRIVE (smooth opamp saturation), TRANS. (asymmetric transistor stage, adds even harmonics) and FUZZ (gritty, with an octave up growl). MIX blends clean and driven. |
| LOW END | POCKET HPF clears the low mids so kick and bass keep their pocket (pads and leads about 250 to 300 Hz, bass off). LOW MONO makes everything below its frequency mono (about 120 Hz). |
| M/S EQ | MID PRES. (centre, around 3 kHz), SIDE HPF (high pass on the sides only) and AIR (high shelf at 10 kHz). |
| TRANSIENT | ATTACK adds or removes punch at the start of a note, SUSTAIN the body after it (-12 to +12). |
| TAMER | Calms harsh zones (FOCUS: 3 to 5 kHz, 7 to 9 kHz or both) by DEPTH. It only acts when peaks stick out. |
| GLUE | A gentle compressor: RATIO (1.5:1 to 4:1), THRESH (set for about 1.5 to 2 dB of reduction), ATTACK and PARALLEL (a squashed copy blended in). |
| WIDENER | MODE MICRO PITCH (sides shifted about 7 cent with short delays, mono safe) or HAAS, with AMOUNT. |
| AUTO PAN | A slow pan: RATE and DEPTH. |

## MASTER page

| Module | What it does |
|---|---|
| OPTO LEVELLER | A slow, program dependent compressor in the style of a classic opto unit (our own model). MODE: COMPRESS (gentle) or LIMIT (firm). PEAK RED. and GAIN on a 0 to 100 scale, MIX with the dry sound. |
| OTT SQUEEZE | Three bands: loud parts go down, quiet parts come up, for a big dense sound. SQUASH and LIFT per band, DEPTH, TIME and LOW, MID, HIGH levels. A little goes a long way: 12 to 18 percent depth is plenty on a bright synth. |
| MULTIBAND COMPRESSOR & LIMITER | Four bands (LOW, LOW MID, HIGH MID, HIGH) with three crossovers you can drag in the display. Every band has a compressor (THRESH, RATIO, ATTACK, RELEASE, BOOST) and its own limiter (RELEASE, OUTPUT), with BAND on, MUTE and SOLO. MIX below 100 percent gives parallel compression. MUTE and SOLO are not saved with the sound. |
| OUTPUT STAGE | A soft CLIPPER (CLIP) in front of a true peak LIMITER (LOUDNESS 0 to 12 dB, CEILING -3.0 to 0.0 dB, RELEASE, LOOK AHEAD). A ceiling of -1.0 dB is safe for any format. More LOOK AHEAD is cleaner but adds latency, which the plugin reports to your DAW. |

The whole chain runs in this order: gate (BEFORE FX position), DRIVE STACK, LOW END, M/S EQ, TRANSIENT, TAMER, OPTO LEVELLER, OTT SQUEEZE, MULTIBAND COMPRESSOR & LIMITER, GLUE, WIDENER, AUTO PAN, CLIPPER, LIMITER. The WHERE IT SITS display on the MASTER page shows it.

![The FX page, MASTER](img/fx-master.png)

# Presets and banks

- **Browser:** click the preset name. Banks A to D are factory, E to G are yours (128 slots each). Search by name, type, genre or tag, filter by TYPE and GENRE chips, and go back through the HISTORY. The START HERE list gathers the best first sounds. This version has 51 factory sounds in bank A; banks B to D are empty for now.
- **Types:** Lead, Bass, Keys, Pad, Pluck, Chord, Brass, Bell, Drum, FX. **Genres:** Trance, Uplifting, Hard Trance, Goa, Psytrance, Progressive, Techno, Acid, Ambient.
- **Save:** asks before it overwrites (Overwrite, Save as new or Cancel). Saving or renaming a factory sound makes your own copy in bank E, the factory sound stays as it was.
- **Rename:** double click the preset name. **Delete:** only your own sounds.
- The sound is also stored in your DAW project, so a song keeps its sound even if you later change your presets.

![The preset browser](img/browser.png)

# Import and export

- **Own format (text):** Export shows the preset as plain JSON text that you can copy. Import checks it first: unknown keys are ignored, values that are the wrong type or out of range are skipped and counted, and a file that is not a JPurrr-8000 preset is refused. Banks use the same format.
- **JP-8000 and JP-8080 SysEx (planned):** import and export of patches and sound banks as SysEx messages (one message per patch, with checksum). It will be built from the documented parameter address map and checked against real files and hardware before release. Parts of the format that cannot be verified are left out instead of guessed.

# MIDI

JPurrr-8000 is a MIDI instrument: it receives notes with velocity, pitch bend, channel pressure and controllers. The mod lever moves LFO 2. A list of the MIDI controller and NRPN numbers for the knobs and sliders is planned and will be added to this manual with the first version that has it. Until then, automate parameters from your DAW: every saved parameter is a host parameter, named like `CUTOFF FREQ [fc]`.

# Settings

The Settings button opens:

- **Show info bubbles:** on by default.
- **Power saving:** pauses meters and displays when the window is hidden.
- **Colour look:** JP Blue (default), Neon Navy, Purple Soda Pop or PurrOne charcoal. The look is saved with the plugin settings, not with each preset. The wooden cheeks, the logo, the bubbles and the meter colours stay the same in every look.

The Guide button shows a short list of the shortcuts. About & Support shows the version, the project links and the Ko-fi page. Support is optional. Nothing is locked and there are no reminders.

# Tips

- Start with a sound from START HERE, then change one thing at a time.
- For a lead, keep POCKET HPF around 250 to 300 Hz so it leaves room for kick and bass. For a bass, switch it off.
- If a sound is already bright, use only a little OTT SQUEEZE (12 to 18 percent).
- Keep the limiter CEILING at -1.0 dB if you export for streaming.
- The trance gate POSITION decides if delay tails ring between steps (BEFORE FX) or get chopped too (AFTER FX).
- Use MUTE and SOLO on the multiband bands to hear what each band does. They are not saved with the sound.
- A higher VOICES count uses more CPU. 10 voices are like the hardware.
- Use a MIDI keyboard or your DAW's piano roll. The standalone app needs a MIDI device.

# Known limits

This is an early version. Written down plainly so nothing surprises you:

- JPurrr-8000 follows the sound and the front panel of the JP-8000 and JP-8080. It is not a sample or ROM copy and it will not sound identical to the hardware.
- SysEx import and export, and the MIDI controller and NRPN list, are planned (see above).
- There is no audio input yet, so the EXTERNAL INPUT section and the EXT oscillator source have no sound to work on. The controls are on the panel and are saved with the sound. The same is planned for the VOICE MODULATOR processing.
- RPS recording and ROM patterns, MOTION recording, INDIVIDUAL TRIGGER and EXTERNAL TRIGGER are planned.
- An on-screen preview keyboard is part of the design mockup but not part of the plugin; play it with a MIDI keyboard.
- Only the VST3 format is built. There is no AU, CLAP or AAX version.
- The installers are not signed (see Install), so Windows and macOS show a warning once.
- A plugin can behave differently in a host that was not tested. Please report problems, with the name of your host, on the issues page of the project.

# Credits and license

JPurrr-8000 is by Studio 87. It is free and open source under the GNU Affero General Public License, version 3 or later (AGPL-3.0-or-later). The source code, the issue tracker and the releases are at github.com/SpeakingColors/JPurrr-8000. The plugin is built with the JUCE framework. The fonts Barlow, Barlow Condensed and JetBrains Mono are used under the SIL Open Font License.

If you like it, you can leave a tip on Ko-fi (ko-fi.com/studio87). Not affiliated with or endorsed by Roland or Arturia. All names belong to their owners.
