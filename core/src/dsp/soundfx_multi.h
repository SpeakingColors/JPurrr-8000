// SoundFx multi-FX engine (internal): 12 chorus / flanger / phaser types plus DISTORTION.
// Own approximations of the JP-8000 / JP-8080 effect names. None of this is checked against hardware.
#pragma once
#include "soundfx_dsp.h"

namespace jpurrr {
namespace sfx {

struct ModDesign;

class MultiFx {
public:
    static constexpr int kNumTypes = 13;
    MultiFx();
    void prepare(double sampleRate);               // allocates
    void reset();                                  // clears all state, LFO phase to 0
    void setType(int t);                           // 0..12, same order as the MULTI-FX TYPE options
    int type() const { return type_; }
    void setRateOverride(double hz) { rateOverride_ = hz; }   // <= 0: use the type's own rate
    // Writes the finished effect output (dry and wet mixed) to out. lev[i] is the level 0..1 per sample.
    // in and out may not alias. n <= kChunk.
    void process(const float* inL, const float* inR, float* outL, float* outR, const float* lev, int n);

private:
    void runChorus(const float* in[2], float* out[2], const float* lev, int n);
    void runFlanger(const float* in[2], float* out[2], const float* lev, int n);
    void runPhaser(const float* in[2], float* out[2], const float* lev, int n);
    void runDistortion(const float* in[2], float* out[2], const float* lev, int n);
    float advanceLfo();                            // returns phase 0..1 after one sample

    double sr_ = 44100.0;
    int type_ = 0;
    const ModDesign* des_ = nullptr;
    double rateOverride_ = 0.0;
    float rateCur_ = 0.f;
    double phase_ = 0.0, phase2_ = 0.0;
    float rateK_ = 0.f;

    DelayBuf line_[2];
    OnePoleLP wetLp1_[2];
    Biquad wetLp_[2];
    OnePoleHP wetHp_[2];
    OnePoleLP fbLp_[2];
    OnePoleHP fbHp_[2];
    float msToSamp_ = 44.1f;

    static constexpr int kMaxStages = 12;
    float apS_[2][kMaxStages] = {};
    Biquad xoLp_[2][2], xoHp_[2][2];   // phaser: Linkwitz Riley 4 split, the allpass chain only sees the low band
    float fbPrev_[2] = {};

    // distortion: 8x oversampling (three half band stages), first order antiderivative anti-aliasing (ADAA)
    HalfbandUp up_[2][3];                               // stage 0: 1x->2x, 1: 2x->4x, 2: 4x->8x
    HalfbandDown dn_[2][3], dry_[2][3];                 // dry_* run the same filters without the shaper, to align the dry part
    Biquad dLp_[2][2];                                  // 4th order Butterworth low pass at 8x
    OnePoleHP dDc_[2];
    double adaaX_[2] = {}, adaaF_[2] = {};
    float pIn_ = 0.f, pOut_ = 0.f, pOutS_ = 0.f, powK_ = 0.f, powKs_ = 0.f;
    float gPrev_ = 1.f;
};

} // namespace sfx
} // namespace jpurrr
