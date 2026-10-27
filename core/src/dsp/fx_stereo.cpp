#include "fx_stereo.h"
JPURRR_FX_FAST
namespace jpurrr {
namespace fxdsp {

// ---------------------------------------------------------------- pocket high pass
void PocketHpf::prepare(double sr) { sr_ = sr; lf_.setTime(0.025, sr / kSub); reset(); }
void PocketHpf::reset() { s_[0].clear(); s_[1].clear(); lf_.snap(first_ ? std::log(10.0) : lf_.tgt); }       // keeps the wanted frequency
void PocketHpf::setParams(bool lowEndOn, int hpKnob) {
    const double f = (lowEndOn && hpKnob >= 3) ? hzv(hpKnob, 20, 500) : 10.0;
    lf_.tgt = std::log(f);
    if (first_) { lf_.snap(lf_.tgt); first_ = false; }
}
void PocketHpf::process(float* l, float* r, int n) {
    const double f = std::exp(lf_.tick());
    c_ = Rbj(f, sr_).highpass(0.70710678118654752);
    for (int i = 0; i < n; ++i) { l[i] = (float) s_[0].run(c_, l[i]); r[i] = (float) s_[1].run(c_, r[i]); }
}

// ---------------------------------------------------------------- low mono
void LowMono::prepare(double sr) { prepareBase(sr); lf_.setTime(0.025, sr / kSub); resetState(); }
void LowMono::resetState() { x_[0].clear(); x_[1].clear(); }
void LowMono::wake() { lf_.snap(lf_.tgt); }
void LowMono::setParams(bool on, int knob) { setOn(on); lf_.tgt = std::log(hzv(knob, 40, 250)); }
void LowMono::run(float* l, float* r, int n) {
    c_ = lr4Design(std::exp(lf_.tick()), sr_);
    for (int i = 0; i < n; ++i) {
        double lo[2], hi[2];
        x_[0].split(c_, l[i], lo[0], hi[0]);
        x_[1].split(c_, r[i], lo[1], hi[1]);
        const double m = 0.5 * (lo[0] + lo[1]);
        l[i] = (float) (hi[0] + m); r[i] = (float) (hi[1] + m);
    }
}

// ---------------------------------------------------------------- mid / side EQ
void MsEq::prepare(double sr) {
    prepareBase(sr);
    const double tr = sr / kSub;
    g3_.setTime(0.025, tr); gair_.setTime(0.025, tr); fsh_.setTime(0.025, tr);
    resetState();
}
void MsEq::resetState() { mid_.clear(); midAir_.clear(); side_.clear(); sideAir_.clear(); }
void MsEq::wake() { g3_.snap(g3_.tgt); gair_.snap(gair_.tgt); fsh_.snap(fsh_.tgt); }
void MsEq::setParams(bool on, int midPres, int sideHpf, int air) {
    setOn(on);
    g3_.tgt = midPres / 2.0; gair_.tgt = air / 2.0; fsh_.tgt = std::log(hzv(sideHpf, 80, 400));
}
void MsEq::run(float* l, float* r, int n) {
    cMid_ = Rbj(3000.0, sr_).peaking(0.8, g3_.tick());
    cAir_ = Rbj(10000.0, sr_).highshelf(gair_.tick());
    cSide_ = Rbj(std::exp(fsh_.tick()), sr_).highpass(0.70710678118654752);
    for (int i = 0; i < n; ++i) {
        const double m = 0.5 * ((double) l[i] + r[i]), s = 0.5 * ((double) l[i] - r[i]);
        const double m2 = midAir_.run(cAir_, mid_.run(cMid_, m));
        const double s2 = sideAir_.run(cAir_, side_.run(cSide_, s));
        l[i] = (float) (m2 + s2); r[i] = (float) (m2 - s2);
    }
}

// ---------------------------------------------------------------- widener
namespace {
struct Sin2Table {   // sin^2(pi p) for p in [0, 1), 1024 steps, linear interpolation
    float t[1026];
    Sin2Table() { for (int i = 0; i < 1026; ++i) { double s = std::sin(kPi * i / 1024.0); t[i] = (float) (s * s); } }
    inline float operator()(double p) const { const double x = p * 1024.0; const int i = (int) x; const float f = (float) (x - i); return t[i] + (t[i + 1] - t[i]) * f; }
};
const Sin2Table& sin2() { static const Sin2Table s; return s; }
}
void Widener::prepare(double sr) {
    prepareBase(sr);
    amt_.setTime(0.03, sr / kSub); haas_.setTime(0.05, sr / kSub);
    sin2();
    mid_.alloc((int) (0.05 * sr) + 64); right_.alloc((int) (0.05 * sr) + 64);
    resetState();
}
void Widener::resetState() { mid_.clear(); right_.clear(); phase_ = 0; }
void Widener::wake() { amt_.snap(amt_.tgt); haas_.snap(haas_.tgt); }
void Widener::setParams(bool on, int mode, int amount) {
    setOn(on); mode_ = mode;
    const double a = amount / 127.0;
    amt_.tgt = a; haas_.tgt = mode == 1 ? 1.0 : 0.0;
}
void Widener::run(float* l, float* r, int n) {
    const double a = amt_.tick(), hs = haas_.tick();
    const double gMicro = a * 0.9 * (1.0 - hs);              // injection into the sides, as the display: width = a * 0.9
    const double aHaas = a * 0.6 * hs;                       // preview: blend 0..0.6 of the delayed right channel
    const double cyc = cycle_ * sr_;                         // samples per sawtooth cycle
    const double delta = (std::pow(2.0, 7.0 / 1200.0) - 1.0);// delay change per sample for 7 cent
    const double sweep = delta * cyc;                        // samples the delay moves during one cycle
    const double base1 = 0.010 * sr_, base2 = 0.015 * sr_, haasD = 0.020 * sr_;
    const double inv = 1.0 / std::sqrt(2.0);
    const Sin2Table& S2 = sin2();
    for (int i = 0; i < n; ++i) {
        const float m = 0.5f * (l[i] + r[i]);
        mid_.push(m); right_.push(r[i]);
        double d = 0.0;
        if (gMicro > 1e-6) {
            double p1 = phase_, p2 = phase_ + 0.5; if (p2 >= 1.0) p2 -= 1.0;
            const double w1 = S2(p1), w2 = S2(p2);
            // voice up (+7 cent): the delay shrinks over the cycle; voice down (-7 cent): the delay grows
            const double u1 = mid_.frac(base1 + sweep * (1.0 - p1)) * w1 + mid_.frac(base1 + sweep * (1.0 - p2)) * w2;
            const double d1 = mid_.frac(base2 + sweep * p1) * w1 + mid_.frac(base2 + sweep * p2) * w2;
            d = (u1 - d1) * inv;
        }
        double yl = l[i] + gMicro * d, yr = r[i] - gMicro * d;
        if (aHaas > 1e-6) yr = yr * (1.0 - aHaas) + aHaas * right_.frac(haasD);
        l[i] = (float) yl; r[i] = (float) yr;
        phase_ += 1.0 / cyc; if (phase_ >= 1.0) phase_ -= 1.0;
    }
}

// ---------------------------------------------------------------- auto pan
namespace {
struct PanTable {    // cos and sin of x * pi / 2 for x in [0, 1], 1024 steps
    float c[1026], s[1026];
    PanTable() { for (int i = 0; i < 1026; ++i) { c[i] = (float) std::cos(kPi * 0.5 * i / 1024.0); s[i] = (float) std::sin(kPi * 0.5 * i / 1024.0); } }
};
const PanTable& panTable() { static const PanTable t; return t; }
}
void AutoPan::prepare(double sr) { panTable(); prepareBase(sr); depth_.setTime(0.05, sr / kSub); rate_.setTime(0.05, sr / kSub); resetState(); }
void AutoPan::resetState() { phase_ = 0; }
void AutoPan::wake() { depth_.snap(depth_.tgt); rate_.snap(rate_.tgt); }
void AutoPan::setParams(bool on, int rate, int depth) {
    setOn(on);
    depth_.tgt = depth / 127.0; rate_.tgt = std::log(hzv(rate, 0.05, 4.0));
}
void AutoPan::run(float* l, float* r, int n) {
    const PanTable& T = panTable();
    const double d = depth_.tick(), f = std::exp(rate_.tick());
    const double dp = f / sr_;
    // the pan position is computed at both ends of the chunk and interpolated (it moves at most 4 Hz)
    const double p0 = std::sin(2.0 * kPi * phase_) * d;
    double ph1 = phase_ + dp * n; ph1 -= std::floor(ph1);
    const double p1 = std::sin(2.0 * kPi * ph1) * d;
    const double inv = 1.0 / n;
    for (int i = 0; i < n; ++i) {
        const double p = p0 + (p1 - p0) * ((i + 1) * inv);
        const bool neg = p <= 0.0;
        const double x = (neg ? p + 1.0 : p) * 1024.0;
        int k = (int) x; if (k > 1023) k = 1023; if (k < 0) k = 0;
        const float fr = (float) (x - k);
        const float gl = T.c[k] + (T.c[k + 1] - T.c[k]) * fr, gr = T.s[k] + (T.s[k + 1] - T.s[k]) * fr;
        const float il = l[i], ir = r[i];
        if (neg) { l[i] = il + ir * gl; r[i] = ir * gr; }
        else { l[i] = il * gl; r[i] = ir + il * gr; }
    }
    phase_ = ph1;
}

}  // namespace fxdsp
}  // namespace jpurrr
