// The two parts (UPPER and LOWER) of KEY MODE DUAL and SPLIT, as the mockup v3.5 defines them. Pure C++17, no JUCE.
//   PERF  = the performance setup: KEY MODE, PANEL SELECT, Split Point, Part Detune. A sound never changes them unless it stores them.
//   GLOB  = shared by both parts and always taken from the panel (voices, arpeggiator, MIX READY chain, trance gate, setup, master volume).
//   PART  = the sound itself (oscillators, filter, amp, LFOs, envelopes, effects section ...): every part has its own set.
//   NONE  = not saved with a sound (levers, display buttons, PREVIEW).
// The panel shows the PART values of the edited part; the other part keeps its values in a store.
#pragma once
#include "jpurrr/params_gen.h"
namespace jpurrr {

enum class PartClass : unsigned char { None, Perf, Glob, Part };
PartClass partClassOf(int paramIndex);
inline bool isPartParam(int i) { return partClassOf(i) == PartClass::Part; }

constexpr int kUpper = 0, kLower = 1;      // part numbers as in Synth::setPartParams

// The part whose sound is on the panel. KEY MODE SINGLE: UPPER. PANEL SELECT LOWER: LOWER. UPPER and BOTH: UPPER.
// VOICE MOD keeps whatever part was edited before (current).
int effectiveEditPart(int keyMode, int panelSelect, int current);
// The parts that panel edits reach: SINGLE: UPPER. BOTH (in DUAL/SPLIT): both. VOICE MOD: none. Else the edited part. Returns the count, parts in out[2].
int editedParts(int keyMode, int panelSelect, int current, int out[2]);

// What the engine plays. panel = every parameter as the panel and the host have them; editPart = the part the panel shows;
// store[p] = the PART values of each part (only the entries of the part not on the panel are read).
// out[0] / out[1] = full parameter vectors for Synth::setPartParams(0/1). The effects section (a single shared effect) and the voice
// engine take out[0] (UPPER).
void composePartVectors(const int* panel, int editPart, const int store[2][kNumParams], int out[2][kNumParams]);

// A part keeps the sound it was loaded with; this copies the PART values of a full vector into a store entry (and the other way).
void copyPartValues(const int* from, int* to);

}
