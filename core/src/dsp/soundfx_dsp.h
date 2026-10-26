// SoundFx building blocks (internal). Pure C++17, no JUCE. Realtime safe once prepared.
// All of this is our own design; nothing here is copied from Roland or from other projects.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

namespace jpurrr {
namespace sfx {

constexpr double kPi = 3.14159265358979323846;
constexpr int kChunk = 128;   // block size of the internal scratch buffers

inline float flush(float x) { return std::fabs(x) < 1e-20f ? 0.f : x; }

// Smooth saturation for feedback loops: identity below 1, bounded by 2, continuous slope.
inline float loopSat(float x) {
    float a = std::fabs(x);
    if (a <= 1.f) return x;
    float s = 1.f + (a - 1.f) / (1.f + (a - 1.f));
    return x < 0.f ? -s : s;
}

// Last line of defence: never let NaN, inf or runaway values leave the effect.
inline float sanitize(float x) {
    if (!(std::fabs(x) < 64.f)) return (x == x) ? (x < 0.f ? -64.f : 64.f) : 0.f;
    return x;
}

inline float onePoleCoef(double fc, double sr) { return (float)(1.0 - std::exp(-2.0 * kPi * fc / sr)); }

// Windowed sinc fractional delay kernel: 8 taps, 128 sub sample phases, Kaiser window, DC gain 1.
struct Kernel8 {
    static constexpr int kPhases = 128;
    static constexpr int kTaps = 8;
    float t[kPhases + 1][kTaps];
    Kernel8();
};
const Kernel8& kernel8();

// Circular delay line with high quality fractional read.
// read(d): d samples back from the sample that is written next (d = 1 is the newest sample).
// d must lie in [kMinDelay, size - 8].
class DelayBuf {
public:
    static constexpr float kMinDelay = 5.f;
    void init(int minSize);
    void clear();
    int capacity() const { return mask_ + 1; }
    float read(float d) const {
        int i = (int)d;
        float f = d - (float)i;
        float q = f * (float)Kernel8::kPhases;
        int q0 = (int)q;
        float r = q - (float)q0;
        const float* a = kernel8().t[q0];
        const float* b = kernel8().t[q0 + 1];
        int base = w_ - i - 4;
        float acc = 0.f;
        for (int k = 0; k < Kernel8::kTaps; ++k)
            acc += buf_[(base + k) & mask_] * (a[k] + (b[k] - a[k]) * r);
        return acc;
    }
    void push(float x) { buf_[w_] = x; w_ = (w_ + 1) & mask_; }
private:
    std::vector<float> buf_;
    int mask_ = 0, w_ = 0;
};

// Second order section, transposed direct form 2.
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void lowShelf(double sr, double f0, double gainDb);   // RBJ, slope S = 1
    void highShelf(double sr, double f0, double gainDb);
    void lowpass(double sr, double f0, double q);
    void clear() { z1 = z2 = 0; }
    float process(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void flushState() { z1 = flush(z1); z2 = flush(z2); }
};

struct OnePoleLP {
    float a = 1.f, s = 0.f;
    void set(double fc, double sr) { a = onePoleCoef(fc, sr); }
    float process(float x) { s += a * (x - s); return s; }
    void clear() { s = 0.f; }
};

// One pole high pass: y = x - lowpass(x).
struct OnePoleHP {
    float a = 0.f, s = 0.f;
    void set(double fc, double sr) { a = onePoleCoef(fc, sr); }
    float process(float x) { s += a * (x - s); return x - s; }
    void clear() { s = 0.f; }
};

// Polyphase IIR half band (two allpass branches). Used for 2x up and down sampling. Zero latency.
struct HalfbandCoefs {
    int n0, n1;
    float c0[3], c1[3];
};
struct HalfbandUp {
    const HalfbandCoefs* c = nullptr;
    float s0[3] = {}, s1[3] = {};
    void clear() { for (int i = 0; i < 3; ++i) s0[i] = s1[i] = 0.f; }
    // one input sample in, two output samples (even, odd) out
    void process(float x, float& o0, float& o1) {
        float v = x;
        for (int i = 0; i < c->n0; ++i) { float y = c->c0[i] * v + s0[i]; s0[i] = v - c->c0[i] * y; v = y; }
        o0 = v;
        v = x;
        for (int i = 0; i < c->n1; ++i) { float y = c->c1[i] * v + s1[i]; s1[i] = v - c->c1[i] * y; v = y; }
        o1 = v;
    }
};
struct HalfbandDown {
    const HalfbandCoefs* c = nullptr;
    float s0[3] = {}, s1[3] = {};
    float prevOdd = 0.f;
    void clear() { for (int i = 0; i < 3; ++i) s0[i] = s1[i] = 0.f; prevOdd = 0.f; }
    // two input samples in time order (x0 = u[2n], x1 = u[2n+1]), one output sample out.
    // Branch 1 sees the odd sample of the previous pair: y[n] = 0.5 * (A0(u[2n]) + A1(u[2n-1])).
    float process(float x0, float x1) {
        float v = x0;
        for (int i = 0; i < c->n0; ++i) { float y = c->c0[i] * v + s0[i]; s0[i] = v - c->c0[i] * y; v = y; }
        float a = v;
        v = prevOdd;
        for (int i = 0; i < c->n1; ++i) { float y = c->c1[i] * v + s1[i]; s1[i] = v - c->c1[i] * y; v = y; }
        prevOdd = x1;
        return 0.5f * (a + v);
    }
};
extern const HalfbandCoefs kHbA;   // 1x <-> 2x, sharp
extern const HalfbandCoefs kHbB;   // 2x <-> 4x, relaxed

// Tempo sync tables. Index as in the Delay Sync / Chorus Sync options. Length in quarter note beats.
// 0 = off (also returned for "LFO 1" and out of range).
double syncBeats(int idx);

} // namespace sfx
} // namespace jpurrr
