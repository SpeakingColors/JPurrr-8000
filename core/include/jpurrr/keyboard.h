// Logic of the on-screen keyboard strip (BEND and MOD wheels, 61 keys, computer keys). Pure C++17, no JUCE, no allocation.
// Everything here is a port of the mockup's strip code (docs/mockups/JPurrr-8000-mockup-v3.7.html: CKEY, CKCMD, ckDown, ckUp, bendNorm, bendCents,
// setN, springBack, wheel and key handlers) so the plugin behaves like the mockup.
#pragma once
#include <cstdint>

namespace jpurrr {

constexpr int kKeyFirstNote = 36, kKeyLastNote = 96;      // C2..C7, 61 keys
constexpr int kKeyVelocity = 100;                         // velocity of notes played with the mouse, and the start value of the computer keys
constexpr int kKeyChannel = 15;                           // channel index the synth uses for strip notes (they play whatever the part channels are)

// ---- computer keys, like Ableton Live's Computer MIDI Keyboard (mockup v3.7: CKEY, CKCMD, ckDown, ckUp).
// A S D F G H J K L ; ' are the white keys, W E T Y U O P the black keys: 18 notes from the base note (A = base).
// Z / X move the octave down / up, C / V lower / raise the velocity in steps of 20 (Live: "intervals of twenty").
// The base note starts at 48 = C3 (the name the strip prints on its keys, C4 = 60) and moves in whole octaves from 0 (C-1) to 108 (C8),
// so the 18 notes always stay inside MIDI 0..127.
constexpr int kCkSpan = 18, kCkBaseDefault = 48, kCkBaseMin = 0, kCkBaseMax = 108;
enum class CkCmd { None, Note, OctaveDown, OctaveUp, VelocityDown, VelocityUp };
struct CkAction { CkCmd cmd = CkCmd::None; int offset = 0; };      // offset: 0..17 above the base for a note
// What a typed character means; upper case counts as lower case. Letters are matched as typed (QWERTY positions, see ComputerKeys).
CkAction computerKeyAction(uint32_t c);
int ckStepOctave(int base, int dir);                               // base moved one octave up (+1) or down (-1), clamped to kCkBaseMin..kCkBaseMax
int ckStepVelocity(int vel, int dir);                              // next step of 20, 40, 60, 80, 100, 120, 127 (clamped); any other value moves to the nearest step first
int ckNoteName(int note, char* out, int cap);                      // "C3", "C#4", "C-1": 60 = C4 (as the strip prints it)
// "C3 - F4": text of the KEYS readout for a base note.
int ckKeysText(int base, char* out, int cap);

// The computer keys that are down. A key plays the note it had when it went down; Z / X first release every note of the old octave (the keys stay
// down but silent until they are let go), so no note sticks. Auto repeat of a key that is already down does nothing. Calls f(...) for the synth.
class ComputerKeys {
public:
    int base() const { return base_; }
    int velocity() const { return vel_; }
    int lowNote() const { return base_; }
    int highNote() const { return base_ + kCkSpan - 1; }
    int keysDown() const { return n_; }
    bool isDown(int code) const { return find(code) >= 0; }
    bool plays(int note) const { for (int i = 0; i < n_; ++i) if (slot_[i].note == note) return true; return false; }   // a key down that has this note
    void reset() { n_ = 0; base_ = kCkBaseDefault; vel_ = kKeyVelocity; }
    void setBase(int b) { base_ = b < kCkBaseMin ? kCkBaseMin : b > kCkBaseMax ? kCkBaseMax : b; }
    void setVelocity(int v) { vel_ = v < 1 ? 1 : v > 127 ? 127 : v; }
    // A key goes down. code identifies the physical key (anything unique), ch is the character it typed. Returns true when the key is one of ours
    // (also for auto repeat). on(note, velocity) starts a note, off(note) ends one.
    template <class On, class Off> bool press(int code, uint32_t ch, On on, Off off) {
        const CkAction a = computerKeyAction(ch);
        if (a.cmd == CkCmd::None) return false;
        if (find(code) >= 0 || n_ >= kMax) return true;
        if (a.cmd == CkCmd::Note) { const int note = base_ + a.offset; slot_[n_++] = { code, note }; on(note, vel_); return true; }
        slot_[n_++] = { code, -1 };
        if (a.cmd == CkCmd::OctaveDown || a.cmd == CkCmd::OctaveUp) {
            const int b = ckStepOctave(base_, a.cmd == CkCmd::OctaveUp ? 1 : -1);
            if (b != base_) { silence(off); base_ = b; }
        } else vel_ = ckStepVelocity(vel_, a.cmd == CkCmd::VelocityUp ? 1 : -1);
        return true;
    }
    // A key came up (or is gone). off(note) ends its note if it still has one.
    template <class Off> void release(int code, Off off) {
        const int i = find(code); if (i < 0) return;
        const int note = slot_[i].note; slot_[i] = slot_[--n_];
        if (note >= 0) off(note);
    }
    // Releases every key for which isUp(code) is true.
    template <class Up, class Off> void releaseIf(Up isUp, Off off) {
        for (int i = 0; i < n_;) {
            if (!isUp(slot_[i].code)) { ++i; continue; }
            const int note = slot_[i].note; slot_[i] = slot_[--n_];
            if (note >= 0) off(note);
        }
    }
    template <class Off> void releaseAll(Off off) { silence(off); n_ = 0; }
    // Calls f(code) for every key that is down.
    template <class F> void forEachKey(F f) const { for (int i = 0; i < n_; ++i) f(slot_[i].code); }
private:
    static constexpr int kMax = 32;
    struct Slot { int code; int note; };
    Slot slot_[kMax] = {};
    int n_ = 0, base_ = kCkBaseDefault, vel_ = kKeyVelocity;
    int find(int code) const { for (int i = 0; i < n_; ++i) if (slot_[i].code == code) return i; return -1; }
    template <class Off> void silence(Off off) { for (int i = 0; i < n_; ++i) if (slot_[i].note >= 0) { const int n = slot_[i].note; slot_[i].note = -1; off(n); } }
};

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
