// SoundFx: shared DSP helpers, tone control, delay, and the glue for the multi-FX.
// Order (JP-8000 manual p.65): tone control, delay, multi-FX. Pan belongs to the synth.
#include "jpurrr/soundfx.h"
#include "soundfx_dsp.h"
#include "soundfx_multi.h"
#include <algorithm>
#include <cstring>

namespace jpurrr {
namespace sfx {

// ---------------------------------------------------------------- shared helpers

static double besselI0(double x) {
    double s = 1.0, t = 1.0, q = x * x * 0.25;
    for (int k = 1; k < 60; ++k) { t *= q / ((double)k * k); s += t; if (t < 1e-14 * s) break; }
    return s;
}

Kernel8::Kernel8() {
    const double beta = 6.0;
    const double i0b = besselI0(beta);
    for (int q = 0; q <= kPhases; ++q) {
        double f = (double)q / kPhases, sum = 0.0, h[kTaps];
        for (int k = 0; k < kTaps; ++k) {
            double d = (double)(k - 4) + f;                 // distance from the target, tap k is x[n - i + k - 4]
            double sinc = std::fabs(d) < 1e-12 ? 1.0 : std::sin(kPi * d) / (kPi * d);
            double r = d / 4.0;
            double win = std::fabs(r) >= 1.0 ? 0.0 : besselI0(beta * std::sqrt(1.0 - r * r)) / i0b;
            h[k] = sinc * win; sum += h[k];
        }
        for (int k = 0; k < kTaps; ++k) t[q][k] = (float)(h[k] / sum);
    }
}
const Kernel8& kernel8() { static const Kernel8 k; return k; }

void DelayBuf::init(int minSize) {
    int n = 64;
    while (n < minSize) n <<= 1;
    buf_.assign((size_t)n, 0.f);
    mask_ = n - 1; w_ = 0;
}
void DelayBuf::clear() { std::fill(buf_.begin(), buf_.end(), 0.f); w_ = 0; }

void Biquad::lowShelf(double sr, double f0, double gainDb) {
    double A = std::pow(10.0, gainDb / 40.0), w = 2.0 * kPi * f0 / sr, c = std::cos(w), s = std::sin(w);
    double al = s / std::sqrt(2.0), sa = 2.0 * std::sqrt(A) * al;
    double a0 = (A + 1) + (A - 1) * c + sa;
    b0 = (float)(A * ((A + 1) - (A - 1) * c + sa) / a0);
    b1 = (float)(2 * A * ((A - 1) - (A + 1) * c) / a0);
    b2 = (float)(A * ((A + 1) - (A - 1) * c - sa) / a0);
    a1 = (float)(-2 * ((A - 1) + (A + 1) * c) / a0);
    a2 = (float)(((A + 1) + (A - 1) * c - sa) / a0);
}
void Biquad::highShelf(double sr, double f0, double gainDb) {
    double A = std::pow(10.0, gainDb / 40.0), w = 2.0 * kPi * f0 / sr, c = std::cos(w), s = std::sin(w);
    double al = s / std::sqrt(2.0), sa = 2.0 * std::sqrt(A) * al;
    double a0 = (A + 1) - (A - 1) * c + sa;
    b0 = (float)(A * ((A + 1) + (A - 1) * c + sa) / a0);
    b1 = (float)(-2 * A * ((A - 1) + (A + 1) * c) / a0);
    b2 = (float)(A * ((A + 1) + (A - 1) * c - sa) / a0);
    a1 = (float)(2 * ((A - 1) - (A + 1) * c) / a0);
    a2 = (float)(((A + 1) - (A - 1) * c - sa) / a0);
}
void Biquad::lowpass(double sr, double f0, double q) {
    double w = 2.0 * kPi * std::min(f0, 0.45 * sr) / sr, c = std::cos(w), al = std::sin(w) / (2.0 * q);
    double a0 = 1 + al;
    b0 = (float)((1 - c) / 2 / a0); b1 = (float)((1 - c) / a0); b2 = b0;
    a1 = (float)(-2 * c / a0); a2 = (float)((1 - al) / a0);
}

// 6 + 4 coefficients, designed offline with a minimax fit (tools/ notes in the report).
// Stage A: 1x <-> 2x, passband to 0.4 of the 1x Nyquist-doubled rate, stopband better than -100 dB.
const HalfbandCoefs kHbA = { 3, 3, { 0.30325866f, 0.03925859f, 0.67513915f }, { 0.14774324f, 0.88321676f, 0.48314575f } };
// Stage B: 2x <-> 4x, wide transition, stopband better than -110 dB.
const HalfbandCoefs kHbB = { 2, 2, { 0.39393295f, 0.04260338f, 0.f }, { 0.17119026f, 0.74608930f, 0.f } };

// Option order of "Delay Sync" (0..10) and "Chorus Sync" (0..23): OFF, 1/16, 1/8 (3), 1/16 (.), 1/8, 1/4 (3), 1/8 (.),
// 1/4, 1/2 (3), 1/4 (.), 1/2, 1/1 (3), 1/2 (.), 1/1, 2/1 (3), 1/1 (.), 2/1, 3 .. 8 MEASURES, LFO 1.
// Quarter note beats; a measure is 4 beats (4/4 assumed).
double syncBeats(int idx) {
    static const double tab[23] = { 0.0, 0.25, 1.0 / 3, 0.375, 0.5, 2.0 / 3, 0.75, 1.0, 4.0 / 3, 1.5, 2.0,
                                    8.0 / 3, 3.0, 4.0, 16.0 / 3, 6.0, 8.0, 12.0, 16.0, 20.0, 24.0, 28.0, 32.0 };
    return (idx >= 0 && idx < 23) ? tab[idx] : 0.0;
}

// ---------------------------------------------------------------- tone control

// Bass and treble shelves. Corners and range follow the mockup's preview rig (200 Hz and 4 kHz, +-12 dB).
// Unverified against the hardware.
class ToneFx {
public:
    void prepare(double sr) { sr_ = sr; reset(); }
    void reset() { for (int c = 0; c < 2; ++c) { lo_[c] = Biquad(); hi_[c] = Biquad(); } gLo_ = gHi_ = 0.f; tLo_ = tHi_ = 0.f; snap_ = true; }
    void setParams(int bass, int treble) { tLo_ = (float)bass / 64.f * 12.f; tHi_ = (float)treble / 64.f * 12.f; if (snap_) { gLo_ = tLo_; gHi_ = tHi_; snap_ = false; recalc(); } }
    void process(float* l, float* r, int n) {
        const int sub = 32;
        const float k = 1.f - std::exp(-(float)sub / (0.020f * (float)sr_));   // 20 ms smoothing, per sub block
        for (int p = 0; p < n; p += sub) {
            int m = std::min(sub, n - p);
            gLo_ += (tLo_ - gLo_) * k; if (std::fabs(tLo_ - gLo_) < 0.002f) gLo_ = tLo_;
            gHi_ += (tHi_ - gHi_) * k; if (std::fabs(tHi_ - gHi_) < 0.002f) gHi_ = tHi_;
            if (gLo_ == 0.f && gHi_ == 0.f) {
                for (int c = 0; c < 2; ++c) { lo_[c].clear(); hi_[c].clear(); lo_[c].setIdentity(); hi_[c].setIdentity(); }
                continue;                                        // exact bypass
            }
            recalc();
            for (int i = 0; i < m; ++i) {
                l[p + i] = hi_[0].process(lo_[0].process(l[p + i]));
                r[p + i] = hi_[1].process(lo_[1].process(r[p + i]));
            }
            for (int c = 0; c < 2; ++c) { lo_[c].flushState(); hi_[c].flushState(); }
        }
    }
private:
    void recalc() {
        for (int c = 0; c < 2; ++c) {
            if (gLo_ == 0.f) lo_[c].setIdentity(); else lo_[c].lowShelf(sr_, 200.0, gLo_);
            if (gHi_ == 0.f) hi_[c].setIdentity(); else hi_[c].highShelf(sr_, std::min(4000.0, 0.4 * sr_), gHi_);
        }
    }
    double sr_ = 44100.0;
    Biquad lo_[2], hi_[2];
    float gLo_ = 0.f, gHi_ = 0.f, tLo_ = 0.f, tHi_ = 0.f;
    bool snap_ = true;
};

// ---------------------------------------------------------------- delay

// Five types. Longest hop in ms (own reading of the manual's maxima):
//   0 PANNING L>R 625, 1 PANNING R>L 625, 2 PANNING SHORT 156, 3 MONO SHORT 156, 4 MONO LONG 1250.
// Panning types are a ping pong pair: mono input, first echo on one side after one hop, next on the other side.
// Mono types are one delay line to the centre. Feedback is damped (7 kHz low pass, 30 Hz high pass), so repeats
// get darker and the loop can never grow. Unverified against the hardware.
static const double kDelayMaxMs[5] = { 625.0, 625.0, 156.0, 156.0, 1250.0 };

class DelayFx {
public:
    void prepare(double sr) {
        sr_ = sr;
        a_.init((int)(2.5 * sr) + 64);
        b_.init((int)(2.5 * sr) + 64);
        maxSamp_ = (double)a_.capacity() - 16.0;
        kT_ = (float)(1.0 - std::exp(-1.0 / (0.06 * sr)));
        kL_ = (float)(1.0 - std::exp(-1.0 / (0.015 * sr)));
        fadeStep_ = (float)(1.0 / (0.012 * sr));
        lp_[0].set(7000.0, sr); lp_[1].set(7000.0, sr);
        hp_[0].set(30.0, sr); hp_[1].set(30.0, sr);
        reset();
    }
    void reset() {
        a_.clear(); b_.clear(); lp_[0].clear(); lp_[1].clear(); hp_[0].clear(); hp_[1].clear();
        levCur_ = 0.f; fade_ = 1.f; dirty_ = false; snap_ = true; type_ = tType_;
    }
    // timeSamples already includes sync and the type's range.
    void setTarget(int type, double timeSamples, float fb, float lev) {
        tType_ = type; tFb_ = fb; tLev_ = lev;
        tTime_ = (float)std::min(std::max(timeSamples, 8.0), maxSamp_);
        if (snap_) { tCur_ = tTime_; fb_ = tFb_; snap_ = false; }
    }
    void process(float* l, float* r, int n) {
        const float sq2 = 1.41421356f;
        for (int i = 0; i < n; ++i) {
            levCur_ += (tLev_ - levCur_) * kL_;
            if (tLev_ == 0.f && levCur_ < 1e-5f) levCur_ = 0.f;
            if (levCur_ == 0.f && tLev_ == 0.f) {                       // off: stay silent and clean
                if (dirty_) { a_.clear(); b_.clear(); lp_[0].clear(); lp_[1].clear(); hp_[0].clear(); hp_[1].clear(); dirty_ = false; }
                type_ = tType_; tCur_ = tTime_; fb_ = tFb_; fade_ = 1.f;
                continue;
            }
            dirty_ = true;
            if (type_ != tType_) {                                      // type change: fade out, flush, fade in
                fade_ -= fadeStep_;
                if (fade_ <= 0.f) {
                    fade_ = 0.f; a_.clear(); b_.clear(); lp_[0].clear(); lp_[1].clear(); hp_[0].clear(); hp_[1].clear();
                    type_ = tType_; tCur_ = tTime_;
                }
            } else if (fade_ < 1.f) {
                fade_ = std::min(1.f, fade_ + fadeStep_);
            }
            tCur_ += (tTime_ - tCur_) * kT_;
            fb_ += (tFb_ - fb_) * kL_;
            float in = 0.5f * (l[i] + r[i]);
            float wl, wr;
            if (type_ >= 3) {                                           // mono short, mono long
                float y = a_.read(tCur_);
                float f = hp_[0].process(lp_[0].process(y)) * fb_;
                a_.push(flush(in + loopSat(f)));
                wl = wr = y;
            } else {                                                    // panning pair
                float ya = a_.read(tCur_), yb = b_.read(tCur_);
                b_.push(ya);
                float f = hp_[0].process(lp_[0].process(yb)) * fb_;
                a_.push(flush(in + loopSat(f)));
                if (type_ == 1) { wl = yb * sq2; wr = ya * sq2; }       // R>L
                else            { wl = ya * sq2; wr = yb * sq2; }       // L>R and PANNING SHORT
            }
            float g = levCur_ * fade_ * fade_;
            l[i] += wl * g; r[i] += wr * g;
        }
    }
private:
    double sr_ = 44100.0, maxSamp_ = 1000.0;
    DelayBuf a_, b_;
    OnePoleLP lp_[2];
    OnePoleHP hp_[2];
    int type_ = 0, tType_ = 0;
    float tCur_ = 1000.f, tTime_ = 1000.f, fb_ = 0.f, tFb_ = 0.f, levCur_ = 0.f, tLev_ = 0.f, fade_ = 1.f;
    float kT_ = 0.001f, kL_ = 0.001f, fadeStep_ = 0.01f;
    bool dirty_ = false, snap_ = true;
};

} // namespace sfx

// ---------------------------------------------------------------- SoundFx

using namespace sfx;

struct SoundFx::Impl {
    double sr = 44100.0, bpm = 120.0, lfo1 = 0.0;
    int maxBlock = 512;
    int pTb = 0, pTt = 0, pXl = 55, pDt = 70, pDfb = 40, pDl = 22, pXt = 1, pDty = 0, pDsy = 0, pCsy = 0;
    ToneFx tone;
    DelayFx delay;
    MultiFx fx[2];
    int cur = 0;
    bool xfading = false, wasActive = false, snapLev = true;
    float xpos = 0.f, xstep = 0.01f, levCur = 0.f, levT = 0.f, kLev = 0.01f;
    int typeT = 1;
    float bufL[2][kChunk], bufR[2][kChunk], lev[kChunk];

    void updateDelay() {
        double ms = SoundFx::delayTimeMs(pDty, pDt);
        double beats = SoundFx::syncBeats(pDsy);
        double samples;
        if (pDsy > 0 && beats > 0.0) {
            double sec = beats * 60.0 / std::max(bpm, 20.0);
            while (sec > 2.4) sec *= 0.5;                               // keep the rhythm, fold into the buffer
            samples = sec * sr;
        } else {
            samples = ms * 0.001 * sr;
        }
        delay.setTarget(std::min(std::max(pDty, 0), 4), samples, (float)pDfb / 127.f * 0.92f, (float)pDl / 127.f);
    }
    void updateRate() {
        double hz = 0.0;
        if (pCsy >= 1 && pCsy <= 22) hz = std::min(12.0, std::max(bpm, 20.0) / 60.0 / syncBeats(pCsy));
        else if (pCsy == 23 && lfo1 > 0.0) hz = std::min(12.0, lfo1);
        fx[0].setRateOverride(hz); fx[1].setRateOverride(hz);
    }
};

SoundFx::SoundFx() : impl_(new Impl) {}
SoundFx::~SoundFx() = default;

double SoundFx::delayTimeMs(int type, int knob) {
    type = std::min(std::max(type, 0), 4);
    knob = std::min(std::max(knob, 0), 127);
    return std::max(1.0, kDelayMaxMs[type] * (double)knob / 127.0);
}
double SoundFx::syncBeats(int idx) { return sfx::syncBeats(idx); }
int SoundFx::latencySamples() const { return 0; }

void SoundFx::prepare(double sampleRate, int maxBlock) {
    Impl& m = *impl_;
    m.sr = std::max(sampleRate, 8000.0); m.maxBlock = std::max(maxBlock, 1);
    kernel8();                                                         // build the static table now, not on the audio thread
    m.tone.prepare(m.sr);
    m.delay.prepare(m.sr);
    m.fx[0].prepare(m.sr); m.fx[1].prepare(m.sr);
    m.kLev = (float)(1.0 - std::exp(-1.0 / (0.008 * m.sr)));
    m.xstep = (float)(1.0 / (0.030 * m.sr));
    reset();
}

void SoundFx::reset() {
    Impl& m = *impl_;
    m.tone.reset(); m.delay.reset();
    m.fx[0].reset(); m.fx[1].reset();
    m.cur = 0; m.xfading = false; m.wasActive = false; m.snapLev = true; m.levCur = 0.f;
    m.fx[0].setType(m.typeT);
    m.updateDelay();
}

void SoundFx::setTempo(double bpm) {
    Impl& m = *impl_;
    if (!(bpm > 0.0)) return;
    m.bpm = std::min(std::max(bpm, 20.0), 400.0);
    m.updateDelay(); m.updateRate();
}
void SoundFx::setLfo1Hz(double hz) { impl_->lfo1 = (hz > 0.0 && hz < 1000.0) ? hz : 0.0; impl_->updateRate(); }

void SoundFx::setParams(const int* v) {
    Impl& m = *impl_;
    auto cl = [&](int i, int lo, int hi) { return std::min(std::max(v[i], lo), hi); };
    m.pTb = cl(P_tb, -64, 63); m.pTt = cl(P_tt, -64, 63); m.pXl = cl(P_xl, 0, 127);
    m.pDt = cl(P_dt, 0, 127); m.pDfb = cl(P_dfb, 0, 127); m.pDl = cl(P_dl, 0, 127);
    m.pXt = cl(P_xt, 0, MultiFx::kNumTypes - 1); m.pDty = cl(P_dty, 0, 4);
    m.pDsy = cl(P_dsy, 0, 10); m.pCsy = cl(P_csy, 0, 23);
    m.tone.setParams(m.pTb, m.pTt);
    m.updateDelay(); m.updateRate();
    m.typeT = m.pXt; m.levT = (float)m.pXl / 127.f;
    if (m.snapLev) { m.levCur = m.levT; m.snapLev = false; if (!m.wasActive) m.fx[m.cur].setType(m.typeT); }
}

void SoundFx::process(float* left, float* right, int n) {
    Impl& m = *impl_;
    for (int p = 0; p < n; p += kChunk) {
        const int c = std::min(kChunk, n - p);
        float* l = left + p; float* r = right + p;
        for (int i = 0; i < c; ++i) {                                  // never let bad input in
            if (!(std::fabs(l[i]) < 64.f)) l[i] = sanitize(l[i]);
            if (!(std::fabs(r[i]) < 64.f)) r[i] = sanitize(r[i]);
        }
        m.tone.process(l, r, c);
        m.delay.process(l, r, c);

        // multi-FX with level smoothing and a short crossfade when the type changes
        for (int i = 0; i < c; ++i) {
            m.levCur += (m.levT - m.levCur) * m.kLev;
            if (m.levT == 0.f && m.levCur < 1e-5f) m.levCur = 0.f;
            m.lev[i] = m.levCur;
        }
        const bool active = m.levT > 0.f || m.levCur > 0.f;
        if (!active) {
            m.wasActive = false; m.xfading = false;
        } else {
            if (!m.wasActive) { m.fx[m.cur].setType(m.typeT); m.fx[m.cur].reset(); m.wasActive = true; m.xfading = false; }
            if (!m.xfading && m.fx[m.cur].type() != m.typeT) {
                int o = 1 - m.cur;
                m.fx[o].setType(m.typeT); m.fx[o].reset();
                m.xfading = true; m.xpos = 0.f;
            }
            MultiFx& a = m.fx[m.cur];
            a.process(l, r, m.bufL[0], m.bufR[0], m.lev, c);
            if (m.xfading) {
                m.fx[1 - m.cur].process(l, r, m.bufL[1], m.bufR[1], m.lev, c);
                for (int i = 0; i < c; ++i) {
                    float x = std::min(1.f, m.xpos); m.xpos += m.xstep;
                    m.bufL[0][i] += (m.bufL[1][i] - m.bufL[0][i]) * x;
                    m.bufR[0][i] += (m.bufR[1][i] - m.bufR[0][i]) * x;
                }
                if (m.xpos >= 1.f) { m.cur = 1 - m.cur; m.xfading = false; }
            }
            for (int i = 0; i < c; ++i) { l[i] = sanitize(m.bufL[0][i]); r[i] = sanitize(m.bufR[0][i]); }
        }
    }
}

} // namespace jpurrr
