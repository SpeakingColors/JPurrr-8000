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
    { 1, 0.12f, { 0, 0 }, { 0, 0 }, 0, 0, 0.5f, 7.0f, 0.60f,               0, 0, 0, 1, 0.25f, 0, 1.f },   // FLANGER SLOW
    { 1, 0.22f, { 0, 0 }, { 0, 0 }, 0, 0, 0.25f, 9.0f, 0.80f,              0, 0, 0, 1, 0.25f, 0, 1.f },   // FLANGER DEEP
    { 1, 0.90f, { 0, 0 }, { 0, 0 }, 0, 0, 0.5f, 3.5f, 0.55f,               0, 0, 0, 1, 0.25f, 0, 1.f },   // FLANGER FAST
    { 2, 0.15f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, 0.65f,                     8, 250, 2500, 1, 0.25f, 0, 1.f },   // DEEP PHASING SLW
    { 2, 0.06f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, 0.78f,                     6, 120, 2000, 1, 0.15f, 0, 1.f },  // JET PHASING
    { 2, 0.45f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, 0.50f,                     6, 300, 2800, 1, 0.50f, 0.35f, 1.f }, // TWISTING
    { 2, 0.03f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, 0.72f,                     8, 860, 900, 1.45f, 0.50f, 0, 1.f }, // FREEZE PHASE 1
    { 2, 0.025f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, -0.72f,                   10, 420, 440, 2.6f, 0.50f, 0, 1.f },  // FREEZE PHASE 2
    { 3, 0.f, { 0, 0 }, { 0, 0 }, 0, 0, 0, 0, 0,                           0, 0, 0, 1, 0, 0,    1.f },   // DISTORTION
};

// Overall output trim at full level, from the pink-ish multi sine level test in tests/test_soundfx.cpp
// (the trim fades in with the level knob, so level 0 stays bit exact). Brings every type within about 0.5 dB.
static const float kTrim[13] = { 1.07f, 1.08f, 1.084f, 1.04f, 1.05f, 1.11f, 1.076f, 0.86f, 0.80f, 0.90f, 0.83f, 1.11f, 1.f };

// ---- distortion waveshaper

static const double kBias = 0.1;   // small asymmetry for some even harmonics; the DC it makes is removed after
static inline double logcosh(double x) { double a = std::fabs(x); return a + std::log1p(std::exp(-2.0 * a)) - 0.69314718055994531; }

// ---- MultiFx

static const double kPhaserSplitHz = 2800.0;
static const double kDistLpHz = 8000.0;

MultiFx::MultiFx() { des_ = &kDesign[0]; }

void MultiFx::prepare(double sr) {
    sr_ = sr;
    int cap = (int)(0.08 * sr) + 64;
    line_[0].init(cap); line_[1].init(cap);
    msToSamp_ = (float)(sr * 0.001);
    rateK_ = (float)(1.0 - std::exp(-1.0 / (0.1 * sr)));
    powK_ = (float)(1.0 - std::exp(-1.0 / (0.010 * sr)));
    powKs_ = (float)(1.0 - std::exp(-1.0 / (0.060 * sr)));
    kSmooth_ = (float)(1.0 - std::exp(-1.0 / (0.0015 * sr)));
    for (int c = 0; c < 2; ++c) {
        for (int k = 0; k < 3; ++k) { const HalfbandCoefs* hc = k == 0 ? &kHbA : &kHbB; up_[c][k].c = hc; dn_[c][k].c = hc; dry_[c][k].c = hc; }
        dLp_[c][0].lowpass(8.0 * sr, kDistLpHz, 0.5411961); dLp_[c][1].lowpass(8.0 * sr, kDistLpHz, 1.3065630); dDc_[c].set(25.0, sr);
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
            for (int k = 0; k < 2; ++k) { xoLp_[c][k].lowpass(sr_, kPhaserSplitHz, 0.70710678); xoHp_[c][k].highpass(sr_, kPhaserSplitHz, 0.70710678); }
        }
    }
}

void MultiFx::reset() {
    line_[0].clear(); line_[1].clear();
    for (int c = 0; c < 2; ++c) {
        wetLp_[c].clear(); wetHp_[c].clear(); fbLp_[c].clear(); fbHp_[c].clear();
        for (int k = 0; k < 2; ++k) { xoLp_[c][k].clear(); xoHp_[c][k].clear(); }
        for (int s = 0; s < kMaxStages; ++s) apS_[c][s] = 0.f;
        fbPrev_[c] = 0.f;
        for (int k = 0; k < 3; ++k) { up_[c][k].clear(); dn_[c][k].clear(); dry_[c][k].clear(); }
        dLp_[c][0].clear(); dLp_[c][1].clear(); dDc_[c].clear(); adaaX_[c] = adaaF_[c] = 0.0;
    }
    pIn_ = pOut_ = pOutS_ = 0.f; kS_ = 1.f;
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
        float g = mixNorm(lev[i]) * (1.f + lev[i] * (kTrim[type_] - 1.f));
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
    const float wetScale = std::sqrt(std::max(0.f, 1.f - d.fb * d.fb)) * d.wetGain;   // average wet power 1 despite the feedback
    for (int i = 0; i < n; ++i) {
        float ph = advanceLfo();
        float g = mixNorm(lev[i]) * (1.f + lev[i] * (kTrim[type_] - 1.f));
        for (int c = 0; c < 2; ++c) {
            float x = in[c][i];
            float u = 0.5f + 0.5f * std::sin(kTwoPi * (ph + (c ? d.lr : 0.f)));
            float dl = d.dMin * std::exp2(l2 * u) * msToSamp_;
            float y = line_[c].read(std::max(dl, DelayBuf::kMinDelay));
            float f = fbHp_[c].process(fbLp_[c].process(y)) * d.fb;
            line_[c].push(flush(x + loopSat(f)));
            out[c][i] = (x + lev[i] * y * wetScale) * g;
        }
    }
}

// Phaser family: first order allpass chain, swept exponentially, damped feedback.
// The feedback loop is solved without a unit delay (the chain is affine in its input, so the loop closes in
// one division), which keeps the resonance independent of the sample rate.
// Freeze types hardly move: the notches sit still and the two channels sit at different places.
void MultiFx::runPhaser(const float* in[2], float* out[2], const float* lev, int n) {
    const ModDesign& d = *des_;
    const float l2 = std::log2(d.fHi / d.fLo);
    const float fmax = (float)(0.45 * sr_);
    const float piOverSr = (float)(kPi / sr_);
    const float hpk = fbHp_[0].a, oneMinusK = 1.f - hpk;
    const float wetScale = std::sqrt(std::max(0.f, 1.f - d.fb * d.fb)) * d.wetGain;   // keeps the average wet power at 1
    for (int i = 0; i < n; ++i) {
        float ph = advanceLfo();
        float g = (1.f + lev[i] * (kTrim[type_] - 1.f)) / (1.f + lev[i]);      // allpass sums are in phase outside the notches: peak normalised
        float wx = std::min(1.f, lev[i] * 20.f);
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
            float* s = apS_[c];
            // The chain only sees the low band. A first order allpass built with the bilinear transform has the
            // wrong phase near Nyquist, so above the split (2.8 kHz) the wet path keeps the input's own phase: this is what an analog
            // chain does there, and it makes the sound the same at every sample rate.
            float lo = xoLp_[c][1].process(xoLp_[c][0].process(x));
            float hi = xoHp_[c][1].process(xoHp_[c][0].process(x));
            // free response of the chain (zero input) and its direct gain a^N
            float v = 0.f, aN = 1.f;
            for (int k = 0; k < d.stages; ++k) { v = a * v + s[k]; aN *= a; }
            float hs = fbHp_[c].s, q = aN * d.fb * oneMinusK;
            float y = (aN * lo + v - q * hs) / (1.f - q);
            float xin = lo + d.fb * oneMinusK * (y - hs);
            fbHp_[c].s = hs + hpk * (y - hs);
            v = xin;
            for (int k = 0; k < d.stages; ++k) { float yy = a * v + s[k]; s[k] = flush(v - a * yy); v = yy; }
            // The dry part moves to the split's own allpass (lo + hi) as soon as the level leaves zero, so wet and dry stay in phase above the split.
            out[c][i] = (x + wx * (lo + hi - x) + lev[i] * (v * wetScale + hi)) * g;
        }
    }
    for (int c = 0; c < 2; ++c) for (int k = 0; k < 2; ++k) { xoLp_[c][k].flushState(); xoHp_[c][k].flushState(); }
}

// Distortion (JP-8080 style 13th type, our own version): 8x oversampled tanh with a small asymmetry and first order
// ADAA, drive tied to the level knob (6 dB at 0, 26 dB at full), post low pass 4th order at 8 kHz (run at 8x so its shape does
// not depend on the sample rate; it also takes the harmonics that would fold around Nyquist), DC blocked.
// Level match: gain = sqrt(input power / wet power). Input power uses a 10 ms follower; wet power uses the larger of
// a 10 ms and a 60 ms follower, so the wet side never reads low right after an onset. Linked over both channels.
// The level knob also mixes dry and wet with an equal power law; the dry part runs through the same half band filters
// so both sides have the same delay.
void MultiFx::runDistortion(const float* in[2], float* out[2], const float* lev, int n) {
    float g1 = std::pow(10.f, (6.f + 20.f * lev[n - 1]) / 20.f);
    float g0 = gPrev_ > 0.f ? gPrev_ : g1;
    gPrev_ = g1;
    const double tb = std::tanh(kBias);
    for (int i = 0; i < n; ++i) {
        double g = g0 + (g1 - g0) * ((float)(i + 1) / (float)n);
        float m = lev[i], y[2], dryAl[2], pi = 0.f, po = 0.f;
        for (int c = 0; c < 2; ++c) {
            float x = in[c][i];
            float s1[2], s2[4], s3[8], w[8], d4[4], d2[2];
            up_[c][0].process(x, s1[0], s1[1]);
            for (int a = 0; a < 2; ++a) up_[c][1].process(s1[a], s2[2 * a], s2[2 * a + 1]);
            for (int a = 0; a < 4; ++a) up_[c][2].process(s2[a], s3[2 * a], s3[2 * a + 1]);
            for (int a = 0; a < 8; ++a) {
                double xs = g * (double)s3[a], x0 = adaaX_[c];
                double f1 = logcosh(xs + kBias), dx = xs - x0, r;
                if (std::fabs(dx) > 1e-5) r = (f1 - adaaF_[c]) / dx - tb;
                else r = std::tanh(0.5 * (xs + x0) + kBias) - tb;
                adaaX_[c] = xs; adaaF_[c] = f1;
                w[a] = dLp_[c][1].process(dLp_[c][0].process((float)r));
            }
            for (int a = 0; a < 4; ++a) d4[a] = dn_[c][2].process(w[2 * a], w[2 * a + 1]);
            for (int a = 0; a < 2; ++a) d2[a] = dn_[c][1].process(d4[2 * a], d4[2 * a + 1]);
            y[c] = dDc_[c].process(dn_[c][0].process(d2[0], d2[1]));
            // dry path with the same delay
            float q4[4], q2[2];
            for (int a = 0; a < 4; ++a) q4[a] = dry_[c][2].process(s3[2 * a], s3[2 * a + 1]);
            for (int a = 0; a < 2; ++a) q2[a] = dry_[c][1].process(q4[2 * a], q4[2 * a + 1]);
            dryAl[c] = dry_[c][0].process(q2[0], q2[1]);
            pi += x * x; po += y[c] * y[c];
        }
        pIn_ += (0.5f * pi - pIn_) * powK_;
        pOut_ += (0.5f * po - pOut_) * powK_;
        pOutS_ += (0.5f * po - pOutS_) * powKs_;
        pIn_ = flush(pIn_); pOut_ = flush(pOut_); pOutS_ = flush(pOutS_);
        float k = std::sqrt((pIn_ + 1e-14f) / (std::max(pOut_, pOutS_) + 1e-14f));
        k = std::min(k, 30.f);
        kS_ += (k - kS_) * kSmooth_;      // the follower ripples at twice the signal frequency; this keeps that ripple from folding back
        k = kS_;
        const float wd = std::cos(m * 1.57079633f), ww = std::sin(m * 1.57079633f);
        for (int c = 0; c < 2; ++c) {
            float x = in[c][i];
            float xc = x + m * (dryAl[c] - x);                   // dry part moves to the wet path's delay as the level rises
            out[c][i] = m == 0.f ? x : wd * xc + ww * flush(y[c] * k);
        }
    }
    for (int c = 0; c < 2; ++c) { dLp_[c][0].flushState(); dLp_[c][1].flushState(); }
}

} // namespace sfx
} // namespace jpurrr
