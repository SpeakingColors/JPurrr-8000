// DSP building blocks for the JPurrr-8000 sound engine. Header only, pure C++17, no allocation.
// Original code: textbook math (zero delay feedback filters after Zavalishin's published method,
// polyBLEP after Valimaki et al., Kaiser windowed half band) written for this project.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace jpurrr {
namespace dsp {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

// ---------- small math ----------
// Soft saturation, close to tanh, unity small signal gain, exactly +-1 beyond +-3.
inline float sat(float x) {
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}
inline double satD(double x) {
    if (x > 3.0) return 1.0;
    if (x < -3.0) return -1.0;
    const double x2 = x * x;
    return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}
// Output stage: linear below the knee, then a smooth approach to +-1.
inline float softLimit(float x) {
    const float knee = 0.8f;
    const float a = std::fabs(x);
    if (a <= knee) return x;
    const float t = (a - knee) * 5.f;                 // 1 / (1 - knee)
    return std::copysign(knee + (1.f - knee) * sat(t), x);
}
// 2^x for |x| < 8, relative error below 1e-4. For per sample use.
inline float fastExp2(float x) {
    x = std::min(8.f, std::max(-8.f, x));
    const float fl = std::floor(x);
    const float f = x - fl;
    const float p = 1.f + f * (0.69314718f + f * (0.24022651f + f * (0.05550411f + f * 0.00961813f)));
    return std::ldexp(p, (int) fl);
}
inline float finiteOr(float x, float alt) { return std::isfinite(x) ? x : alt; }

// ---------- noise ----------
struct Rng {
    uint32_t s = 0x9E3779B9u;
    void seed(uint32_t v) { s = v ? v : 0x9E3779B9u; for (int i = 0; i < 4; ++i) next(); }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }        // [0,1)
    float bi() { return uni() * 2.f - 1.f; }                                   // [-1,1)
};

// ---------- polyBLEP ----------
// A band limited step: the oscillator sample after a step of height J that happened t samples ago
// (0 <= t < 1) gets J/2 * blepAfter(t); the sample before it gets J/2 * blepBefore(t).
inline float blepAfter(float t) { return 2.f * t - t * t - 1.f; }
inline float blepBefore(float t) { return t * t; }

// One sample delayed output so a step found in this sample can also correct the previous one.
struct BlepOut {
    float prev = 0.f;
    // Adds a step of height j that happened tfrac samples before the end of the current sample interval start.
    inline void step(float j, float tfrac, float& cur) {
        prev += j * 0.5f * blepBefore(tfrac);
        cur += j * 0.5f * blepAfter(tfrac);
    }
    inline float push(float cur) { const float o = prev; prev = cur; return o; }
    void reset() { prev = 0.f; }
};

// ---------- half band decimator, 2:1 ----------
// Linear phase Kaiser windowed half band FIR. Latency kHalfbandLatency host samples.
constexpr int kHbL = 24;                        // odd taps per side
constexpr int kHbRing = 64;                     // >= 2 * kHbL
constexpr int kHalfbandLatency = kHbL - 1;
struct HalfbandCoefs {
    float c[kHbL];
    float centre = 0.5f;
    static double bessel0(double x) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; }
    HalfbandCoefs() {
        const double beta = 10.0;
        const int half = 2 * kHbL;                  // window half length in taps
        double h[kHbL]; double sum = 0.5;
        for (int k = 1; k <= kHbL; ++k) {
            const int m = 2 * k - 1;
            const double sinc = ((k & 1) ? 1.0 : -1.0) / (kPi * m);       // sin(pi m / 2) / (pi m)
            const double r = (double) m / (double) (half + 1);
            const double w = bessel0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / bessel0(beta);
            h[k - 1] = sinc * w; sum += 2.0 * h[k - 1];
        }
        for (int k = 0; k < kHbL; ++k) c[k] = (float) (h[k] / sum);
        centre = (float) (0.5 / sum);
    }
    static const HalfbandCoefs& get() { static const HalfbandCoefs hc; return hc; }
};
struct Decimator2 {
    float even[kHbRing] = {}, odd[kHbRing] = {};
    int pos = 0;
    void reset() { std::memset(even, 0, sizeof even); std::memset(odd, 0, sizeof odd); pos = 0; }
    // x0 is the sample at the even phase, x1 the following one.
    inline float push(float x0, float x1, const HalfbandCoefs& hc) {
        pos = (pos + 1) & (kHbRing - 1);
        even[pos] = x0; odd[pos] = x1;
        float acc = hc.centre * even[(pos - (kHbL - 1)) & (kHbRing - 1)];
        for (int k = 1; k <= kHbL; ++k)
            acc += hc.c[k - 1] * (odd[(pos - kHbL + k) & (kHbRing - 1)] + odd[(pos - kHbL + 1 - k) & (kHbRing - 1)]);
        return acc;
    }
};

// ---------- envelope ----------
// Linear attack, exponential decay towards the sustain level, exponential release. Times in seconds
// are the time to reach about 95 percent (decay) and about 98 percent (release), as in the mockup preview.
struct Env {
    enum Stage : uint8_t { Idle, Attack, Decay, Release };
    float level = 0.f;
    Stage stage = Idle;
    float aInc = 1.f, dCoef = 0.99f, sus = 0.f, rCoef = 0.99f;
    void setTimes(double sr, double a, double d, double s, double r) {
        aInc = (float) (1.0 / (std::max(a, 1e-4) * sr));
        dCoef = (float) std::exp(-3.0 / (std::max(d, 1e-3) * sr));
        rCoef = (float) std::exp(-4.0 / (std::max(r, 1e-3) * sr));
        sus = (float) s;
    }
    void gate(bool keepLevel) { if (!keepLevel) level = 0.f; stage = Attack; }
    void release() { if (stage != Idle) stage = Release; }
    void kill() { level = 0.f; stage = Idle; }
    inline float tick() {
        switch (stage) {
            case Attack: level += aInc; if (level >= 1.f) { level = 1.f; stage = Decay; } break;
            case Decay: level = sus + (level - sus) * dCoef; if (sus <= 0.f && level < 1e-5f) { level = 0.f; stage = Idle; } break;
            case Release: level *= rCoef; if (level < 1e-4f) { level = 0.f; stage = Idle; } break;
            default: break;
        }
        return level;
    }
};

// ---------- voice filter ----------
// 24 dB: four pole zero delay feedback ladder with a soft saturated input, self oscillates at k >= 4.
// 12 dB: two pole state variable filter (Cytomic style TPT), saturated input and bandpass state, self oscillates at k <= 0.
// types: 0 HPF, 1 BPF, 2 LPF. g = tan(pi fc / fs).
struct VoiceFilter {
    double s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    void reset() { s0 = s1 = s2 = s3 = 0; }
    inline float process(float in, double g, double k, int type, bool pole24) {
        if (pole24) {
            const double G = g / (1.0 + g), G2 = G * G, G3 = G2 * G, G4 = G2 * G2, om = 1.0 - G;
            const double sigma = G3 * (s0 * om) + G2 * (s1 * om) + G * (s2 * om) + s3 * om;
            const double u = ((double) in - k * sigma) / (1.0 + k * G4);
            const double x = satD(u);
            const double v1 = (x - s0) * G, y1 = v1 + s0; s0 = y1 + v1;
            const double v2 = (y1 - s1) * G, y2 = v2 + s1; s1 = y2 + v2;
            const double v3 = (y2 - s2) * G, y3 = v3 + s2; s2 = y3 + v3;
            const double v4 = (y3 - s3) * G, y4 = v4 + s3; s3 = y4 + v4;
            const double comp = 1.0 + 0.12 * k;
            double out;
            if (type == 2) out = y4;
            else if (type == 0) out = x - 4.0 * y1 + 6.0 * y2 - 4.0 * y3 + y4;
            else out = 4.0 * (y2 - 2.0 * y3 + y4);
            return (float) (out * comp);
        }
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v0 = satD((double) in);
        const double v3 = v0 - s1;
        const double v1 = a1 * s0 + a2 * v3;
        const double v2 = s1 + a2 * s0 + a3 * v3;
        s0 = satD(2.0 * v1 - s0);
        s1 = 2.0 * v2 - s1;
        double out;
        if (type == 2) out = v2;
        else if (type == 0) out = v0 - k * v1 - v2;
        else out = v1 * (0.5 + 0.5 * k);
        return (float) out;
    }
    // Reset when the state ever leaves a sane range (never expected; keeps NaN out for good).
    inline void guard() {
        if (!(std::fabs(s0) < 1e4 && std::fabs(s1) < 1e4 && std::fabs(s2) < 1e4 && std::fabs(s3) < 1e4)) reset();
    }
};

// Filter resonance mapping (resonance knob 0..127 -> feedback). Own law, unverified against hardware.
inline double ladderK(double r01) { return 4.6 * std::pow(r01, 1.8); }       // self oscillation (k >= 4) from about 0.93
inline double svfK(double r01) {
    double k = 2.0 * std::pow(1.0 - r01, 1.3);
    const double t = std::min(1.0, std::max(0.0, (r01 - 0.93) / 0.07));
    return k - 0.05 * t * t * (3.0 - 2.0 * t);                                // slightly negative at the top: sustained ring
}

// ---------- scale helpers ----------
inline float panL(float p) { return std::cos((p * 0.5f + 0.5f) * 1.57079633f); }   // p in -1..1
inline float panR(float p) { return std::sin((p * 0.5f + 0.5f) * 1.57079633f); }

}  // namespace dsp
}  // namespace jpurrr
