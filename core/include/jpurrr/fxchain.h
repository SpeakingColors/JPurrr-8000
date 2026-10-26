// Mix-ready FX chain contract (the q* and g* params). Owner: FX DSP. Pure C++17, no JUCE.
//
// Signal order (as the mockup's FLOW): trance gate -> drive stack -> pocket HPF + low mono -> M/S EQ -> transient shaper
// -> resonance tamer -> opto leveller -> OTT squeeze -> multiband compressor and limiter -> glue compressor (+ parallel)
// -> widener -> auto pan -> loudness boost -> soft clipper -> true peak limiter -> output trim.
//
// Latency is constant: oversampling delays of the drive stack and the clipper (always present, so switching blocks never moves
// the sound in time) plus the limiter's look-ahead, which is the only part that follows a parameter (LOOK AHEAD).
// MIX READY (qon) off: the input, after the gate, comes out delayed by exactly latencySamples(), bit for bit.
#pragma once
#include "jpurrr/params_gen.h"
#include "jpurrr/live.h"
#include <memory>
namespace jpurrr {
class FxChain {
public:
    FxChain(); ~FxChain();
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void setParams(const int* v);                  // kNumParams ints, same array as Synth; cheap when nothing changed
    void setTempo(double bpm);
    void setTransport(bool playing, double ppqPosition);   // ppq of the first sample of the next process() call
    void setLive(Live* live);                      // optional readouts for the editor, may stay null
    void process(float* left, float* right, int n); // in place
    int latencySamples() const;                    // exact, changes only on setParams (LOOK AHEAD)

    // Optional: gate position BEFORE FX (gpre = 0) means before the Sound page effects. Call this on the synth output, before
    // SoundFx::process, once per block with the same n as the later process() call; process() then skips its own gate for that
    // block. When never called, process() gates at the chain input in both positions (delay tails then get chopped too).
    void processPreGate(float* left, float* right, int n);
    void restartGate();                            // pattern back to step 0 (free running mode), e.g. on the first note

    // The drive curve the DRIVE / TRANS. / FUZZ knobs draw in the mockup (shapeCurve), for the editor display. x in -1..1.
    static double driveCurve(int drive, int trans, int fuzz, double x);
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
