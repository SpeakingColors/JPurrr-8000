// Synth voice engine contract. Owner: synth DSP. Pure C++17, no JUCE, no allocation in process().
#pragma once
#include "jpurrr/params_gen.h"
#include "jpurrr/live.h"
#include <memory>
namespace jpurrr {
// Sound engine options. They belong to the plugin, not to a sound (the plugin keeps them in its settings).
struct EngineOptions {
    int style = 0;          // 0 CLASSIC: the Super Saw uses raw saws without anti-aliasing, like the original unit (Szabo 2010)
                            // 1 CLEAN: every Super Saw oscillator gets its steps corrected (band limited)
    int antiAlias = 0;      // 0 STANDARD: 2 point correction (light), 1 HIGH: 4 point correction (cleaner top octave, slightly softer)
                            // applies to every corrected wave (pulse, saw, sync) and to the Super Saw in CLEAN style
    int oversampling = 0;   // 0 AUTO: 2x below 80 kHz, else 1x (the same as before this option existed); 1, 2 or 4
                            // never above 192 kHz inside: at 96 kHz the largest factor is 2
    bool operator==(const EngineOptions& o) const { return style == o.style && antiAlias == o.antiAlias && oversampling == o.oversampling; }
    bool operator!=(const EngineOptions& o) const { return !(*this == o); }
};
class Synth {
public:
public:
    Synth(); ~Synth();
    void prepare(double sampleRate, int maxBlock);
    void reset();
    // Call before process() on the audio thread, or while not processing. A change of the oversampling factor restarts
    // the oscillator filters of the sounding voices (a tiny click on a held note, nothing on a quiet synth).
    void setEngineOptions(const EngineOptions& o);
    EngineOptions engineOptions() const;
    // v points at kNumParams ints in the parameters' own units (kParams[i].min..max). Called before every block. Smooth inside.
    void setParams(const int* v);
    // Key mode DUAL and SPLIT: one patch per part (0 = upper, 1 = lower), same layout as setParams.
    // A part without its own patch (never set, or v == nullptr) plays the patch given to setParams.
    // Key mode, split point, voice count, voice assign, part detune, master volume and arpeggiator always come from setParams.
    void setPartParams(int part, const int* v);
    void noteOn(int ch, int note, int vel);        // ch 0..15, vel 1..127
    void noteOff(int ch, int note);
    void allNotesOff();
    // Notes from the on-screen keyboard strip: they play whatever the part MIDI channels are set to (the mockup keyboard always
    // sounds). They use channel index 15 inside, so they never clash with notes from a MIDI keyboard.
    void stripNoteOn(int note, int vel);
    void stripNoteOff(int note);
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
    int oversampling() const;                      // the factor in use: 1, 2 or 4
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
