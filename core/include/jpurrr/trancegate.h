// Trance gate: 16 steps, tempo synced, with depth in dB, decay, swell and smoothing. Pure C++17, no allocation.
// The shape of one step is the mockup's gLevel(on, u, nextOn). FxChain owns one; the synth side may use another
// before the Sound page effects (position BEFORE FX), see FxChain::processPreGate.
#pragma once
#include "jpurrr/params_gen.h"
namespace jpurrr {
class TranceGate {
public:
    void prepare(double sampleRate);
    void reset();
    void restart();                                   // next sample is step 0 of the pattern (free running mode)
    void setParams(const int* v);                     // reads gon gpat grate gdep gsm gdc gsw
    void setTempo(double bpm);
    void setTransport(bool playing, double ppqPosition);
    void process(float* left, float* right, int n);   // in place, safe for n of any size
    int step() const { return step_; }                // 0..15 of the last processed sample, -1 when the gate is off
    bool enabled() const { return on_; }

    // Beats per step for the rate index (1/4, 1/8, 1/8 triplet, 1/16, 1/16 triplet, 1/32).
    static double stepBeats(int rateIndex);
    // Linear floor level for a CUT DEPTH knob value 0..127 (0 dB at 0, -60 dB at 126, silence at 127).
    static double floorLevel(int depthKnob);
    // The mockup's gLevel: target level at position u (0..1) in a step.
    static double level(bool on, double u, bool nextOn, double floor, double decay01, double swell01);
private:
    double sr_ = 48000, bpm_ = 120, ppq_ = 0, gain_ = 1, engage_ = 0;
    bool playing_ = false, on_ = false;
    int pattern_ = 0, rate_ = 3, step_ = -1;
    double floor_ = 0, decay_ = 0, swell_ = 0, smooth_ = 0;
};
}
