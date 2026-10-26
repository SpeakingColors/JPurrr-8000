// Dynamics blocks of the mix-ready chain: transient shaper, resonance tamer, opto leveller, OTT squeeze,
// multiband compressor and limiter, glue compressor with parallel path. All stereo linked, no look-ahead, no latency.
#pragma once
#include "fx_block.h"

namespace jpurrr {
namespace fxdsp {

// ---- building blocks -------------------------------------------------------------------------------------------------
// Static downward curve with soft knee. xdb = level, returns the gain change in dB (<= 0).
inline float downwardDb(float xdb, float thr, float ratio, float knee) {
    const float over = xdb - thr, slope = 1.f / ratio - 1.f;
    if (knee <= 0.01f) return over > 0.f ? slope * over : 0.f;
    if (2.f * over < -knee) return 0.f;
    if (2.f * std::fabs(over) <= knee) { const float o = over + 0.5f * knee; return slope * o * o / (2.f * knee); }
    return slope * over;
}
// Upward curve: lifts levels below thr. Returns the gain change in dB (>= 0), limited to maxBoost.
inline float upwardDb(float xdb, float thr, float ratio, float knee, float maxBoost) {
    const float under = thr - xdb, k = 1.f - 1.f / ratio;
    float g;
    if (2.f * under < -knee) g = 0.f;
    else if (2.f * std::fabs(under) <= knee) { const float o = under + 0.5f * knee; g = k * o * o / (2.f * std::max(knee, 0.01f)); }
    else g = k * under;
    return std::min(g, maxBoost);
}

// Feed forward compressor core. Peak envelope with attack and release, then the gain computer on the envelope.
// release == 0 selects AUTO: the release time follows how long the compressor has been working (see tick()).
struct CompCore {
    float thr = 0.f, ratio = 1.f, knee = 6.f;
    PeakEnv det;
    bool autoRel = false; double tauMin = 0.03, tauMax = 0.3, mAuto = 0.0, kAuto = 0.0, relScale = 1.0;
    float grDb = 0.f;                                 // last gain change in dB (<= 0)
    double sr = 48000;
    void prepare(double sr_) { sr = sr_; kAuto = 1.0 - std::exp(-(double) kSub / (1.0 * sr)); clear(); }
    void clear() { det.clear(); mAuto = 0.0; grDb = 0.f; }
    // attackSec > 0; releaseSec <= 0 means AUTO between tauMin and tauMax (times relScale)
    void set(float thrDb, float ratio_, float kneeDb, double attackSec, double releaseSec) {
        thr = thrDb; ratio = std::max(1.f, ratio_); knee = kneeDb;
        autoRel = releaseSec <= 0.0;
        det.set(attackSec, autoRel ? tauMin * relScale : releaseSec, sr);
    }
    void tick() {                                      // once per kSub samples
        if (autoRel) {
            mAuto += (std::min(1.0, -(double) grDb / 6.0) - mAuto) * kAuto;
            det.aR = std::exp(-1.0 / ((tauMin + (tauMax - tauMin) * mAuto) * relScale * sr));
        }
    }
    // lvl: linear magnitude of the sidechain. Returns the linear gain (<= 1).
    inline float run(float lvl) {
        const float e = (float) det.run(lvl);
        if (ratio <= 1.0001f || e < 1e-7f) { grDb = 0.f; return 1.f; }
        grDb = downwardDb(linToDb(e), thr, ratio, knee);
        return dbToLin(grDb);
    }
};

// Zero latency peak limiter for one band: instant attack, exponential release (AUTO = program dependent).
struct BandLimiter {
    float ceil = 1.f; double env = 0, aR = 0, mAuto = 0, kAuto = 0, sr = 48000; bool autoRel = true; float grDb = 0.f;
    void prepare(double s) { sr = s; kAuto = 1.0 - std::exp(-(double) kSub / (1.0 * sr)); clear(); }
    void clear() { env = 0; mAuto = 0; grDb = 0.f; }
    void set(float ceilLin, double releaseSec) { ceil = ceilLin; autoRel = releaseSec <= 0.0; if (!autoRel) aR = std::exp(-1.0 / (releaseSec * sr)); }
    void tick() {
        if (autoRel) { mAuto += (std::min(1.0, -(double) grDb / 3.0) - mAuto) * kAuto; aR = std::exp(-1.0 / ((0.03 + 0.22 * mAuto) * sr)); }
    }
    inline float run(float lvl) {
        env = std::max((double) lvl, env * aR);
        if (env <= (double) ceil) { grDb = 0.f; return 1.f; }
        const float g = (float) ((double) ceil / env);
        grDb = linToDb(g); return g;
    }
};

// ---- blocks ------------------------------------------------------------------------------------------------------------
class TransientShaper : public FxBlock {
public:
    void prepare(double sr);
    void setParams(bool on, int attack, int sustain);        // raw knob values -12..12, displayed as v / 2 dB
protected:
    void run(float* l, float* r, int n) override;
    void resetState() override;
private:
    double aDb_ = 0, sDb_ = 0, gain_ = 1.0, kg_ = 0;
    PeakEnv fast_, slow_;
};

class Tamer : public FxBlock {
public:
    void prepare(double sr);
    void setParams(bool on, int focus, int depth);           // focus 0: 3-5 kHz, 1: 7-9 kHz, 2: both; depth 0..127
protected:
    void run(float* l, float* r, int n) override;
    void resetState() override;
    void wake() override;
private:
    struct Band {
        double f0 = 4000; bool active = false;
        BqCoef narrowC, wideC; BqState narrow, wide; PeakEnv eN, eW;
        BqState bell[2]; double redDb = 0, kAtk = 0, kRel = 0;
    };
    Band b_[2];
    double maxDb_ = 0;
    bool use_[2] = { true, true };
    float fadeBand_[2] = { 1.f, 1.f };
};

class OptoLeveller : public FxBlock {
public:
    void prepare(double sr);
    void setParams(bool on, bool limitMode, int peakRed, int gain, int mix);   // 0..100, 0..100, 0..127
    float grDb() const { return gr_; }                        // <= 0
protected:
    void run(float* l, float* r, int n) override;
    void resetState() override;
    void wake() override;
private:
    float thr_ = -12.f, ratio_ = 3.f, knee_ = 12.f; Smooth mix_, makeup_;
    PeakEnv det_;
    double c1_ = 0, c2_ = 0, a1A_ = 0, a1R_ = 0, a2A_ = 0, a2R_[2] = { 0, 0 }, tau2Slow_ = 2.2;
    float gr_ = 0.f;
};

class OttSqueeze : public FxBlock {
public:
    void prepare(double sr);
    // down/up amount per band 0..127, level per band -12..12 (displayed v / 2 dB), depth and time 0..127
    void setParams(bool on, const int* down, const int* up, const int* level, int depth, int time);
    float grDb(int band) const { return gr_[band]; }
protected:
    void run(float* l, float* r, int n) override;
    void resetState() override;
    void wake() override;
private:
    struct Band { CompCore dn; PeakEnv upDet; double upGainDb = 0; float upThr = -34.f, upRatio = 1.f; };
    Band b_[3];
    Lr4Chan x1_[2], x2_[2]; BqState ap_[2];
    Lr4Coefs c1_, c2_; BqCoef apc_;
    Smooth depth_, dnS_[3], upS_[3], lvS_[3];
    double mkDb_[3] = { 0, 0, 0 };
    float gr_[3] = { 0, 0, 0 };
    double tt_ = 0.5, upAtt_ = 0, upRel_ = 0;
    void update();
};

class MultibandDyn : public FxBlock {
public:
    struct Band {
        bool on = false, mute = false, solo = false, limOn = true;
        int thr = 64, ratio = 46, att = 30, rel = 21, boost = 0, limRel = 0, limOut = 0;
    };
    void prepare(double sr);
    void setParams(bool on, int mix, const int* xover3, const Band* bands);
    float compGrDb(int b) const { return cgr_[b]; }
    float limGrDb(int b) const { return lgr_[b]; }
protected:
    void run(float* l, float* r, int n) override;
    void resetState() override;
    void wake() override;
private:
    Smooth fx_[3], mix_;
    Lr4Chan sa_[2], sb_[2], sc_[2]; BqState ap1a_[2], ap1b_[2], ap2_[2];
    Lr4Coefs ca_, cb_, cc_; BqCoef apf2_, apf3_;
    CompCore comp_[4]; BandLimiter lim_[4];
    Smooth boost_[4], thr_[4], ratio_[4], ceil_[4];
    float gate_[4] = { 1, 1, 1, 1 }, gateT_[4] = { 1, 1, 1, 1 };
    bool bandOn_[4] = { false, false, false, false }, limOn_[4] = { false, false, false, false };
    float cgr_[4] = { 0, 0, 0, 0 }, lgr_[4] = { 0, 0, 0, 0 };
    Band prm_[4];
    void configure();
    void updateCoefs();
};

class GlueComp : public FxBlock {
public:
    void prepare(double sr);
    void setParams(bool on, int ratioIdx, int thresh, int attack, int parallel);
    float grDb() const { return main_.grDb; }
protected:
    void run(float* l, float* r, int n) override;
    void resetState() override;
    void wake() override;
private:
    CompCore main_, par_;
    Smooth thr_, ratio_, pg_;
    double attSec_ = 0.03;
};

}  // namespace fxdsp
}  // namespace jpurrr
