// One synth voice and the per patch parameter set it reads. Pure C++17, no allocation.
#pragma once
#include "synth_dsp.h"
#include "jpurrr/params_gen.h"
#include "jpurrr/synth_math.h"

namespace jpurrr {

constexpr int kControlBlock = 16;        // host samples between control updates (about 3 kHz at 48 kHz)

// Everything the Sound page says about one patch, converted to engine units.
// Fields in s[] are smoothed by the engine; everything else is read as is.
struct PatchParams {
    enum {
        S_cut,      // log2 of the base cutoff in Hz
        S_res,      // 0..1
        S_fenv,     // octaves at full envelope (signed)
        S_flfo,     // octaves at full LFO 1 (signed)
        S_fkey,     // octaves per octave of key
        S_level,    // amp level times patch gain
        S_bal1, S_bal2,   // OSC balance gains (0..1)
        S_o1c1, S_o1c2, S_o2c1, S_o2c2,   // oscillator controls 0..127
        S_o2semi,   // OSC 2 pitch against OSC 1 in semitones (range + fine)
        S_xmod,     // 0..1
        S_ol,       // LFO 1 pitch depth in semitones
        S_pdep,     // pitch envelope depth in semitones (signed)
        S_al,       // -1..1
        S_m2p, S_m2f, S_m2a,   // LFO 2 depths: semitones, octaves, tremolo
        S_ud,       // unison spread in cents
        S_shift,    // octave shifts, transpose, master tune, part detune in semitones
        S_kshift,   // keyboard shift + transpose in semitones (for key follow)
        S_count
    };
    float s[S_count] = {};
    int o1w = 0, o2w = 0, od = 0, ft = 2, ap = 0, l1w = 0, pgSel = 0;
    bool f24 = true, ring = false, sync = false, mono = false, leg = false, uni = false, porta = false;
    bool velMorph = false, analogEnv = false, l1sync = false;
    float aA = 0.002f, aD = 0.1f, aS = 0.8f, aR = 0.1f;       // seconds / level
    float fA = 0.002f, fD = 0.1f, fS = 0.5f, fR = 0.1f;
    float pA = 0.001f, pD = 0.1f;
    float portaTime = 0.f;                                      // seconds
    float l1Hz = 1.f, l1Beats = 1.f, l1Fade = 0.f, l2Hz = 1.f; // l1Beats: cycle length in beats when synced
    float bendUp = 2.f, bendDown = -2.f;                        // semitones
    int vc = 10;

    // v: kNumParams ints. partDetuneCents: from the Part Detune parameter in dual and split.
    void derive(const int* v, float partDetuneCents);
    // Fields that rebuild the signal path (waves, filter type and slope, ring, sync, pan mode, LFO wave): changing them
    // while notes sound would click, so the engine ducks the output around the change.
    int structKey() const { return o1w | (o2w << 3) | (od << 5) | (ft << 7) | (ap << 9) | (l1w << 11) | ((f24 ? 1 : 0) << 13) | ((ring ? 1 : 0) << 14) | ((sync ? 1 : 0) << 15); }
    void copyStruct(const PatchParams& t) { o1w = t.o1w; o2w = t.o2w; od = t.od; ft = t.ft; ap = t.ap; l1w = t.l1w; f24 = t.f24; ring = t.ring; sync = t.sync; }
    // Moves s[] toward target.s and copies the other fields (not the structural ones unless applyStruct).
    void smoothToward(const PatchParams& target, float a, bool applyStruct = true);
};

// Controller state shared by all voices for one control block.
struct BlockCtl {
    float bend = 0.f;        // -1..1 (MIDI bend plus UI lever)
    float mod = 0.f;         // 0..1 (mod wheel, lever, pressure)
    float l2[2] = { 0.f, 0.f };   // LFO 2 value per part, -1..1
    double bpm = 120.0;
    bool playing = false;
    double ppq = 0.0;        // quarter note position at the start of this control block
    bool parallelOut = false; // dual / split with OUTPUT ASSIGN = PARALLEL OUT: lower part left, upper part right
};

enum class VState : uint8_t { Idle, Held, Released, Fading };

struct Voice {
    // identity
    VState st = VState::Idle;
    int part = 0, note = 60, ch = 0, vel = 100;
    uint32_t age = 0;
    bool sustained = false;       // key is up but the pedal holds the note
    bool solo = false;            // belongs to the mono / legato / unison group of its part
    int uniIdx = 0, uniN = 1;     // position in the unison stack
    float uniGain = 1.f;

    // setup
    double sr = 48000.0, osr = 96000.0; int osFactor = 2;
    const dsp::HalfbandCoefs* hc = nullptr;
    void init(double hostSr, int osFac);
    void reset();

    // events
    void start(const PatchParams& P, int partIdx, int ch_, int note_, int vel_, uint32_t age_, uint32_t seed,
               double fromSemis, bool glideFrom);
    void retrigger(const PatchParams& P, int vel_, bool keepEnv);          // mono: new note on a sounding group voice
    void setNote(int note_, int vel_) { note = note_; vel = vel_; tgtSemis = note_; }
    void release();
    void fadeOut() { if (st != VState::Idle) { st = VState::Fading; fadeG = 1.f; } }
    bool sounding() const { return st == VState::Held || st == VState::Released; }

    // per control block
    void control(const PatchParams& P, const BlockCtl& C, bool snap);
    // adds n samples to L and R
    void render(int n, float* L, float* R);

    float level() const { return aenv.level; }

    // ---------- state ----------
    double noteSemis = 60.0, tgtSemis = 60.0;
    bool needSnap = true;
    // oscillators (phases in 0..1)
    double phSaw[7] = {}, ph1 = 0.0, ph2 = 0.0;
    double incSaw[7] = {}, dSaw[7] = {}, inc1 = 0.0, d1 = 0.0, inc2 = 0.0, d2 = 0.0;
    dsp::BlepOut bo1, bo2;
    float lastY2 = 0.f, fbState = 0.f, fbOut = 0.f, ssHp = 0.f;
    float nz1[2] = {}, nz2[2] = {};            // noise filter states (osc 1, osc 2)
    dsp::Rng rng;
    dsp::Decimator2 dec;
    dsp::VoiceFilter filt;
    dsp::Env aenv, fenv;
    float penv = 0.f; bool penvAttack = false; bool penvOn = false;
    // lfo 1
    double lfoPh = 0.0; float lfoSH = 0.f, lfoFade = 0.f;
    // per block constants for the oscillator section
    int kind1 = 0, kind2 = 0;                   // wave ids
    float ssGain[7] = {}; float ssHpA = 0.f;
    float pw1 = 0.5f, pw2 = 0.5f;
    float w1Saw = 1.f, w1Tri = 0.f, w1Sin = 0.f, w2Saw = 1.f, w2Tri = 0.f, w2Sin = 0.f;
    float triModA = 0.f, fbBeta = 0.f, fbLp = 0.5f;
    float noiseA1 = 0.f, noiseK1 = 1.f, noiseG1 = 0.5f, noiseA2 = 0.f, noiseK2 = 1.f, noiseG2 = 0.5f, nzG1 = 0.f, nzG2 = 0.f;
    float mix1 = 0.7f, mix2 = 0.f;
    float xmodOct = 0.f;
    bool ringOn = false, syncOn = false;
    // ramped per block values
    double g = 0.1, dg = 0.0, kf = 0.0, dkf = 0.0;
    float ampG = 0.f, dAmpG = 0.f, pl = 0.7f, dpl = 0.f, pr = 0.7f, dpr = 0.f;
    int ftype = 2; bool p24 = true;
    float declick = 0.f, declickStep = 0.f;
    float fadeG = 1.f;
    bool seeded = false;
    uint32_t noteCounter = 0;
    // last control block copies used by render
    float invCB = 1.f / kControlBlock;
    bool ssMode() const { return kind1 == 0; }
    inline float oscStep();
    inline float waveTick(double& ph, dsp::BlepOut& bo, double inc, int kind, float pw, float wSaw, float wTri, float wSin,
                          float triA, bool doSync, float syncT, bool& wrapped, float& wrapT);
};

}  // namespace jpurrr
