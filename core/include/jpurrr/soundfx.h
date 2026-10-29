// Sound page effects: tone control, delay, multi-FX (chorus, flanger, phaser, distortion).
// Runs on the synth's stereo output, before the mix-ready FxChain. Owner: SoundFx DSP.
// Pure C++17, no JUCE, no allocation in process(). All algorithms are our own approximations,
// none of them is checked against Roland hardware (see ROADMAP / report: Unverified).
#pragma once
#include "jpurrr/params_gen.h"
#include <memory>
namespace jpurrr {
class SoundFx {
public:
    SoundFx(); ~SoundFx();
    void prepare(double sampleRate, int maxBlock);
    void reset();
    // v points at kNumParams ints (tb tt xl dt dfb dl xt dty dsy csy are read). Called before every block.
    void setParams(const int* v);
    void setTempo(double bpm);                     // host or arp tempo, for DELAY SYNC and CHORUS SYNC
    // Optional: LFO 1 rate in Hz, used when Chorus Sync is "LFO 1". 0 or less means unknown: the multi-FX then free runs.
    void setLfo1Hz(double hz);
    void process(float* left, float* right, int n); // in place
    int latencySamples() const;                    // always 0

    // Helpers for the editor and the tests.
    // Delay time in ms for a delay type (0..4) and the DELAY TIME knob (0..127), sync off.
    static double delayTimeMs(int delayType, int knob);
    // Length in quarter note beats for an index of the Delay Sync / Chorus Sync option lists; 0 for OFF, "LFO 1", unknown.
    static double syncBeats(int syncIndex);
    // Delay time in ms when DELAY SYNC is on (syncIndex 1..10) at a tempo. JP-8000 manual p.87: a mono delay cannot be longer than
    // 1250 ms and a panning delay not longer than 625 ms; a longer time is halved (repeatedly here, so it always fits).
    // Mono = MONO SHORT and MONO LONG (delayType 3, 4), panning = the other three. Returns 0 for OFF.
    static double delaySyncMs(int delayType, int syncIndex, double bpm);
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
