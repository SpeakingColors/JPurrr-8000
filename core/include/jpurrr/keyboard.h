// Logic of the on-screen keyboard strip (BEND and MOD wheels, 61 keys, computer keys). Pure C++17, no JUCE, no allocation.
// Everything here is a port of the mockup's strip code (docs/mockups/JPurrr-8000-mockup-v3.3.html: KMAP, bendNorm, bendCents,
// setN, springBack, wheel and key handlers) so the plugin behaves like the mockup.
#pragma once
#include <cstdint>

namespace jpurrr {

constexpr int kKeyFirstNote = 36, kKeyLastNote = 96;      // C2..C7, 61 keys
constexpr int kKeyVelocity = 100;                         // the mockup has no velocity; 100 is the neutral point of the velocity morph
constexpr int kKeyChannel = 15;                           // channel index the synth uses for strip notes (they play whatever the part channels are)

// Computer key to MIDI note (mockup KMAP): Z S X D C V G B H N J M , = C3..B3, Q 2 W 3 E R 5 T 6 Y 7 U I = C4..C5. -1 when unmapped.
// c is the typed character; upper case counts as lower case (the mockup lowercases e.key).
int keyboardNoteForChar(uint32_t c);

// ---- wheels. Positions are 0..1 (1 = top). BEND rests at 0.5, MOD at 0.
float bendWheelNorm(int bend);                            // bend -64..63 (the P_bend lever value) to wheel position
int bendFromWheelNorm(float n);                           // wheel position to bend value (rounded like the mockup's setP)
float modWheelNorm(int mod);                              // mod 0..127
int modFromWheelNorm(float n);
float bendWheelCents(int bend, int bendUp, int bendDown); // what the BEND UP / BEND DOWN range makes of it (bendDown is negative)
float clamp01(float n);

// Drag: new position from the start position and the pointer travel up (positive dy, same unit as height). The mockup moves the
// wheel by travel / (0.8 * wheel height).
float wheelDragNorm(float startNorm, float travelUp, float wheelHeight);
// Mouse wheel and arrow keys on a wheel (mockup): returns the new position. dir is +1 or -1.
float bendWheelStep(float norm, int dir, bool page);      // BEND: 0.08 per step, 0.25 per page
float modWheelScroll(float norm, int dir, bool fine);     // scroll: 6 (shift: 1) of 127
float modWheelKey(float norm, int dir, bool page, bool fine); // arrows: 4 (shift: 1) of 127, page keys 16
// BEND spring back after release: linear over kSpringMs. Returns the bend value at tMs after the release started from v0.
constexpr float kSpringMs = 140.0f;
int bendSpringValue(int v0, float tMs);
// The handle slides with a CSS transition (0.12 s ease-out) except while dragging: eased position at tMs since the target changed.
constexpr float kHandleMs = 120.0f;
float handleEase(float from, float to, float tMs);
// Text of the value bubble: "BEND +1.0 st" or "MOD  64" (two spaces, as the mockup).
int wheelBubbleText(bool mod, int value, int bendUp, int bendDown, char* out, int cap);

// ---- notes sounding from the strip. A note already sounding is not started again; releasing one that is not sounding does nothing.
class HeldNotes {
public:
    bool press(int note) { if (!valid(note) || on_[note]) return false; on_[note] = true; ++n_; return true; }
    bool release(int note) { if (!valid(note) || !on_[note]) return false; on_[note] = false; --n_; return true; }
    bool isDown(int note) const { return valid(note) && on_[note]; }
    int count() const { return n_; }
    // Calls f(note) for every sounding note and clears them all.
    template <class F> void releaseAll(F f) { for (int i = 0; i < 128; ++i) if (on_[i]) { on_[i] = false; f(i); } n_ = 0; }
private:
    static bool valid(int n) { return n >= 0 && n < 128; }
    bool on_[128] = {};
    int n_ = 0;
};

// Which key is at a point. Keys are given as white keys first, then black keys (the draw order), black keys win where they overlap.
struct KeyBox { int note; bool black; float x, y, w, h; };
int keyAtPoint(const KeyBox* keys, int n, float x, float y);   // note, or -1

}
