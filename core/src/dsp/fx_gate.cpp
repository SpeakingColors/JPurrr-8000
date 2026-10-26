#include "jpurrr/trancegate.h"
#include "fx_common.h"
JPURRR_FX_FAST
namespace jpurrr {
using namespace fxdsp;

double TranceGate::stepBeats(int i) {
    static const double t[6] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return t[i < 0 ? 0 : (i > 5 ? 5 : i)];
}
double TranceGate::floorLevel(int d) { return d >= 127 ? 0.0 : std::pow(10.0, -(double) d / 127.0 * 60.0 / 20.0); }
double TranceGate::level(bool on, double u, bool nextOn, double f, double dc, double sw) {
    if (on) {
        if (dc < 0.01) return 1.0;
        double tau = 1.2 * (1.0 - dc) + 0.05;
        return f + (1.0 - f) * std::exp(-u / tau);
    }
    if (sw > 0.01 && nextOn) { double a = 1.0 - sw; if (u > a) return f + (1.0 - f) * ((u - a) / sw); }
    return f;
}

void TranceGate::prepare(double sr) { sr_ = sr; reset(); }
void TranceGate::reset() { gain_ = 1.0; engage_ = 0.0; ppq_ = 0.0; step_ = -1; }
void TranceGate::restart() { ppq_ = 0.0; }
void TranceGate::setParams(const int* v) {
    on_ = v[P_gon] != 0; pattern_ = v[P_gpat] & 0xFFFF; rate_ = v[P_grate];
    floor_ = floorLevel(v[P_gdep]); decay_ = v[P_gdc] / 127.0; swell_ = v[P_gsw] / 127.0; smooth_ = v[P_gsm] / 127.0;
}
void TranceGate::setTempo(double bpm) { if (bpm >= 20.0 && bpm <= 400.0) bpm_ = bpm; }
void TranceGate::setTransport(bool playing, double ppq) {
    playing_ = playing;
    if (playing && std::isfinite(ppq)) ppq_ = ppq;      // host position of the first sample of the next block
}

void TranceGate::process(float* l, float* r, int n) {
    const double beats = stepBeats(rate_), dppq = bpm_ / 60.0 / sr_;
    const double dur = 60.0 / bpm_ * beats;                                  // seconds per step
    const double sm = std::min(dur * 0.45, 0.001 + smooth_ * dur * 0.45);   // as the preview, setTargetAtTime tau = sm / 3
    const double ag = std::min(1.0, 1.0 - std::exp(-1.0 / (std::max(sm / 3.0, 1.0 / sr_) * sr_)));
    const double eStep = 1.0 / (0.012 * sr_);
    for (int i = 0; i < n; ++i) {
        engage_ += on_ ? std::min(eStep, 1.0 - engage_) : -std::min(eStep, engage_);
        double pos = ppq_ / beats;
        double fl = std::floor(pos);
        double u = pos - fl;
        long long idx = (long long) fl;
        int s = (int) (((idx % 16) + 16) % 16);
        bool onStep = (pattern_ >> s) & 1, nx = (pattern_ >> ((s + 1) & 15)) & 1;
        double tgt = on_ || engage_ > 0 ? level(onStep, u, nx, floor_, decay_, swell_) : 1.0;
        gain_ += (tgt - gain_) * ag;
        double g = 1.0 + (gain_ - 1.0) * engage_;
        if (engage_ <= 0.0 && !on_) g = 1.0;
        l[i] = (float) (l[i] * g); r[i] = (float) (r[i] * g);
        ppq_ += dppq;
        step_ = on_ ? s : -1;
    }
    if (n == 0) step_ = on_ ? step_ : -1;
}
}  // namespace jpurrr
