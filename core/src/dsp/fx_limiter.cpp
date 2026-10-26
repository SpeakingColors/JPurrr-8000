#include "fx_limiter.h"
JPURRR_FX_FAST
namespace jpurrr {
namespace fxdsp {

struct TruePeakLimiter::Impl {
    double sr = 48000;
    int maxLook = 0, look = 0, wantLook = 0;
    bool on = true, wantOn = true;
    double ceilLin = 1.0, relCoef = 0.0;
    TruePeakProbe probe[2];
    // audio delay line (stereo), length probeDelay + look
    std::vector<float> dl[2]; unsigned dmask = 0, dw = 0;
    // sliding minimum (monotone deque) over the last look+1 values of gr
    std::vector<float> dqVal; std::vector<unsigned> dqIdx; unsigned dqMask = 0, dqHead = 0, dqTail = 0;   // [head, tail)
    unsigned t = 0;                                  // index of the newest gr value
    // box average of m over look+1 values
    std::vector<float> mring; unsigned mmask = 0, mw = 0; double msum = 0; unsigned sumCount = 0;
    double s = 1.0;                                  // released gain state
    double wOn = 1.0;                                // smooth enable (1 = limiting)
    void clearState() {
        for (auto& p : probe) p.clear();
        for (auto& d : dl) std::fill(d.begin(), d.end(), 0.f);
        std::fill(dqVal.begin(), dqVal.end(), 1.f); dqHead = dqTail = 0; t = 0;
        std::fill(mring.begin(), mring.end(), 1.f); mw = 0; msum = (double) (look + 1); sumCount = 0;
        s = 1.0; dw = 0;
    }
};

TruePeakLimiter::TruePeakLimiter() : p_(new Impl) {}
TruePeakLimiter::~TruePeakLimiter() = default;

void TruePeakLimiter::prepare(double sr, double maxLookaheadSec) {
    Impl& s = *p_;
    s.sr = sr; s.maxLook = (int) std::ceil(maxLookaheadSec * sr) + 1;
    for (auto& p : s.probe) p.init();
    unsigned n = 16; while ((int) n < s.maxLook + TruePeakProbe::delay() + 8) n <<= 1;
    for (auto& d : s.dl) d.assign(n, 0.f);
    s.dmask = n - 1;
    unsigned m = 16; while ((int) m < s.maxLook + 8) m <<= 1;
    s.dqVal.assign(m, 1.f); s.dqIdx.assign(m, 0u); s.dqMask = m - 1;
    s.mring.assign(m, 1.f); s.mmask = m - 1;
    s.look = s.wantLook = 0; s.ceilLin = 1.0; s.relCoef = std::exp(-1.0 / (0.1 * sr));
    s.clearState();
}
void TruePeakLimiter::reset() { p_->clearState(); grDb_ = 0.f; }
void TruePeakLimiter::setParams(bool on, double ceilingDb, double releaseSec, int lookaheadSamples) {
    Impl& s = *p_;
    s.wantOn = on;
    // 0.05 dB of margin covers the interpolation error of the 8x probe and float rounding
    s.ceilLin = std::pow(10.0, (std::min(ceilingDb, 0.0) - 0.05) / 20.0);
    s.relCoef = std::exp(-1.0 / (std::max(releaseSec, 0.0005) * s.sr));
    s.wantLook = std::max(0, std::min(lookaheadSamples, s.maxLook));
    if (s.wantLook != s.look) { s.look = s.wantLook; s.clearState(); }       // a look-ahead change flushes the limiter
}
int TruePeakLimiter::latency() const { return TruePeakProbe::delay() + p_->look; }

void TruePeakLimiter::process(float* l, float* r, int n) {
    Impl& s = *p_;
    const int D = TruePeakProbe::delay() + s.look;
    const unsigned W = (unsigned) s.look + 1u;
    const double inv = 1.0 / (double) W;
    float* ch[2] = { l, r };
    const double wStep = 1.0 / (0.01 * s.sr);
    for (int i = 0; i < n; ++i) {
        const float x0 = ch[0][i], x1 = ch[1][i];
        s.dl[0][s.dw & s.dmask] = x0; s.dl[1][s.dw & s.dmask] = x1; ++s.dw;
        float pk = std::max(s.probe[0].push(x0, (float) s.ceilLin), s.probe[1].push(x1, (float) s.ceilLin));
        float gr = pk > (float) s.ceilLin ? (float) s.ceilLin / pk : 1.f;
        // sliding minimum of gr over the last W values (t is the index of gr)
        ++s.t;
        while (s.dqTail != s.dqHead && s.dqVal[(s.dqTail - 1) & s.dqMask] >= gr) --s.dqTail;
        s.dqVal[s.dqTail & s.dqMask] = gr; s.dqIdx[s.dqTail & s.dqMask] = s.t; ++s.dqTail;
        while (s.dqIdx[s.dqHead & s.dqMask] + W <= s.t) ++s.dqHead;
        const float m = s.dqVal[s.dqHead & s.dqMask];
        // box average of m over W values
        const double old = s.mring[(s.mw - W) & s.mmask];      // the value that leaves the window of W values
        s.mring[s.mw & s.mmask] = m; ++s.mw;
        s.msum += (double) m - old;
        if (++s.sumCount >= 65536u) { s.sumCount = 0; double acc = 0; for (unsigned k = 0; k < W; ++k) acc += s.mring[(s.mw - 1u - k) & s.mmask]; s.msum = acc; }
        double G = s.msum * inv;
        if (G > 1.0) G = 1.0;
        // enable fade: when off the target gain is 1
        s.wOn += (s.wantOn ? 1.0 : -1.0) * wStep; s.wOn = s.wOn < 0.0 ? 0.0 : (s.wOn > 1.0 ? 1.0 : s.wOn);
        // release
        const double rec = 1.0 - (1.0 - s.s) * s.relCoef;
        double sg = std::min(G, rec);
        s.s = sg;
        double g = 1.0 + (sg - 1.0) * s.wOn;
        const unsigned rd = s.dw - 1u - (unsigned) D;
        ch[0][i] = (float) (s.dl[0][rd & s.dmask] * g);
        ch[1][i] = (float) (s.dl[1][rd & s.dmask] * g);
        if (i == n - 1) grDb_ = linToDb((float) std::max(g, 1e-6));
    }
}

}  // namespace fxdsp
}  // namespace jpurrr
