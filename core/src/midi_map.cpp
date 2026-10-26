// MIDI controller map. See midi_map.h and docs/midi-and-sysex.md.
#include "jpurrr/midi_map.h"
#include <cstdint>

namespace jpurrr {
namespace {

// Factory assignment of every knob and slider: JP-8080 owner's manual p. 188, "Transmit/Receive Setting List", MODE2 column.
const MidiCcEntry kCc[] = {
    { 92, "tb",   "TREMOLO (CC92): tone control bass" },
    { 95, "tt",   "PHASER (CC95): tone control treble" },
    { 93, "xl",   "CHORUS (CC93): multi-FX level" },
    { 12, "dt",   "EFFECT-CTL1 (CC12): delay time" },
    { 13, "dfb",  "EFFECT-CTL2 (CC13): delay feedback" },
    { 94, "dl",   "CELESTE (CC94): delay level" },
    { 16, "l1r",  "GENERAL1 (CC16): LFO 1 rate" },
    { 20, "l1f",  "CC20: LFO 1 fade" },
    { 17, "m2r",  "GENERAL2 (CC17): LFO 2 rate" },
    { 22, "m2dp", "CC22: LFO 2 depth pitch" },
    { 23, "m2df", "CC23: LFO 2 depth filter" },
    { 24, "m2da", "CC24: LFO 2 depth amp" },
    { 5,  "pt",   "PORTA-TIME (CC05): portamento time" },
    { 4,  "o1c1", "FOOT-TYPE (CC04): OSC 1 control 1" },
    { 76, "o1c2", "SOUND-CTL7 (CC76): OSC 1 control 2" },
    { 21, "o2r",  "CC21: OSC 2 range" },
    { 77, "o2f",  "SOUND-CTL8 (CC77): OSC 2 fine / wide" },
    { 78, "o2c1", "SOUND-CTL9 (CC78): OSC 2 control 1" },
    { 79, "o2c2", "SOUND-CTL10 (CC79): OSC 2 control 2" },
    { 8,  "ob",   "BALANCE (CC08): OSC balance" },
    { 70, "ox",   "SOUND-CTL1 (CC70): X-MOD depth" },
    { 18, "ol",   "GENERAL3 (CC18): OSC LFO 1 depth" },
    { 25, "pd",   "CC25: pitch envelope depth" },
    { 26, "pa",   "CC26: pitch envelope attack" },
    { 27, "pdd",  "CC27: pitch envelope decay" },
    { 74, "fc",   "SOUND-CTL5 (CC74): cutoff" },
    { 71, "fr",   "SOUND-CTL2 (CC71): resonance" },
    { 30, "fk",   "CC30: key follow" },
    { 19, "fl",   "GENERAL4 (CC19): filter LFO 1 depth" },
    { 81, "fe",   "GENERAL6 (CC81): filter envelope depth" },
    { 82, "fea",  "GENERAL7 (CC82): filter envelope attack" },
    { 83, "fed",  "GENERAL8 (CC83): filter envelope decay" },
    { 28, "fes",  "CC28: filter envelope sustain" },
    { 29, "fer",  "CC29: filter envelope release" },
    { 80, "al",   "GENERAL5 (CC80): amp LFO 1 depth" },
    { 73, "aea",  "SOUND-CTL4 (CC73): amp envelope attack" },
    { 75, "aed",  "SOUND-CTL6 (CC75): amp envelope decay" },
    { 31, "aes",  "CC31: amp envelope sustain" },
    { 72, "aer",  "SOUND-CTL3 (CC72): amp envelope release" },
};
constexpr int kNumCc = int(sizeof(kCc) / sizeof(kCc[0]));

struct Lookup {
    short byCc[128];            // parameter index or -1
    short ccOf[kNumParams];
    Lookup() {
        for (short& x : byCc) x = -1;
        for (short& x : ccOf) x = -1;
        for (int i = 0; i < kNumCc; ++i) {
            int p = paramIndexById(kCc[i].paramId);
            if (p >= 0) { byCc[kCc[i].cc] = short(p); ccOf[p] = short(kCc[i].cc); }
        }
    }
};
const Lookup& lookup() { static const Lookup l; return l; }

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

MidiMapResult one(int param, int value) {
    MidiMapResult r; r.count = 1; r.updates[0] = { param, value }; return r;
}

}  // namespace

void midiMapInit() { (void)lookup(); }
const MidiCcEntry* midiCcTable(int& count) { count = kNumCc; return kCc; }
int midiParamForCc(int cc) { return (cc >= 0 && cc < 128) ? lookup().byCc[cc] : -1; }
int midiCcForParam(int p) { return (p >= 0 && p < kNumParams) ? lookup().ccOf[p] : -1; }

int midiNrpnForParam(int p) {
    if (p < 0 || p >= kNumParams || (kParams[p].flags & kMomentary)) return -1;
    return p;
}
int midiParamForNrpn(int n) { return midiNrpnForParam(n); }

int midiScaleToParam(int p, int raw, int bits) {
    if (p < 0 || p >= kNumParams) return 0;
    const ParamDef& d = kParams[p];
    int64_t den = bits >= 14 ? 16383 : 127;
    int64_t r = raw < 0 ? 0 : (raw > den ? den : raw);
    int64_t span = int64_t(d.max) - d.min;
    return int(d.min + (r * span + den / 2) / den);
}

int midiBendToParam(int bend14) { return clampi(bend14 >> 7, 0, 127) - 64; }

void MidiMapper::reset() {
    mode_ = None; selMsb_ = 127; selLsb_ = 127; dataMsb_ = 0; dataLsb_ = 0; haveMsb_ = false; haveLsb_ = false;
}

MidiMapResult MidiMapper::applyData(bool fromMsb) {
    MidiMapResult none;
    if (mode_ == Nrpn) {
        int p = midiParamForNrpn(selMsb_ * 128 + selLsb_);
        if (p < 0 || !haveMsb_) return none;
        // Only the MSB so far: treat it as a 7 bit value so a sender that never sends CC 38 still reaches the full range.
        int v = haveLsb_ ? midiScaleToParam(p, dataMsb_ * 128 + dataLsb_, 14) : midiScaleToParam(p, dataMsb_, 7);
        return one(p, v);
    }
    if (mode_ == Rpn) {
        if (!fromMsb) return none;                                     // the manual: the LSB of both supported RPNs is ignored
        if (selMsb_ == 0 && selLsb_ == 0) {                            // pitch bend sensitivity, 0..24 semitones
            int semis = clampi(dataMsb_, 0, 24);
            int up = paramIndexById("bu"), down = paramIndexById("bd");
            MidiMapResult r;
            if (up >= 0) r.updates[r.count++] = { up, semis };
            if (down >= 0) r.updates[r.count++] = { down, -semis };
            return r;
        }
        if (selMsb_ == 0 && selLsb_ == 2) {                            // master coarse tuning 28H..58H = -24..+24 semitones
            int pt = paramIndexById("ptr");
            if (pt >= 0) return one(pt, clampi(dataMsb_ - 64, -24, 24));
        }
    }
    return none;
}

MidiMapResult MidiMapper::controlChange(int cc, int value, const int* current) {
    MidiMapResult r;
    if (cc < 0 || cc > 127) return r;
    value = clampi(value, 0, 127);
    switch (cc) {
    case 99: selMsb_ = value; mode_ = Nrpn; haveMsb_ = haveLsb_ = false; dataMsb_ = dataLsb_ = 0; return r;
    case 98: selLsb_ = value; mode_ = Nrpn; haveMsb_ = haveLsb_ = false; dataMsb_ = dataLsb_ = 0; return r;
    case 101: selMsb_ = value; mode_ = Rpn; haveMsb_ = haveLsb_ = false; dataMsb_ = dataLsb_ = 0; if (selMsb_ == 127 && selLsb_ == 127) mode_ = None; return r;
    case 100: selLsb_ = value; mode_ = Rpn; haveMsb_ = haveLsb_ = false; dataMsb_ = dataLsb_ = 0; if (selMsb_ == 127 && selLsb_ == 127) mode_ = None; return r;
    case 6:  dataMsb_ = value; haveMsb_ = true; return applyData(true);
    case 38: if (!haveMsb_) return r; dataLsb_ = value; haveLsb_ = true; return applyData(false);
    case 1:  { int p = P_mod; return one(p, midiScaleToParam(p, value, 7)); }          // modulation lever (LFO 2 depth source)
    case 7:  { int p = P_vol; return one(p, midiScaleToParam(p, value, 7)); }          // volume: plugin master level
    case 10: {                                                                          // pan: only meaningful in MANUAL pan
        if (!current || current[P_ap] != 2) return r;
        return one(P_al, midiScaleToParam(P_al, value, 7));
    }
    case 64: r.sustain = value >= 64 ? 1 : 0; return r;
    case 65: return one(P_pon, value >= 64 ? 1 : 0);                                    // portamento switch
    case 120: r.allSoundOff = true; return r;
    case 121: r.resetControllers = true; mode_ = None; haveMsb_ = haveLsb_ = false; return r;
    case 123: case 124: case 125: r.allNotesOff = true; return r;
    case 126: r = one(P_mono, 1); r.allNotesOff = true; return r;                        // Mono: manual says all notes off, MONO on
    case 127: r = one(P_mono, 0); r.allNotesOff = true; return r;                        // Poly: all notes off, MONO off
    default: break;
    }
    int p = midiParamForCc(cc);
    if (p >= 0) return one(p, midiScaleToParam(p, value, 7));
    return r;
}

}  // namespace jpurrr
