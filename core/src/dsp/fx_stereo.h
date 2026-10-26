// Tone and stereo blocks of the mix-ready chain: pocket high pass, low mono, mid/side EQ, widener, auto pan.
#pragma once
#include "fx_block.h"

namespace jpurrr {
namespace fxdsp {

// 12 dB/oct high pass in front of the chain. OFF still leaves a 10 Hz high pass (DC guard), as the preview does.
class PocketHpf {
public:
    void prepare(double sr);
    void reset();
    void setParams(bool lowEndOn, int hpKnob);
    void process(float* l, float* r, int n);
private:
    double sr_ = 48000; Smooth lf_; BqState s_[2]; BqCoef c_; bool first_ = true;
};

// Everything below LOW MONO frequency is summed to mono (Linkwitz-Riley 24 dB split, the highs keep their stereo).
class LowMono : public FxBlock {
public:
    void prepare(double sr);
    void setParams(bool on, int knob);
protected:
    void run(float* l, float* r, int n) override;
    void resetState() override;
    void wake() override;
private:
    Smooth lf_; Lr4Chan x_[2]; Lr4Coefs c_;
};

// Mid/side EQ: MID PRES. bell at 3 kHz (Q 0.8) on the mid, SIDE HPF (12 dB/oct) on the sides, AIR high shelf at 10 kHz on both.
class MsEq : public FxBlock {
public:
    void prepare(double sr);
    void setParams(bool on, int midPres, int sideHpf, int air);
protected:
    void run(float* l, float* r, int n) override;
    void resetState() override;
    void wake() override;
private:
    Smooth g3_, gair_, fsh_;
    BqState mid_, midAir_, side_, sideAir_;
    BqCoef cMid_, cAir_, cSide_;
};

// MICRO PITCH: two voices from the mid signal, +7 and -7 cent (10 ms and 15 ms base delay), added to the sides with opposite sign
// (L + g*d, R - g*d), so the mono sum is untouched. HAAS: the right channel is blended with a 20 ms delayed copy.
class Widener : public FxBlock {
public:
    void prepare(double sr);
    void setParams(bool on, int mode, int amount);
protected:
    void run(float* l, float* r, int n) override;
    void resetState() override;
    void wake() override;
private:
    Smooth amt_, haas_;
    Ring mid_, right_;
    double phase_ = 0, cycle_ = 0.08;                // sawtooth phase 0..1, cycle length in seconds
    int mode_ = 0;
};

// Equal power balance pan (what the WebAudio StereoPanner does for stereo input) driven by a sine LFO.
class AutoPan : public FxBlock {
public:
    void prepare(double sr);
    void setParams(bool on, int rate, int depth);
protected:
    void run(float* l, float* r, int n) override;
    void resetState() override;
    void wake() override;
private:
    Smooth depth_, rate_;
    double phase_ = 0;
};

}  // namespace fxdsp
}  // namespace jpurrr
