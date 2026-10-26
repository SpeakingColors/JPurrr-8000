// Pure math the Synth engine uses, exposed so tests and tools can check it. Header only, no state.
#pragma once
#include <cmath>
namespace jpurrr {

// ---- Super Saw (Adam Szabo, "How to Emulate the Super Saw", 2010) ----
// Seven NAIVE saws: a centre saw and six sides. Numbers are from the thesis (measured on the real unit).
constexpr double kSuperSawRatio[7] = { -0.11002313, -0.06288439, -0.01952356, 0.0, 0.01991221, 0.06216538, 0.10745242 };

// Detune curve: 11th order polynomial of x = knob / 127, Horner form, highest order first.
inline double superSawCurve(double x) {
    static const double co[12] = { 10028.7312891634, -50818.8652045924, 111363.4808729368, -138150.6761080548,
                                   106649.6679158292, -53046.9642751875, 17019.9518580080, -3425.0836591318,
                                   404.2703938388, -24.1878824391, 0.6717417634, 0.0030115596 };
    double r = 0.0;
    for (double k : co) r = r * x + k;
    return r;
}
// Frequency of side saw i (0..6) relative to the note frequency: 1 + ratio * curve(knob/127).
inline double superSawFreqRatio(int i, int knob127) { return 1.0 + kSuperSawRatio[i] * superSawCurve(knob127 / 127.0); }
inline double superSawCents(int i, int knob127) { return 1200.0 * std::log2(superSawFreqRatio(i, knob127)); }
// Mix knob m = knob / 127.
inline double superSawCentreGain(double m) { return -0.55366 * m + 0.99785; }
inline double superSawSideGain(double m) { return -0.73764 * m * m + 1.2841 * m + 0.044372; }

// ---- Knob mappings (same as the mockup displays and its WebAudio preview) ----
inline double knobCutoffHz(int v) { return 20.0 * std::pow(1000.0, v / 127.0); }            // 20 Hz .. 20 kHz, log
inline double knobTime(int v, double lo, double hi) { return lo * std::pow(hi / lo, v / 127.0); }
inline double knobAmpAttack(int v)  { return knobTime(v, 0.002, 3.0); }    // seconds, linear ramp
inline double knobAmpDecay(int v)   { return knobTime(v, 0.01, 4.0); }     // seconds to reach about 95 percent
inline double knobAmpRelease(int v) { return knobTime(v, 0.01, 5.0); }     // seconds to reach about 98 percent
inline double knobPitchAttack(int v) { return knobTime(v, 0.001, 2.0); }
inline double knobPitchDecay(int v)  { return knobTime(v, 0.01, 3.0); }
// LFO rate in Hz. Own choice (the mockup has no number): 0.05 Hz .. 50 Hz, log. Unverified against hardware.
inline double knobLfoRate(int v) { return 0.05 * std::pow(1000.0, v / 127.0); }
}
