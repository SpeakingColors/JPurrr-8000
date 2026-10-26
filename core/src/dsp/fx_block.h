// Base class of every switchable FX block: a 12 ms fade between dry and processed, sleeping (zero cost, identity) when off.
#pragma once
#include "fx_common.h"

namespace jpurrr {
namespace fxdsp {

class FxBlock {
public:
    virtual ~FxBlock() {}
    void prepareBase(double sr) { sr_ = sr; fade_.setRate(sr); asleep_ = true; fade_.snap(false); }
    void setOn(bool on) { fade_.set(on); }
    bool sleeping() const { return asleep_; }
    void reset() { asleep_ = true; fade_.snap(fade_.tgt > 0.f); resetState(); }
    // In place, n <= kSub.
    void process(float* l, float* r, int n) {
        const float a0 = fade_.advance(n), a1 = fade_.cur;
        if (a0 <= 0.f && a1 <= 0.f) { asleep_ = true; return; }
        if (asleep_) { resetState(); wake(); asleep_ = false; }
        if (a0 >= 1.f && a1 >= 1.f) { run(l, r, n); return; }
        float dl[kSub], dr[kSub];
        std::memcpy(dl, l, sizeof(float) * (size_t) n); std::memcpy(dr, r, sizeof(float) * (size_t) n);
        run(l, r, n);
        const float inv = 1.f / (float) n;
        for (int i = 0; i < n; ++i) {
            const float a = a0 + (a1 - a0) * (float) (i + 1) * inv;
            l[i] = dl[i] + (l[i] - dl[i]) * a; r[i] = dr[i] + (r[i] - dr[i]) * a;
        }
    }
protected:
    virtual void run(float* l, float* r, int n) = 0;
    virtual void resetState() {}      // clear filters and envelopes
    virtual void wake() {}            // snap smoothers to their targets
    double sr_ = 48000;
    Fade fade_;
    bool asleep_ = true;
};

}  // namespace fxdsp
}  // namespace jpurrr
