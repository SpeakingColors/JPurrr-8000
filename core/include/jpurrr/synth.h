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
    // Key mode DUAL and SPLIT: one patch per part (0 = upper, 1 = lower), same layout as setParams.
    // A part without its own patch (never set, or v == nullptr) plays the patch given to setParams.
    // Key mode, split point, voice count, voice assign, part detune, master volume and arpeggiator always come from setParams.
    void setPartParams(int part, const int* v);
    void noteOn(int ch, int note, int vel);        // ch 0..15, vel 1..127
    void noteOff(int ch, int note);
    void allNotesOff();
    void pitchBend(int ch, int value14);           // -8192..8191
    void controller(int ch, int cc, int value);    // 0..127: 1 mod wheel, 64 sustain, 120 all sound off, 121 reset controllers, 123 all notes off
    void channelPressure(int ch, int value);       // adds to the mod wheel (LFO 2)
    void setTempo(double bpm);
    void setTransport(bool playing, double ppqPosition);
    void setLive(Live* live);                      // optional readouts for the editor, may stay null
    void process(float* left, float* right, int n); // overwrites the buffers
    int latencySamples() const { return 0; }       // the oscillator decimator delays every voice by about 0.5 ms; not reported
    // Introspection for tests and the editor.
    int activeVoices() const;                      // voices that are not idle
    int heldVoices() const;                        // voices whose key is still down (or held by the pedal)
    int oversampling() const;                      // 1 or 2
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
