// True peak look-ahead limiter. Stereo linked.
// Detection: 8x interpolated peak (TruePeakProbe) per sample. Attack: minimum hold over the look-ahead window followed by a box
// average of the same length (the gain reaches the needed value exactly when the peak arrives). Release: exponential in the linear
// gain domain, never above the attack curve, so the ceiling holds. Delay = TruePeakProbe::delay() + look-ahead samples.
#pragma once
#include "fx_common.h"
#include "fx_oversample.h"
#include <memory>

namespace jpurrr {
namespace fxdsp {

class TruePeakLimiter {
public:
    TruePeakLimiter();
    ~TruePeakLimiter();
    void prepare(double sr, double maxLookaheadSec);
    void reset();
    // ceilingDb <= 0, releaseSec > 0, lookaheadSamples >= 0, on = limiting enabled (off passes the delayed signal).
    void setParams(bool on, double ceilingDb, double releaseSec, int lookaheadSamples);
    int latency() const;                             // exact, in samples
    void process(float* l, float* r, int n);
    float gainReductionDb() const { return grDb_; }  // <= 0, the gain at the end of the last call
private:
    struct Impl; std::unique_ptr<Impl> p_;
    float grDb_ = 0.f;
};

}  // namespace fxdsp
}  // namespace jpurrr
