// Live readouts the audio thread publishes for the editor displays. Pure C++17, lock free.
// The audio thread writes, the message thread reads. Torn reads are harmless (display only).
#pragma once
#include <atomic>
#include <cstdint>
namespace jpurrr {

// Mono ring buffer of the last 1024 samples at one point of the chain.
struct TapBuf {
    static constexpr int kSize = 1024;
    float s[kSize] = {};
    std::atomic<int> pos { 0 };                    // next write index
    void push(float v) { int p = pos.load(std::memory_order_relaxed); s[p] = v; pos.store((p + 1) & (kSize - 1), std::memory_order_relaxed); }
    // copy the last kSize samples, oldest first
    void snapshot(float* out) const { int p = pos.load(std::memory_order_relaxed); for (int i = 0; i < kSize; ++i) out[i] = s[(p + i) & (kSize - 1)]; }
};

// Order matches the mockup taps ('in','drv','eq','opt','ott','mb','glu','out').
enum LiveTap { kTapIn = 0, kTapDrive, kTapEq, kTapOpto, kTapOtt, kTapMb, kTapGlue, kTapOut, kNumTaps };

struct Live {
    TapBuf tap[kNumTaps];              // mono sums, written by FxChain (kTapIn = chain input after the gate position rules)
    TapBuf preL, preR;                 // stereo chain input (the mockup's 'mg')
    TapBuf outL, outR;                 // final output
    std::atomic<float> outPeakL { 0 }, outPeakR { 0 };      // linear peak per block, held and decayed by the editor
    std::atomic<float> gluGR { 0 };                         // glue compressor gain reduction, dB, <= 0
    std::atomic<float> optoGR { 0 };                        // opto leveller GR, dB, <= 0
    std::atomic<float> ottGR[3] = { {0}, {0}, {0} };        // OTT per band GR (downward), dB <= 0
    std::atomic<float> mbCompGR[4] = { {0}, {0}, {0}, {0} };// multiband compressor GR per band, dB <= 0
    std::atomic<float> mbLimGR[4] = { {0}, {0}, {0}, {0} }; // multiband limiter GR per band, dB <= 0
    std::atomic<int> gatePhase { -1 };                      // trance gate step 0..15, -1 when off or stopped
    // Synth side: counters the editor uses to animate envelope playheads.
    std::atomic<uint32_t> noteOnCount { 0 };                // incremented on every note on
    std::atomic<uint32_t> noteOffAllCount { 0 };            // incremented when the last held key is released
    std::atomic<int> activeVoices { 0 };
    std::atomic<int> lastNote { -1 }, lastVel { 0 };
};
}
