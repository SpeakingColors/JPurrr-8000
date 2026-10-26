// Mix-ready FX chain contract (the q* and g* params). Owner: FX DSP. Pure C++17, no JUCE.
#pragma once
#include "jpurrr/params_gen.h"
#include <memory>
namespace jpurrr {
class FxChain {
public:
    FxChain(); ~FxChain();
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void setParams(const int* v);                  // kNumParams ints, same array as Synth
    void setTempo(double bpm);
    void setTransport(bool playing, double ppqPosition);
    void process(float* left, float* right, int n); // in place
    int latencySamples() const;                    // exact, changes only on setParams
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
