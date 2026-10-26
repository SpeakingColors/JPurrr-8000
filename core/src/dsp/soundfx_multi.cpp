// SoundFx multi-FX engine. All 13 types are our own approximations of the JP-8000 / JP-8080 effect names.
// The Roland algorithms are not public; every number in the design table below is a musical choice, not a
// measurement of hardware. Unverified.
#include "soundfx_multi.h"
#include <algorithm>

namespace jpurrr {
namespace sfx {

struct ModDesign {
    int kind;            // 0 chorus, 1 flanger, 2 phaser, 3 distortion
    float rate;          // LFO rate in Hz
    // chorus: two voices per channel, base delay and sweep depth in ms, wet filter corners
    float base[2], depth[2], lpHz, hpHz;
    // flanger: exponential sweep between dMin and dMax (ms), feedback
    float dMin, dMax, fb;
    // phaser: allpass stages, exponential sweep fLo..fHi (Hz), right channel scale, L/R LFO offset (cycles), twist
    int stages; float fLo, fHi, rScale, lr, twist;
    float wetGain;       // level compensation of the wet path (calibrated against noise, see tests)
};

static const ModDesign kDesign[13] = {
    // kind rate    base         depth        lp     hp    dMin dMax fb     stages fLo   fHi   rScale lr    twist wet
    { 0, 0.30f, { 9.0f, 13.0f }, { 2.0f, 2.4f }, 7500, 120, 0, 0, 0,        0, 0, 0, 1, 0, 0,    1.f },   // SUPER CHORUS SLW
    { 0, 0.70f, { 8.0f, 11.5f }, { 1.6f, 1.9f }, 8500, 120, 0, 0, 0,        0, 0, 0, 1, 0, 0,    1.f },   // SUPER CHORUS MID
    { 0, 1.60f, { 7.0f, 10.0f }, { 0.9f, 1.1f }, 9000, 120, 0, 0, 0,        0, 0, 0, 1, 0, 0,    1.f },   // SUPER CHORUS FST
    { 0, 0.90f, { 10.0f, 14.5f }, { 1.3f, 1.6f }, 0,    80, 0, 0, 0,        0, 0, 0, 1, 0, 0,    1.f },   // SUPER CHORUS CLR (no wet low pass)
    { 1, 0.12f, { 0, 0 }, { 0, 0 }, 0, 0, 0.5f, 7.0f, 0.60f,               0, 0, 0, 1, 0.25f, 0, 0.8f },   // FLANGER SLOW
    { 1, 0.22f, { 0, 0 }, { 0, 0 }, 0, 0, 0.25f, 9.0f, 0.80f,              0, 0, 0, 1, 0.25f, 0, 0.6f },   // FLANGER DEEP
    { 1, 0.90f, { 0, 0 }, { 0, 0 }, 0, 0, 0.5f, 3.5f, 0.55f,               0, 0, 0, 1, 0.25f, 0, 0.8f },   // FLANGER FAST
    { 2, 0.15f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, 0.65f,                     8, 250, 3500, 1, 0.25f, 0, 1.f },   // DEEP PHASING SLW
    { 2, 0.06f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, 0.82f,                     6, 120, 9000, 1, 0.15f, 0, 0.7f },  // JET PHASING
    { 2, 0.45f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, 0.50f,                     6, 300, 4200, 1, 0.50f, 0.35f, 1.f }, // TWISTING
    { 2, 0.03f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, 0.72f,                     8, 820, 1000, 1.45f, 0.50f, 0, 0.9f }, // FREEZE PHASE 1
    { 2, 0.025f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, -0.72f,                   10, 390, 470, 2.6f, 0.50f, 0, 0.9f },  // FREEZE PHASE 2
    { 3, 0.f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, 0,                           0, 0, 0, 1, 0, 0,    1.f },   // DISTORTION
};

// ---- distortion waveshaper and its level match table

static const float kBias = 0.1f;   // small asymmetry for some even harmonics; the DC it makes is removed after
static inline float shaper(float u) { return std::tanh(u + kBias) - std::tanh(kBias); }

// ---- MultiFx

MultiFx::MultiFx() { des_ = &kDesign[0]; }

void MultiFx::prepare(double sr) {
    sr_ = sr;
    int cap = (int)(0.08 * sr) + 64;
    line_[0].init(cap); line_[1].init(cap);
    msToSamp_ = (float)(sr * 0.001);
    rateK_ = (float)(1.0 - std::exp(-1.0 / (0.1 * sr)));
    powK_ = (float)(1.0 - std::exp(-1.0 / (0.010 * sr)));
    powKs_ = (float)(1.0 - std::exp(-1.0 / (0.060 * sr)));
    for (int c = 0; c < 2; ++c) {
        upA_[c].c = &kHbA; dnA_[c].c = &kHbA; upB_[c].c = &kHbB; dnB_[c].c = &kHbB;
        dLp_[c].lowpass(sr, 9000.0, 0.7071); dDc_[c].set(25.0, sr);
        fbLp_[c].set(6000.0, sr); wetHp_[c].set(120.0, sr);
    }
    setType(type_);
    reset();
}

void MultiFx::setType(int t) {
    type_ = std::min(std::max(t, 0), kNumTypes - 1);
    des_ = &kDesign[type_];
    for (int c = 0; c < 2; ++c) {
        if (des_->kind == 0) {
            wetHp_[c].set(des_->hpHz, sr_);
            if (des_->lpHz > 0.f) wetLp_[c].lowpass(sr_, des_->lpHz, 0.7071); else wetLp_[c].setIdentity();
        } else if (des_->kind == 1) {
            fbHp_[c].set(100.0, sr_); fbLp_[c].set(6000.0, sr_);
        } else if (des_->kind == 2) {
            fbHp_[c].set(150.0, sr_);
        }
    }
}

void MultiFx::reset() {
    line_[0].clear(); line_[1].clear();
    for (int c = 0; c < 2; ++c) {
        wetLp_[c].clear(); wetHp_[c].clear(); fbLp_[c].clear(); fbHp_[c].clear();
        for (int s = 0; s < kMaxStages; ++s) apS_[c][s] = 0.f;
        fbPrev_[c] = 0.f;
        upA_[c].clear(); upB_[c].clear(); dnA_[c].clear(); dnB_[c].clear(); dLp_[c].clear(); dDc_[c].clear();
    }
    pIn_ = pOut_ = pOutS_ = 0.f;
    phase_ = 0.0; rateCur_ = rateOverride_ > 0.0 ? (float)rateOverride_ : des_->rate;
    gPrev_ = 0.f;
}

float MultiFx::advanceLfo() {
    float tgt = rateOverride_ > 0.0 ? (float)rateOverride_ : des_->rate;
    rateCur_ += (tgt - rateCur_) * rateK_;
    phase_ += (double)rateCur_ / sr_;
    if (phase_ >= 1.0) phase_ -= 1.0;
    return (float)phase_;
}

void MultiFx::process(const float* inL, const float* inR, float* outL, float* outR, const float* lev, int n) {
    const float* in[2] = { inL, inR };
    float* out[2] = { outL, outR };
    switch (des_->kind) {
        case 0: runChorus(in, out, lev, n); break;
        case 1: runFlanger(in, out, lev, n); break;
        case 2: runPhaser(in, out, lev, n); break;
        default: runDistortion(in, out, lev, n); break;
    }
}

static inline float mixNorm(float lev) { return 1.f / std::sqrt(1.f + lev * lev); }
static const float kTwoPi = 6.28318530717958647692f;

// Super chorus: two modulated taps per channel, taps and sides in quadrature so the image is wide.
void MultiFx::runChorus(const float* in[2], float* out[2], const float* lev, int n) {
    const ModDesign& d = *des_;
    static const float off[2][2] = { { 0.f, 0.25f }, { 0.5f, 0.75f } };
    for (int i = 0; i < n; ++i) {
        float ph = advanceLfo();
        float ds = std::min(1.f, std::max(0.25f, d.rate / std::max(rateCur_, 0.01f)));   // keep pitch wobble sane at fast sync rates
        float g = mixNorm(lev[i]);
        for (int c = 0; c < 2; ++c) {
            float x = in[c][i], v[2];
            for (int k = 0; k < 2; ++k) {
                float s = std::sin(kTwoPi * (ph + off[c][k]));
                float dl = (d.base[k] + d.depth[k] * ds * s) * msToSamp_;
                v[k] = line_[c].read(std::max(dl, DelayBuf::kMinDelay));
            }
            line_[c].push(x);
            float w = 0.70710678f * (v[0] + v[1]);
            w = wetHp_[c].process(w);
            w = wetLp_[c].process(w);
            w = flush(w);
            out[c][i] = (x + lev[i] * w * d.wetGain) * g;
        }
    }
    for (int c = 0; c < 2; ++c) { wetLp_[c].flushState(); }
}

// Flanger: one swept short delay with damped feedback. Right channel runs a quarter cycle behind.
void MultiFx::runFlanger(const float* in[2], float* out[2], const float* lev, int n) {
    const ModDesign& d = *des_;
    const float l2 = std::log2(d.dMax / d.dMin);
    for (int i = 0; i < n; ++i) {
        float ph = advanceLfo();
        float g = mixNorm(lev[i]);
        for (int c = 0; c < 2; ++c) {
            float x = in[c][i];
            float u = 0.5f + 0.5f * std::sin(kTwoPi * (ph + (c ? d.lr : 0.f)));
            float dl = d.dMin * std::exp2(l2 * u) * msToSamp_;
            float y = line_[c].read(std::max(dl, DelayBuf::kMinDelay));
            float f = fbHp_[c].process(fbLp_[c].process(y)) * d.fb;
            line_[c].push(flush(x + loopSat(f)));
            out[c][i] = (x + lev[i] * y * d.wetGain) * g;
        }
    }
}

// Phaser family: first order allpass chain, swept exponentially, damped feedback.
// Freeze types hardly move: the notches sit still and the two channels sit at different places.
void MultiFx::runPhaser(const float* in[2], float* out[2], const float* lev, int n) {
    const ModDesign& d = *des_;
    const float l2 = std::log2(d.fHi / d.fLo);
    const float fmax = (float)(0.45 * sr_);
    const float piOverSr = (float)(kPi / sr_);
    for (int i = 0; i < n; ++i) {
        float ph = advanceLfo();
        float g = mixNorm(lev[i]);
        for (int c = 0; c < 2; ++c) {
            float x = in[c][i];
            float p = ph + (c ? d.lr : 0.f);
            float u = std::sin(kTwoPi * p);
            if (d.twist > 0.f) u = (u + d.twist * std::sin(kTwoPi * (3.f * p) + 1.f)) / (1.f + d.twist);
            float f = d.fLo * std::exp2(l2 * (0.5f + 0.5f * u));
            if (c) f *= d.rScale;
            f = std::min(f, fmax);
            float t = std::tan(f * piOverSr);
            float a = (1.f - t) / (1.f + t);
            float v = x + d.fb * fbHp_[c].process(fbPrev_[c]);
            v = loopSat(v);
            float* s = apS_[c];
            for (int k = 0; k < d.stages; ++k) { float y = a * v + s[k]; s[k] = flush(v - a * y); v = y; }
            fbPrev_[c] = flush(v);
            out[c][i] = (x + lev[i] * v * d.wetGain) * g;
        }
    }
}

// Distortion (JP-8080 style 13th type, our own version): 4x oversampled tanh with a small asymmetry,
// drive tied to the level knob (6 dB at 0, 32 dB at full), post low pass at 9 kHz, DC blocked.
// Level match: gain = sqrt(input power / wet power). Input power uses a 10 ms follower; wet power uses the larger of
// a 10 ms and a 60 ms follower, so the wet side never reads low right after an onset. Linked over both channels. The level knob also crossfades dry to wet.
void MultiFx::runDistortion(const float* in[2], float* out[2], const float* lev, int n) {
    float g1 = std::pow(10.f, (6.f + 26.f * lev[n - 1]) / 20.f);
    float g0 = gPrev_ > 0.f ? gPrev_ : g1;
    gPrev_ = g1;
    for (int i = 0; i < n; ++i) {
        float g = g0 + (g1 - g0) * ((float)(i + 1) / (float)n);
        float m = lev[i], y[2], pi = 0.f, po = 0.f;
        for (int c = 0; c < 2; ++c) {
            float x = in[c][i];
            float a0, a1, b00, b01, b10, b11;
            upA_[c].process(x, a0, a1);
            upB_[c].process(a0, b00, b01);
            upB_[c].process(a1, b10, b11);
            float h0 = dnB_[c].process(shaper(g * b00), shaper(g * b01));
            float h1 = dnB_[c].process(shaper(g * b10), shaper(g * b11));
            y[c] = dLp_[c].process(dDc_[c].process(dnA_[c].process(h0, h1)));
            pi += x * x; po += y[c] * y[c];
        }
        pIn_ += (0.5f * pi - pIn_) * powK_;
        pOut_ += (0.5f * po - pOut_) * powK_;
        pOutS_ += (0.5f * po - pOutS_) * powKs_;
        pIn_ = flush(pIn_); pOut_ = flush(pOut_); pOutS_ = flush(pOutS_);
        float k = std::sqrt((pIn_ + 1e-14f) / (std::max(pOut_, pOutS_) + 1e-14f));
        k = std::min(k, 30.f);
        for (int c = 0; c < 2; ++c) {
            float x = in[c][i];
            out[c][i] = x + (flush(y[c] * k) - x) * m;
        }
    }
    for (int c = 0; c < 2; ++c) dLp_[c].flushState();
}

} // namespace sfx
} // namespace jpurrr
