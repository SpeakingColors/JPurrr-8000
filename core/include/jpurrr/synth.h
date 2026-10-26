// Synth voice engine contract. Owner: synth DSP. Pure C++17, no JUCE, no allocation in process().
#pragma once
#include "jpurrr/params_gen.h"
#include "jpurrr/live.h"
#include <memory>
namespace jpurrr {
class Synth {
public:
    Synth(); ~Synth();
    void prepare(double sampleRate, int maxBlock);
    void reset();
    // v points at kNumParams ints in the parameters' own units (kParams[i].min..max). Called before every block. Smooth inside.
    void setParams(const int* v);
    void noteOn(int ch, int note, int vel);        // ch 0..15, vel 1..127
    void noteOff(int ch, int note);
    void allNotesOff();
    void pitchBend(int ch, int value14);           // -8192..8191
    void controller(int ch, int cc, int value);    // 0..127 (sustain, mod wheel, ...)
    void channelPressure(int ch, int value);
    void setTempo(double bpm);
    void setTransport(bool playing, double ppqPosition);
    void setLive(Live* live);                      // optional readouts for the editor, may stay null
    void process(float* left, float* right, int n); // overwrites the buffers
    int latencySamples() const { return 0; }
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
