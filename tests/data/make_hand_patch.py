#!/usr/bin/env python3
"""Builds tests/data/hand_patch.syx: one hand made JP-8080 patch (user slot B11), written by hand from the
JP-8080 owner's manual p.203 "4-3. Patch" and p.206 (checksum). It does not share code with the C++ exporter,
so the test compares two independent readings of the manual. Not Roland data: every value is invented here."""
import sys
P = bytearray(248)
P[0:16] = b"Hand Test 1     "
def put(off, v): P[off] = v
# offsets are the manual's "Offset Address" as 7 bit numbers: 00 00 00 29 = 0x29, 00 00 01 18 = 128 + 0x18
for off in range(74, 151, 2): P[off], P[off + 1] = 0, 127       # Control: depth words, neutral
P[153], P[154] = 0, 127
for off in range(156, 235, 2): P[off], P[off + 1] = 0, 127      # Velocity: depth words, neutral
put(0x10, 2)      # LFO1 waveform SQR
put(0x11, 90)     # LFO1 rate
put(0x12, 5)      # LFO1 fade
put(0x13, 77)     # LFO2 rate
put(0x14, 1)      # LFO2 depth select FILTER
put(0x15, 1)      # ring on
put(0x16, 12)     # cross mod depth
put(0x17, 100)    # osc balance (+36)
put(0x18, 1)      # LFO1 & env destination OSC2
put(0x19, 70)     # osc LFO1 depth (+6)
put(0x1A, 60)     # pitch LFO2 depth (-4)
put(0x1B, 80)     # pitch env depth (+16)
put(0x1C, 9)
put(0x1D, 33)
put(0x1E, 4)      # OSC1 waveform: value 4 = SQR (PWM) (the end of the list is certain)
put(0x1F, 100)
put(0x20, 33)
put(0x21, 2)      # OSC2 waveform value 2 = SAW (table: PULSE, TRI, SAW, NOISE)
put(0x22, 1)      # sync on
put(0x23, 13)     # OSC2 range byte 13 = -12 semitones
put(0x24, 60)     # fine byte 60 = +10 cent
put(0x25, 64)
put(0x26, 20)
put(0x27, 2)      # filter LPF
put(0x28, 1)      # -24 dB
put(0x29, 90)     # cutoff
put(0x2A, 44)     # resonance
put(0x2B, 96)     # key follow (+32)
put(0x2C, 64)
put(0x2D, 70)     # filter LFO2 depth (+6)
put(0x2E, 60)     # filter env depth (-4)
put(0x2F, 1); put(0x30, 50); put(0x31, 64); put(0x32, 30)
put(0x33, 110)    # amp level
put(0x34, 64)
put(0x35, 64)
put(0x36, 3); put(0x37, 80); put(0x38, 100); put(0x39, 70)
put(0x3A, 1)      # auto pan
put(0x3B, 70)     # bass (+6)
put(0x3C, 58)     # treble (-6)
put(0x3D, 12)     # multi-FX type DISTORTION (last entry, certain)
put(0x3E, 50)
put(0x3F, 0)      # delay type PANNING L->R (first entry, certain)
put(0x40, 70); put(0x41, 40); put(0x42, 22)
put(0x43, 12)     # bend up
put(0x44, 12)     # bend down magnitude
put(0x45, 1)      # portamento on
put(0x46, 33)
put(0x47, 1)      # mono
put(0x48, 0)      # legato off
put(0x49, 3)      # osc shift +1
put(128 + 0x18, 1)   # morph bend assign on
put(128 + 0x1B, 1)   # velocity switch on
put(128 + 0x6F, 1)   # env type in solo ANALOG
put(128 + 0x71, 0)
put(128 + 0x72, 1)   # voice modulator send on
put(128 + 0x73, 1)   # unison on
put(128 + 0x74, 20)  # unison detune
put(128 + 0x75, 2)   # patch gain +12 dB
put(128 + 0x76, 1)   # external trigger on
put(128 + 0x77, 2)   # destination FILTER&AMP
def dt1(addr, data):
    body = bytes(addr) + bytes(data)
    s = (128 - sum(body) % 128) % 128
    return bytes([0xF0, 0x41, 0x10, 0x00, 0x06, 0x12]) + body + bytes([s, 0xF7])
base = [2, 1, 0, 0]                      # user patch B11: 02 01 00 00
out = dt1(base, P[:242]) + dt1([2, 1, 1, 0x72], P[242:])
open(sys.argv[1] if len(sys.argv) > 1 else "hand_patch.syx", "wb").write(out)
