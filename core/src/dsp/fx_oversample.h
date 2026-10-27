// Linear phase half band oversampling (2x stages, cascaded to 4x) and the 8x true peak interpolator.
// Filter lengths do not depend on the sample rate: the pass band is about 0.418 fs, so the delay in samples is the same everywhere.
// History buffers are mirrored (every sample is stored twice) so the filters read contiguous memory.
#pragma once
#include "fx_common.h"

namespace jpurrr {
namespace fxdsp {

// Half band kernel for one 2x stage. M is the centre tap index (the delay at the high rate, per direction).
// The nonzero taps h[M +- j], j odd, form a palindromic FIR of 2n taps on one polyphase branch; the centre tap is a pure delay on the other.
struct HbKernel {
    int M = 0, pc = 0, pd = 1;
    int n = 0;                           // half length of the dense branch (2n taps)
    int L = 0;                           // 2n padded with leading zeros to a multiple of 4
    std::vector<float> cUp, cDown;       // padded palindromic coefficients (the up path includes the factor 2)
    int ucd = 0;                         // up: delay of the centre tap in input samples
    int dmin = 0, dcd = 0;               // down: smallest delay of the dense branch, delay of the centre tap
    int N = 0;                           // power of two history length
    void design(int centre, double beta);
};

class HbUp {
public:
    void init(const HbKernel* k) { k_ = k; buf_.assign((size_t) k->N * 2, 0.f); p_ = 0; }
    void clear() { std::fill(buf_.begin(), buf_.end(), 0.f); }
    inline void push(float x, float& even, float& odd) {      // one sample in, two out
        const HbKernel& k = *k_;
        p_ = (p_ + 1) & (unsigned) (k.N - 1);
        buf_[p_] = x; buf_[p_ + (unsigned) k.N] = x;
        const float* b = &buf_[p_ + (unsigned) k.N];          // newest sample
        const float dense = dot4(k.cUp.data(), b - (k.L - 1), k.L);
        const float center = b[-k.ucd];
        if (k.pc == 0) { even = center; odd = dense; } else { even = dense; odd = center; }
    }
private:
    const HbKernel* k_ = nullptr; std::vector<float> buf_; unsigned p_ = 0;
};
class HbDown {
public:
    void init(const HbKernel* k) { k_ = k; e_.assign((size_t) k->N * 2, 0.f); o_ = e_; p_ = 0; }
    void clear() { std::fill(e_.begin(), e_.end(), 0.f); std::fill(o_.begin(), o_.end(), 0.f); }
    inline float push(float even, float odd) {                // two in, one out
        const HbKernel& k = *k_;
        p_ = (p_ + 1) & (unsigned) (k.N - 1);
        e_[p_] = even; e_[p_ + (unsigned) k.N] = even; o_[p_] = odd; o_[p_ + (unsigned) k.N] = odd;
        const float* de = &e_[p_ + (unsigned) k.N]; const float* dd = &o_[p_ + (unsigned) k.N];
        const float* dense = k.pc == 0 ? dd : de; const float* cen = k.pc == 0 ? de : dd;
        return dot4(k.cDown.data(), dense - k.dmin - (k.L - 1), k.L) + 0.5f * cen[-k.dcd];
    }
private:
    const HbKernel* k_ = nullptr; std::vector<float> e_, o_; unsigned p_ = 0;
};

// The 4x kernels, built once (a few kilobytes).
struct OsKernels {
    HbKernel s1, s2;
    OsKernels();
    static const OsKernels& get();
    // Total delay in samples at the base rate of one up plus one down pass (exact integer).
    static int roundTripDelay();
};

// One channel: x -> 4 samples at 4x -> user callback -> x. The callback sees the four samples in time order.
class Oversampler4 {
public:
    void init() { const OsKernels& k = OsKernels::get(); u1_.init(&k.s1); u2_.init(&k.s2); d1_.init(&k.s1); d2_.init(&k.s2); }
    void clear() { u1_.clear(); u2_.clear(); d1_.clear(); d2_.clear(); }
    template <class F> inline float run(float x, F&& nl) {
        float a, b; u1_.push(x, a, b);
        float v[4]; u2_.push(a, v[0], v[1]); u2_.push(b, v[2], v[3]);
        nl(v);
        float p = d2_.push(v[0], v[1]), q = d2_.push(v[2], v[3]);
        return d1_.push(p, q);
    }
private:
    HbUp u1_, u2_; HbDown d1_, d2_;
};

// 8x windowed sinc interpolator used to find inter sample (true) peaks (32 taps, Kaiser beta 6).
// Measured against ideal band limited interpolation: peak error within -0.1 dB for content up to 0.45 fs, about -1.4 dB for
// full band white noise (a finite filter cannot see energy right at Nyquist).
// push() takes one sample and returns the largest |y| of the sample kHalf back and the 7 interpolated points after it.
class TruePeakProbe {
public:
    static constexpr int kHalf = 16;                     // taps on each side
    static constexpr int kPhases = 8;
    void init();
    void clear() { std::fill(h_, h_ + 4 * kHalf, 0.f); w_ = 0; }
    static int delay() { return kHalf; }                // samples between the newest input and the sample the result belongs to
    // Upper bound of the peak around the probed sample from the sample values alone (sum of |taps| times the window maximum).
    float l1() const { return l1_; }
    inline float push(float x, float quickLimit);        // quickLimit: when the sample peak cannot exceed it, returns the sample peak
private:
    float h_[4 * kHalf] = {};
    int w_ = 0;
    alignas(16) float taps_[kPhases][2 * kHalf];
    float l1_ = 1.f;
};

inline float TruePeakProbe::push(float x, float quickLimit) {
    w_ = (w_ + 1) & (2 * kHalf - 1);
    h_[w_] = x; h_[w_ + 2 * kHalf] = x;
    const float* seg = &h_[w_ + 1];                      // oldest first, newest last
    float wm = 0.f;
    for (int i = 0; i < 2 * kHalf; ++i) wm = std::max(wm, std::fabs(seg[i]));
    if (wm * l1_ <= quickLimit) return wm;
    float pk = std::fabs(seg[kHalf - 1]);
    for (int p = 1; p < kPhases; ++p) {
        pk = std::max(pk, std::fabs(dot4(taps_[p], seg, 2 * kHalf)));
    }
    return pk;
}

}  // namespace fxdsp
}  // namespace jpurrr
