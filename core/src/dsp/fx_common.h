// Shared helpers for the mix-ready FX chain (internal). Pure C++17, no allocation outside prepare().
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#if defined(__SSE__) || defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

// In unoptimised builds (Debug test runs) the DSP files are compiled with -O2 so the fuzz tests stay fast.
#if defined(__GNUC__) && !defined(__clang__) && !defined(__OPTIMIZE__)
#define JPURRR_FX_FAST _Pragma("GCC optimize(\"O2\")")
#else
#define JPURRR_FX_FAST
#endif

namespace jpurrr {
namespace fxdsp {

constexpr double kPi = 3.14159265358979323846;
constexpr int kSub = 32;                    // control rate: parameters are updated every kSub samples

inline double clampd(double x, double a, double b) { return x < a ? a : (x > b ? b : x); }
// Fast log2 / exp2 (relative error below 4e-6 / 2e-5 dB), no libm calls. Inputs must be normal positive floats / |x| < 126.
inline float fastLog2(float x) {
    union { float f; uint32_t i; } u; u.f = x;
    const float e = (float) ((int) (u.i >> 23) - 127);
    u.i = (u.i & 0x007FFFFFu) | 0x3F800000u;
    const float t = u.f - 1.f;
    const float p = 1.65e-5f + t * (1.4414924f + t * (-0.70648646f + t * (0.40947030f + t * (-0.18748860f + t * 0.043004958f))));
    return e + p;
}
inline float fastExp2(float x) {
    x = x < -125.f ? -125.f : (x > 125.f ? 125.f : x);
    const float fl = std::floor(x), f = x - fl;
    const float p = 1.0000035f + f * (0.69297292f + f * (0.24160436f + f * (0.051744998f + f * 0.013670309f)));
    union { float f; uint32_t i; } u; u.i = (uint32_t) ((int) fl + 127) << 23;
    return p * u.f;
}
inline float dbToLin(float db) { return fastExp2(db * 0.16609640474436813f); }       // 10^(db/20)
inline float linToDb(float x) { return 6.020599913279624f * fastLog2(x); }          // 20 log10(x), x > 1e-30
inline double flushd(double x) { return std::fabs(x) < 1e-30 ? 0.0 : x; }
inline bool finiteOrZero(float& x) { if (!(std::fabs(x) < 1e9f)) { x = 0.f; return false; } return true; }

// Mockup mappings (single source of truth is docs/mockups/JPurrr-8000-mockup-v3.2.html).
inline double hzv(int v, double lo, double hi) { return lo * std::pow(hi / lo, v / 127.0); }
inline double mbThr(int v) { return -48.0 * (1.0 - v / 127.0); }
inline double mbRat(int v) { return 1.0 + 19.0 * std::pow(v / 127.0, 1.6); }
inline double mbAtkMs(int v) { return 0.1 * std::pow(1000.0, v / 127.0); }
inline double mbRelMs(int v) { return v == 0 ? 0.0 : std::pow(1000.0, v / 127.0); }
inline double mbLimRelMs(int v) { return v == 0 ? 0.0 : 5.0 * std::pow(100.0, v / 127.0); }

// Dot product of n floats, n a multiple of 4 (SSE when available).
inline float dot4(const float* a, const float* b, int n) {
#if defined(__SSE__) || defined(__x86_64__) || defined(_M_X64)
    __m128 s = _mm_setzero_ps();
    for (int i = 0; i < n; i += 4) s = _mm_add_ps(s, _mm_mul_ps(_mm_loadu_ps(a + i), _mm_loadu_ps(b + i)));
    s = _mm_add_ps(s, _mm_movehl_ps(s, s));
    s = _mm_add_ss(s, _mm_shuffle_ps(s, s, 1));
    return _mm_cvtss_f32(s);
#else
    float s0 = 0.f, s1 = 0.f, s2 = 0.f, s3 = 0.f;
    for (int i = 0; i < n; i += 4) { s0 += a[i] * b[i]; s1 += a[i + 1] * b[i + 1]; s2 += a[i + 2] * b[i + 2]; s3 += a[i + 3] * b[i + 3]; }
    return (s0 + s1) + (s2 + s3);
#endif
}

// Turns off denormals for the duration of one process() call (x86 SSE, ARM64), restores the old state after.
struct ScopedNoDenormals {
#if defined(__SSE__) || defined(__x86_64__) || defined(_M_X64)
    unsigned prev;
    ScopedNoDenormals() : prev(_mm_getcsr()) { _mm_setcsr(prev | 0x8040u); }
    ~ScopedNoDenormals() { _mm_setcsr(prev); }
#elif defined(__aarch64__)
    uint64_t prev;
    ScopedNoDenormals() { asm volatile("mrs %0, fpcr" : "=r"(prev)); asm volatile("msr fpcr, %0" : : "r"(prev | (1ull << 24))); }
    ~ScopedNoDenormals() { asm volatile("msr fpcr, %0" : : "r"(prev)); }
#else
    ScopedNoDenormals() {}
#endif
};

// One pole smoother stepped once per control tick.
struct Smooth {
    double cur = 0, tgt = 0, k = 1;
    void setTime(double seconds, double tickRate) { k = seconds <= 0 ? 1.0 : 1.0 - std::exp(-1.0 / (seconds * tickRate)); }
    void snap(double v) { cur = tgt = v; }
    double tick() { cur += (tgt - cur) * k; return cur; }
};

// Linear on/off fade used by every block, stepped per chunk.
struct Fade {
    float cur = 0.f, tgt = 0.f, step = 1.f;      // step is per sample
    void setRate(double sr, double seconds = 0.012) { step = (float) (1.0 / (sr * seconds)); }
    void set(bool on) { tgt = on ? 1.f : 0.f; }
    void snap(bool on) { cur = tgt = on ? 1.f : 0.f; }
    float advance(int n) {                       // returns the value at the start of the chunk, cur is the end
        float s = cur;
        if (cur < tgt) cur = std::min(tgt, cur + step * (float) n);
        else if (cur > tgt) cur = std::max(tgt, cur - step * (float) n);
        return s;
    }
};

// Biquad, transposed direct form II, double precision.
struct BqCoef { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
struct BqState {
    double z1 = 0, z2 = 0;
    inline double run(const BqCoef& c, double x) {
        double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void clear() { z1 = z2 = 0; }
};

// RBJ cookbook designs. f is clamped to a safe range.
struct Rbj {
    double w0, cs, sn;
    Rbj(double f, double sr) {
        f = clampd(f, 1.0, sr * 0.4999);
        w0 = 2.0 * kPi * f / sr; cs = std::cos(w0); sn = std::sin(w0);
    }
    static BqCoef norm(double b0, double b1, double b2, double a0, double a1, double a2) {
        BqCoef c; double i = 1.0 / a0; c.b0 = b0 * i; c.b1 = b1 * i; c.b2 = b2 * i; c.a1 = a1 * i; c.a2 = a2 * i; return c;
    }
    BqCoef lowpass(double Q) const { double al = sn / (2 * Q); return norm((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al); }
    BqCoef highpass(double Q) const { double al = sn / (2 * Q); return norm((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al); }
    BqCoef bandpass(double Q) const { double al = sn / (2 * Q); return norm(al, 0, -al, 1 + al, -2 * cs, 1 - al); }   // constant 0 dB peak
    BqCoef allpass(double Q) const { double al = sn / (2 * Q); return norm(1 - al, -2 * cs, 1 + al, 1 + al, -2 * cs, 1 - al); }
    BqCoef peaking(double Q, double gainDb) const {
        double A = std::pow(10.0, gainDb / 40.0), al = sn / (2 * Q);
        return norm(1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
    }
    BqCoef highshelf(double gainDb) const {      // shelf slope S = 1 (what the WebAudio preview does)
        double A = std::pow(10.0, gainDb / 40.0), al = sn / 2.0 * std::sqrt(2.0), sq = 2.0 * std::sqrt(A) * al;
        return norm(A * ((A + 1) + (A - 1) * cs + sq), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - sq),
                    (A + 1) - (A - 1) * cs + sq, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - sq);
    }
};

// Linkwitz-Riley 24 dB/oct crossover for one channel: two cascaded 12 dB Butterworth sections per branch.
// The sum of low and high is a 2nd order allpass, so bands that skip a crossover get the matching allpass.
struct Lr4Coefs { BqCoef lp, hp; };
inline Lr4Coefs lr4Design(double f, double sr) {
    Rbj d(f, sr); const double q = 0.70710678118654752;
    return { d.lowpass(q), d.highpass(q) };
}
struct Lr4Chan {
    BqState l1, l2, h1, h2;
    inline void split(const Lr4Coefs& c, double x, double& lo, double& hi) {
        lo = l2.run(c.lp, l1.run(c.lp, x));
        hi = h2.run(c.hp, h1.run(c.hp, x));
    }
    void clear() { l1.clear(); l2.clear(); h1.clear(); h2.clear(); }
};
inline BqCoef apDesign(double f, double sr) { return Rbj(f, sr).allpass(0.70710678118654752); }

// Peak envelope follower: attack toward the input, release as an exponential decay.
struct PeakEnv {
    double env = 0, aA = 0, aR = 0;
    void set(double attSec, double relSec, double sr) {
        aA = attSec <= 0 ? 0.0 : std::exp(-1.0 / (attSec * sr));
        aR = relSec <= 0 ? 0.0 : std::exp(-1.0 / (relSec * sr));
    }
    inline double run(double x) {                // x >= 0
        if (x > env) env = x + (env - x) * aA; else env *= aR;
        return env;
    }
    void clear() { env = 0; }
};

// Mono ring delay with power of two size.
struct Ring {
    std::vector<float> b; unsigned mask = 0, w = 0;
    void alloc(int minSize) { unsigned n = 16; while ((int) n < minSize) n <<= 1; b.assign(n, 0.f); mask = n - 1; w = 0; }
    void clear() { std::fill(b.begin(), b.end(), 0.f); w = 0; }
    inline void push(float x) { b[w & mask] = x; ++w; }
    inline float at(int d) const { return b[(w - 1u - (unsigned) d) & mask]; }       // d = 0 is the newest sample
    // Fractional read, 4 point 3rd order Hermite. d >= 1 (the newest sample is d = 0).
    inline float frac(double d) const {
        int i = (int) d; double f = d - i;
        double ym1 = at(i - 1), y0 = at(i), y1 = at(i + 1), y2 = at(i + 2);
        double c1 = 0.5 * (y1 - ym1), c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2, c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
        return (float) (((c3 * f + c2) * f + c1) * f + y0);
    }
};

}  // namespace fxdsp
}  // namespace jpurrr
