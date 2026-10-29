#include "synth_voice.h"

namespace jpurrr {
using namespace dsp;

namespace {
const float kBeats[23] = { 0.f, 0.25f, 1.f / 3.f, 0.375f, 0.5f, 2.f / 3.f, 0.75f, 1.f, 4.f / 3.f, 1.5f, 2.f, 8.f / 3.f, 3.f, 4.f,
                           16.f / 3.f, 6.f, 8.f, 12.f, 16.f, 20.f, 24.f, 28.f, 32.f };
const float kPulseAmp = 0.7f;
// LFO 1 depth on the SHAPE of saw and triangle waves (and on the triangle mod offset) is kept gentle on purpose:
// the factory sounds leave CTRL 2 at its Super Saw value (105), which must not make a plain saw wobble. Own choice.
const float kShapeLfo = 16.f;

inline float lfoShape(int kind, double ph, float sh) {
    switch (kind) {
        case 0: { double t = ph + 0.25; t -= std::floor(t); return (float) (t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t); }   // TRI, starts at zero going up
        case 1: return (float) (1.0 - 2.0 * ph);                                                                        // SAW (falling)
        case 2: return ph < 0.5 ? 1.f : -1.f;                                                                           // SQR
        default: return sh;                                                                                              // S/H
    }
}
inline float triWave(double p) { return (float) (p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p); }
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
}  // namespace

// ================= PatchParams =================
void PatchParams::derive(const int* v, float partDetuneCents) {
    s[S_cut] = (float) (std::log2(20.0) + v[P_fc] / 127.0 * std::log2(1000.0));
    s[S_res] = v[P_fr] / 127.f;
    s[S_fenv] = v[P_fe] / 64.f * 5.f;
    s[S_flfo] = v[P_fl] / 64.f * 4.f;
    // KEY FOLLOW (manual p.61): +32 = +100 percent (one octave of cutoff per octave of key), +64 = +200 percent, -64 = -100 percent.
    s[S_fkey] = v[P_fk] >= 0 ? v[P_fk] / 32.f : v[P_fk] / 64.f;
    pgSel = std::min(2, std::max(0, v[P_pg]));
    s[S_level] = v[P_av] / 127.f * (pgSel == 0 ? 1.f : pgSel == 1 ? 2.f : 4.f);
    const float b = (v[P_ob] + 64) / 127.f;
    s[S_bal1] = std::min(1.f, 2.f * (1.f - b));
    s[S_bal2] = std::min(1.f, 2.f * b);
    s[S_o1c1] = (float) v[P_o1c1]; s[S_o1c2] = (float) v[P_o1c2];
    s[S_o2c1] = (float) v[P_o2c1]; s[S_o2c2] = (float) v[P_o2c2];
    // WIDE moves OSC 2 in whole octaves (-4..+4) and ignores FINE, as in the mockup preview.
    s[S_o2semi] = v[P_o2wide] ? 12.f * std::round(v[P_o2r] / 6.f) : v[P_o2r] + (v[P_o2f] * 50.f / 64.f) / 100.f;
    s[S_xmod] = v[P_ox] / 127.f;
    s[S_ol] = v[P_ol] / 64.f * 12.f;
    s[S_pdep] = v[P_pd] / 64.f * 12.f;                // +-1 octave, as the mockup preview
    s[S_al] = v[P_al] / 64.f;
    s[S_m2p] = v[P_m2dp] / 64.f * 2.f;
    s[S_m2f] = v[P_m2df] / 64.f * 3.f;
    s[S_m2a] = v[P_m2da] / 64.f;
    s[S_ud] = (float) v[P_ud];
    const float tuneCents = (float) (1200.0 * std::log2(std::max(1, v[P_mtu]) / 4400.0));
    s[S_shift] = 12.f * (v[P_os] + v[P_ksh]) + v[P_ptr] + (tuneCents + partDetuneCents) / 100.f;
    s[S_kshift] = 12.f * v[P_ksh] + v[P_ptr];

    o1w = std::min(6, std::max(0, v[P_o1w])); o2w = std::min(3, std::max(0, v[P_o2w]));
    od = v[P_od]; ft = v[P_ft]; f24 = v[P_f24] != 0; ap = v[P_ap];
    ring = v[P_ring] != 0; sync = v[P_sync] != 0 && o2w != 3;
    mono = v[P_mono] != 0; leg = v[P_leg] != 0; uni = v[P_uni] != 0; porta = v[P_pon] != 0;
    // FEEDBACK OSC is mono only and cannot be used with unison (mockup rule).
    if (o1w == 3) { uni = false; if (!leg) mono = true; }
    if (mono && leg) mono = false;
    velMorph = v[P_vel] != 0; analogEnv = v[P_es] != 0;
    l1w = v[P_l1w]; l1sync = v[P_l1s] != 0;
    aA = (float) knobAmpAttack(v[P_aea]); aD = (float) knobAmpDecay(v[P_aed]); aS = v[P_aes] / 127.f; aR = (float) knobAmpRelease(v[P_aer]);
    fA = (float) knobAmpAttack(v[P_fea]); fD = (float) knobAmpDecay(v[P_fed]); fS = v[P_fes] / 127.f; fR = (float) knobAmpRelease(v[P_fer]);
    pA = (float) knobPitchAttack(v[P_pa]); pD = (float) knobPitchDecay(v[P_pdd]);
    // Portamento time: own law, 0..2.5 s, quadratic. Unverified against hardware.
    const float pt = v[P_pt] / 127.f;
    portaTime = porta ? 2.5f * pt * pt : 0.f;
    l1Hz = (float) knobLfoRate(v[P_l1r]);
    const int lsy = std::min(22, std::max(0, v[P_lsy]));
    const int idx = lsy > 0 ? lsy : 1 + std::min(21, (int) (v[P_l1r] / 128.f * 22.f));
    l1Beats = kBeats[idx];
    l1Fade = 8.f * (v[P_l1f] / 127.f) * (v[P_l1f] / 127.f);
    l2Hz = (float) knobLfoRate(v[P_m2r]);
    bendUp = (float) v[P_bu]; bendDown = (float) v[P_bd];
    vc = v[P_vc];
}

void PatchParams::smoothToward(const PatchParams& t, float a, bool applyStruct) {
    float keep[S_count];
    for (int i = 0; i < S_count; ++i) keep[i] = s[i] + (t.s[i] - s[i]) * a;
    PatchParams old = *this;
    *this = t;
    for (int i = 0; i < S_count; ++i) s[i] = keep[i];
    if (!applyStruct) copyStruct(old);
}

// ================= Voice =================
void Voice::init(double hostSr, int osFac) {
    sr = hostSr; osFactor = osFac; osr = hostSr * osFac;
    hc = &HalfbandCoefs::get();
    reset();
}

void Voice::reset() {
    st = VState::Idle; sustained = false; solo = false;
    aenv.kill(); fenv.kill(); filt.reset(); dec.reset(); bo1.reset(); bo2.reset();
    lastY2 = fbState = fbOut = ssHp = 0.f; nz1[0] = nz1[1] = nz2[0] = nz2[1] = 0.f;
    declick = 0.f; fadeG = 1.f; ampG = dAmpG = 0.f;
}

void Voice::start(const PatchParams& P, int partIdx, int ch_, int note_, int vel_, uint32_t age_, uint32_t seed,
                  double fromSemis, bool glideFrom) {
    st = VState::Held; sustained = false;
    part = partIdx; ch = ch_; note = note_; vel = vel_; age = age_;
    rng.seed(seed);
    tgtSemis = note_;
    noteSemis = glideFrom ? fromSemis : (double) note_;
    filt.reset(); dec.reset(); bo1.reset(); bo2.reset();
    lastY2 = fbState = fbOut = ssHp = 0.f; nz1[0] = nz1[1] = nz2[0] = nz2[1] = 0.f;
    aenv.setTimes(sr, P.aA, P.aD, P.aS, P.aR); fenv.setTimes(sr, P.fA, P.fD, P.fS, P.fR);
    aenv.gate(false); fenv.gate(false);
    penv = 0.f; penvAttack = true;
    lfoPh = 0.0; lfoSH = rng.bi(); lfoFade = 0.f;
    // Super Saw: a new random phase per note, then held static (Szabo).
    for (int k = 0; k < 7; ++k) phSaw[k] = rng.uni();
    const bool spread = uniN > 1;
    ph1 = spread ? rng.uni() : 0.0;
    ph2 = spread ? rng.uni() : 0.0;
    declick = 0.f; fadeG = 1.f;
    needSnap = true;
}

void Voice::retrigger(const PatchParams& P, int vel_, bool keepEnv) {
    vel = vel_;
    st = VState::Held; sustained = false;
    if (!keepEnv) { declick = aenv.level; declickStep = declick / 96.f; }
    aenv.setTimes(sr, P.aA, P.aD, P.aS, P.aR); fenv.setTimes(sr, P.fA, P.fD, P.fS, P.fR);
    aenv.gate(keepEnv); fenv.gate(keepEnv);
    penv = 0.f; penvAttack = true;
    lfoFade = 0.f;
}

void Voice::release() {
    if (st != VState::Held) return;
    st = VState::Released; sustained = false;
    aenv.release(); fenv.release();
}

void Voice::control(const PatchParams& P, const BlockCtl& C, bool snap) {
    const double dt = kControlBlock / sr;
    snap = snap || needSnap; needSnap = false;

    // ---- portamento ----
    if (P.portaTime > 0.f && noteSemis != tgtSemis) {
        const double coef = 1.0 - std::exp(-3.0 * dt / P.portaTime);
        noteSemis += (tgtSemis - noteSemis) * coef;
        if (std::fabs(tgtSemis - noteSemis) < 1e-4) noteSemis = tgtSemis;
    } else noteSemis = tgtSemis;

    // ---- LFO 1 (per voice, key triggered, with fade) ----
    if (P.l1sync) {
        const double cyc = std::max(0.05f, P.l1Beats);
        const double old = lfoPh;
        if (C.playing) { lfoPh = C.ppq / cyc; lfoPh -= std::floor(lfoPh); }
        else { lfoPh += dt * C.bpm / 60.0 / cyc; lfoPh -= std::floor(lfoPh); }
        if (lfoPh < old) lfoSH = rng.bi();
    } else {
        lfoPh += dt * P.l1Hz;
        if (lfoPh >= 1.0) { lfoPh -= std::floor(lfoPh); lfoSH = rng.bi(); }
    }
    const float lfoRaw = lfoShape(P.l1w, lfoPh, lfoSH);
    lfoFade = P.l1Fade > 0.001f ? std::min(1.f, lfoFade + (float) (dt / P.l1Fade)) : 1.f;
    const float l1 = lfoRaw * lfoFade;
    const float l2 = C.l2[part] * C.mod;                     // LFO 2 scaled by the lever

    // ---- pitch envelope ----
    if (penvAttack) { penv += (float) (dt / P.pA); if (penv >= 1.f) { penv = 1.f; penvAttack = false; } }
    else penv *= (float) std::exp(-3.0 * dt / P.pD);
    if (penv < 1e-6f) penv = 0.f;
    const float penvSemis = P.s[PatchParams::S_pdep] * penv;

    // ---- pitch ----
    const float bendSemis = C.bend >= 0.f ? C.bend * P.bendUp : -C.bend * P.bendDown;
    const float uniCents = uniN > 1 ? (uniIdx / (float) (uniN - 1) - 0.5f) * P.s[PatchParams::S_ud] : 0.f;
    const double base = noteSemis + P.s[PatchParams::S_shift] + uniCents / 100.0 + bendSemis + l2 * P.s[PatchParams::S_m2p];
    const bool lfoO1 = P.od == 0, lfoO2 = P.od <= 1;
    const double s1 = base + (lfoO1 ? penvSemis + l1 * P.s[PatchParams::S_ol] : 0.0);
    const double s2 = base + P.s[PatchParams::S_o2semi] + (lfoO2 ? penvSemis + l1 * P.s[PatchParams::S_ol] : 0.0);
    const double f1 = 440.0 * std::exp2((s1 - 69.0) / 12.0);
    const double f2 = 440.0 * std::exp2((s2 - 69.0) / 12.0);
    const double maxInc = 0.45;
    const double inv = 1.0 / osr;
    auto ramp = [&](double& cur, double& d, double target) {
        if (snap) { cur = target; d = 0.0; } else d = (target - cur) / kControlBlock;
    };

    kind1 = P.o1w; kind2 = P.o2w;
    const float c11 = P.s[PatchParams::S_o1c1], c12 = P.s[PatchParams::S_o1c2];
    const float c21 = P.s[PatchParams::S_o2c1], c22 = P.s[PatchParams::S_o2c2];

    // OSC 1
    if (kind1 == 0) {
        const double curve = superSawCurve(c11 / 127.0);
        for (int k = 0; k < 7; ++k) ramp(incSaw[k], dSaw[k], std::min(maxInc, f1 * (1.0 + kSuperSawRatio[k] * curve) * inv));
        const double m = c12 / 127.0;
        const double cg = superSawCentreGain(m), sg = superSawSideGain(m);
        const double norm = 0.9 / std::sqrt(cg * cg + 6.0 * sg * sg);
        for (int k = 0; k < 7; ++k) ssGain[k] = (float) ((k == 3 ? cg : sg) * norm);
        // High pass following the pitch (Szabo: the real unit has one). Cutoff not measured: 0.8 x note frequency. Unverified.
        ssHpA = (float) (1.0 - std::exp(-kTwoPi * 0.8 * f1 * inv));
    } else {
        ramp(inc1, d1, std::min(maxInc, f1 * inv));
    }
    switch (kind1) {
        case 1: triModA = clampf(c11 / 127.f + l1 * (c12 / 127.f) * 0.25f, 0.f, 1.f) * 0.45f; break;
        case 2: {
            const double fc = std::min(0.45 * osr, knobCutoffHz((int) c11));
            const double gg = std::tan(kPi * fc * inv), kk = 2.0 * std::pow(1.0 - c12 / 127.0, 1.4) + 0.08;
            noiseA1 = (float) (1.0 / (1.0 + gg * (gg + kk))); noiseK1 = (float) kk; nzG1 = (float) gg;
            noiseG1 = 0.8f * (float) std::min(4.0, std::max(1.0, std::sqrt(8000.0 / fc)));
            break;
        }
        case 3: fbBeta = c12 / 127.f * 3.2f; fbLp = 0.04f + 0.96f * (c11 / 127.f) * (c11 / 127.f); break;
        case 4: pw1 = clampf(0.1f + 0.8f * c11 / 127.f + l1 * (c12 / 127.f) * 0.3f, 0.05f, 0.95f); break;
        case 5: case 6: {
            const float sh = clampf(c11 + l1 * (c12 / 127.f) * kShapeLfo, 0.f, 127.f);
            if (kind1 == 5) { const float w = std::max(0.f, 64.f - sh) / 64.f; w1Saw = 1.f - w; w1Tri = w; w1Sin = 0.f; }
            else if (sh >= 64.f) { const float q = (sh - 64.f) / 63.f; w1Saw = q; w1Tri = 1.f - q; w1Sin = 0.f; }
            else { const float q = (64.f - sh) / 64.f; w1Saw = 0.f; w1Tri = 1.f - q; w1Sin = q; }
            break;
        }
        default: break;
    }
    // OSC 2
    ramp(inc2, d2, std::min(maxInc, f2 * inv));
    switch (kind2) {
        case 0: pw2 = clampf(0.1f + 0.8f * c21 / 127.f + l1 * (c22 / 127.f) * 0.3f, 0.05f, 0.95f); break;
        case 1: case 2: {
            const float sh = clampf(c21 + l1 * (c22 / 127.f) * kShapeLfo, 0.f, 127.f);
            if (kind2 == 1) { const float w = std::max(0.f, 64.f - sh) / 64.f; w2Saw = 1.f - w; w2Tri = w; w2Sin = 0.f; }
            else if (sh >= 64.f) { const float q = (sh - 64.f) / 63.f; w2Saw = q; w2Tri = 1.f - q; w2Sin = 0.f; }
            else { const float q = (64.f - sh) / 64.f; w2Saw = 0.f; w2Tri = 1.f - q; w2Sin = q; }
            break;
        }
        case 3: {
            const double fc = std::min(0.45 * osr, knobCutoffHz((int) c21));
            const double gg = std::tan(kPi * fc * inv), kk = 2.0 * std::pow(1.0 - c22 / 127.0, 1.4) + 0.08;
            noiseA2 = (float) (1.0 / (1.0 + gg * (gg + kk))); noiseK2 = (float) kk; nzG2 = (float) gg;
            noiseG2 = 0.8f * (float) std::min(4.0, std::max(1.0, std::sqrt(8000.0 / fc)));
            break;
        }
        default: break;
    }
    // mix
    {
        const float g1 = P.s[PatchParams::S_bal1], g2 = P.s[PatchParams::S_bal2];
        const float n = std::sqrt(g1 * g1 + g2 * g2);
        mix1 = n > 1e-6f ? 0.7f * g1 / n : 0.f;
        mix2 = n > 1e-6f ? 0.7f * g2 / n : 0.f;
    }
    ringOn = P.ring; syncOn = P.sync;
    {
        float xm = P.s[PatchParams::S_xmod];
        if (P.od == 2) xm += penvSemis / 12.f + l1 * P.s[PatchParams::S_ol] / 12.f;
        xmodOct = (kind1 == 0 || kind1 == 2) ? 0.f : clampf(xm, 0.f, 1.f) * 1.5f;
    }

    // ---- envelopes (times may change while a note plays) ----
    aenv.setTimes(sr, P.aA, P.aD, P.aS, P.aR);
    fenv.setTimes(sr, P.fA, P.fD, P.fS, P.fR);

    // ---- filter ----
    {
        float oct = P.s[PatchParams::S_cut];
        oct += P.s[PatchParams::S_fkey] * (float) ((noteSemis + P.s[PatchParams::S_kshift] - 60.0) / 12.0);
        oct += P.s[PatchParams::S_fenv] * fenv.level;
        oct += l1 * P.s[PatchParams::S_flfo];
        oct += l2 * P.s[PatchParams::S_m2f];
        if (P.velMorph) oct += (vel - 100) / 127.f * 1.5f;           // own interpretation of the velocity switch
        double hz = std::exp2((double) oct);
        hz = std::min(std::min(20000.0, 0.49 * sr), std::max(20.0, hz));
        ftype = P.ft; p24 = P.f24;
        const double gT = std::tan(kPi * hz / sr);
        const double r = P.s[PatchParams::S_res];
        const double kT = p24 ? ladderK(r) : svfK(r);
        ramp(g, dg, gT); ramp(kf, dkf, kT);
        filt.guard();
    }

    // ---- amp, pan ----
    {
        float trem = 1.f;
        if (P.ap == 0) { const float d = P.s[PatchParams::S_al]; trem *= 1.f - std::fabs(d) * lfoFade * (1.f - (d >= 0.f ? lfoRaw : -lfoRaw)) * 0.5f; }
        {
            const float d = P.s[PatchParams::S_m2a] * C.mod, x = C.l2[part];
            trem *= 1.f - std::fabs(d) * (1.f - (d >= 0.f ? x : -x)) * 0.5f;
        }
        float pan = 0.f;
        if (P.ap == 1) pan = clampf(P.s[PatchParams::S_al] * lfoRaw, -1.f, 1.f);   // LFO 1 FADE has no effect on Auto Pan (manual p.62)
        else if (P.ap == 2) pan = clampf(P.s[PatchParams::S_al], -1.f, 1.f);
        if (C.parallelOut) pan = part == 1 ? -1.f : 1.f;
        const float velG = std::pow(vel / 127.f, 0.6f);
        const float aT = P.s[PatchParams::S_level] * velG * uniGain * trem;
        const float plT = panL(pan), prT = panR(pan);
        if (snap) { ampG = aT; dAmpG = 0.f; pl = plT; dpl = 0.f; pr = prT; dpr = 0.f; }
        else { dAmpG = (aT - ampG) * invCB; dpl = (plT - pl) * invCB; dpr = (prT - pr) * invCB; }
    }
}

// Generic band limited wave: class 0 pulse, 1 saw/triangle/sine morph, 2 triangle with phase warp.
inline float Voice::waveTick(double& ph, BlepOut& bo, double inc, int cls, float pw, float wSaw, float wTri, float wSin,
                             float triA, bool doSync, float syncT, bool& wrapped, float& wrapT) {
    auto naive = [&](double p) -> float {
        switch (cls) {
            case 0: return (p < pw ? kPulseAmp : -kPulseAmp) - kPulseAmp * (2.f * pw - 1.f);
            case 1: return wSaw * (float) (2.0 * p - 1.0) + wTri * triWave(p) + (wSin != 0.f ? wSin * std::sin((float) (kTwoPi * p)) : 0.f);
            default: { double q = p + triA * std::sin(kTwoPi * p); q -= std::floor(q); return triWave(q); }
        }
    };
    wrapped = false; wrapT = 0.f;
    const double pOld = ph;
    float cur;
    if (doSync) {
        double pb = pOld + inc * (1.0 - syncT); pb -= std::floor(pb);
        const float vb = naive(pb), va = naive(0.0);
        ph = inc * syncT; if (ph >= 1.0) ph -= 1.0;
        cur = naive(ph);
        bo.step(va - vb, syncT, cur);
        return bo.push(cur);
    }
    ph += inc;
    if (ph >= 1.0) { ph -= 1.0; wrapped = true; wrapT = (float) (ph / inc); }
    cur = naive(ph);
    if (cls == 0) {
        if (wrapped) bo.step(2.f * kPulseAmp, wrapT, cur);
        else if (pOld < pw && ph >= pw) bo.step(-2.f * kPulseAmp, (float) ((ph - pw) / inc), cur);
    } else if (cls == 1 && wrapped && wSaw != 0.f) {
        bo.step(-2.f * wSaw, wrapT, cur);
    }
    return bo.push(cur);
}

inline float Voice::oscStep() {
    float y1 = 0.f; bool mWrap = false; float mT = 0.f;
    double i1 = inc1;
    if (xmodOct != 0.f) i1 *= fastExp2(xmodOct * lastY2);
    if (i1 > 0.49) i1 = 0.49;
    switch (kind1) {
        case 0: {
            float sum = 0.f;
            for (int k = 0; k < 7; ++k) {
                double p = phSaw[k] + incSaw[k];
                if (p >= 1.0) { p -= 1.0; if (k == 3) { mWrap = true; mT = (float) (p / incSaw[3]); } }
                phSaw[k] = p;
                sum += ssGain[k] * (float) (2.0 * p - 1.0);
            }
            ssHp += ssHpA * (sum - ssHp);
            y1 = sum - ssHp;
            break;
        }
        case 1: y1 = waveTick(ph1, bo1, i1, 2, 0.5f, 0.f, 1.f, 0.f, triModA, false, 0.f, mWrap, mT); break;
        case 2: {
            const float x = rng.bi();
            const float v3 = x - nz1[1];
            const float v1 = noiseA1 * nz1[0] + noiseA1 * nzG1 * v3;
            const float v2 = nz1[1] + nzG1 * noiseA1 * nz1[0] + nzG1 * nzG1 * noiseA1 * v3;
            nz1[0] = 2.f * v1 - nz1[0]; nz1[1] = 2.f * v2 - nz1[1];
            nz1[0] = sat(nz1[0]);
            y1 = v2 * noiseG1;
            break;
        }
        case 3: {
            ph1 += i1;
            if (ph1 >= 1.0) { ph1 -= 1.0; mWrap = true; mT = (float) (ph1 / i1); }
            const float y = std::sin((float) (kTwoPi * ph1) + fbBeta * fbState);
            fbState += fbLp * (y - fbState);
            y1 = y;
            break;
        }
        case 4: y1 = waveTick(ph1, bo1, i1, 0, pw1, 0.f, 0.f, 0.f, 0.f, false, 0.f, mWrap, mT); break;
        default: y1 = waveTick(ph1, bo1, i1, 1, 0.5f, w1Saw, w1Tri, w1Sin, 0.f, false, 0.f, mWrap, mT); break;
    }
    float y2 = 0.f;
    if (mix2 > 0.f || ringOn || xmodOct != 0.f) {
        bool w2; float t2;
        switch (kind2) {
            case 0: y2 = waveTick(ph2, bo2, inc2, 0, pw2, 0.f, 0.f, 0.f, 0.f, syncOn && mWrap, mT, w2, t2); break;
            case 3: {
                const float x = rng.bi();
                const float v3 = x - nz2[1];
                const float v1 = noiseA2 * nz2[0] + noiseA2 * nzG2 * v3;
                const float v2 = nz2[1] + nzG2 * noiseA2 * nz2[0] + nzG2 * nzG2 * noiseA2 * v3;
                nz2[0] = 2.f * v1 - nz2[0]; nz2[1] = 2.f * v2 - nz2[1];
                nz2[0] = sat(nz2[0]);
                y2 = v2 * noiseG2;
                break;
            }
            default: y2 = waveTick(ph2, bo2, inc2, 1, 0.5f, w2Saw, w2Tri, w2Sin, 0.f, syncOn && mWrap, mT, w2, t2); break;
        }
    }
    lastY2 = y2;
    if (ringOn) y2 *= y1;                    // ring modulator output replaces OSC 2 in the balance. Unverified placement.
    return mix1 * y1 + mix2 * y2;
}

void Voice::render(int n, float* L, float* R) {
    const HalfbandCoefs& h = *hc;
    for (int i = 0; i < n; ++i) {
        for (int k = 0; k < 7; ++k) incSaw[k] += dSaw[k];
        inc1 += d1; inc2 += d2; g += dg; kf += dkf; ampG += dAmpG; pl += dpl; pr += dpr;
        const float ae = aenv.tick();
        fenv.tick();
        float x;
        if (osFactor == 2) { const float a = oscStep(); const float b = oscStep(); x = dec.push(a, b, h); }
        else x = oscStep();
        // Tiny noise seed so a fully resonant filter starts to ring on its own (about -112 dBFS).
        float y = filt.process(x + 2.5e-6f * rng.bi(), g, kf, ftype, p24);
        float a = ae + declick;
        if (declick > 0.f) { declick -= declickStep; if (declick < 0.f) declick = 0.f; }
        float o = y * a * ampG;
        if (st == VState::Fading) {
            fadeG -= 1.f / 64.f;
            if (fadeG <= 0.f) { st = VState::Idle; aenv.kill(); fenv.kill(); return; }
            o *= fadeG;
        }
        L[i] += o * pl; R[i] += o * pr;
        if (aenv.stage == Env::Idle && declick <= 0.f && st != VState::Idle) { st = VState::Idle; return; }
    }
}

}  // namespace jpurrr
