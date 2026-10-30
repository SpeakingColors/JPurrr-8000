// Tests for the keyboard strip logic (core/src/keyboard.cpp) and the strip notes of the synth.
#include "test_util.h"
#include "jpurrr/keyboard.h"
#include "jpurrr/patch.h"
#include "jpurrr/synth.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
using namespace jpurrr;

// Every computer key like Ableton Live's Computer MIDI Keyboard (and mockup v3.7).
TEST(kb_computer_keys_like_ableton) {
    const char* keys = "awsedftgyhujkolp;'";                 // A W S E D F T G Y H U J K O L P ; ' = C C# D D# E F F# G G# A A# B C C# D D# E F
    for (int i = 0; i < 18; ++i) {
        const CkAction a = computerKeyAction((uint32_t) keys[i]);
        CHECK(a.cmd == CkCmd::Note && a.offset == i);
        const CkAction u = computerKeyAction((uint32_t) std::toupper(keys[i]));
        CHECK(u.cmd == CkCmd::Note && u.offset == i);          // upper case (Shift, Caps Lock) counts
    }
    CHECK(computerKeyAction('z').cmd == CkCmd::OctaveDown && computerKeyAction('x').cmd == CkCmd::OctaveUp);
    CHECK(computerKeyAction('c').cmd == CkCmd::VelocityDown && computerKeyAction('v').cmd == CkCmd::VelocityUp);
    CHECK(computerKeyAction('Z').cmd == CkCmd::OctaveDown && computerKeyAction('V').cmd == CkCmd::VelocityUp);
    for (char ch : std::string("bnmqir,.-/[]\\1234567890 =`@")) CHECK(computerKeyAction((uint32_t) ch).cmd == CkCmd::None);   // the old keys are gone
    CHECK(computerKeyAction(0x00e9).cmd == CkCmd::None && computerKeyAction(0).cmd == CkCmd::None);
    // the white keys of the middle row are the white keys of a piano, the upper row the black keys
    const int whites[11] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 }, blacks[7] = { 1, 3, 6, 8, 10, 13, 15 };
    const char* wk = "asdfghjkl;'"; const char* bk = "wetyuop";
    for (int i = 0; i < 11; ++i) CHECK(computerKeyAction((uint32_t) wk[i]).offset == whites[i]);
    for (int i = 0; i < 7; ++i) CHECK(computerKeyAction((uint32_t) bk[i]).offset == blacks[i]);
}

TEST(kb_computer_keys_octave_and_velocity_steps) {
    CHECK(kCkBaseDefault == 48 && kCkBaseMin == 0 && kCkBaseMax == 108);
    CHECK(ckStepOctave(48, 1) == 60 && ckStepOctave(48, -1) == 36 && ckStepOctave(48, 0) == 48);
    CHECK(ckStepOctave(0, -1) == 0 && ckStepOctave(108, 1) == 108);                     // bounds
    for (int b = kCkBaseMin; b <= kCkBaseMax; b += 12) CHECK(b + kCkSpan - 1 <= 127 && b >= 0);   // the 18 notes always fit MIDI 0..127
    CHECK(kCkBaseMax + 12 + kCkSpan - 1 > 127);                                         // and one more octave would not
    CHECK(ckStepVelocity(100, 1) == 120 && ckStepVelocity(100, -1) == 80);
    CHECK(ckStepVelocity(120, 1) == 127 && ckStepVelocity(127, 1) == 127 && ckStepVelocity(127, -1) == 120);
    CHECK(ckStepVelocity(20, -1) == 20 && ckStepVelocity(40, -1) == 20);
    int v = 100; for (int i = 0; i < 9; ++i) v = ckStepVelocity(v, -1); CHECK(v == 20);
    for (int i = 0; i < 9; ++i) v = ckStepVelocity(v, 1); CHECK(v == 127);
    CHECK(ckStepVelocity(85, 1) == 100 && ckStepVelocity(85, -1) == 60);                  // an odd value joins the nearest step first
    char b[32];
    ckKeysText(48, b, 32); CHECK(std::string(b) == "C3 - F4");
    ckKeysText(60, b, 32); CHECK(std::string(b) == "C4 - F5");
    ckKeysText(0, b, 32);  CHECK(std::string(b) == "C-1 - F0");
    ckKeysText(108, b, 32); CHECK(std::string(b) == "C8 - F9");
    ckNoteName(61, b, 32); CHECK(std::string(b) == "C#4");
}

namespace {
struct Log {
    std::vector<std::pair<int, int>> on; std::vector<int> off; int sounding = 0;
    bool hasOn(int n) const { for (auto& o : on) if (o.first == n) return true; return false; }
    int countOn(int n) const { int c = 0; for (auto& o : on) if (o.first == n) ++c; return c; }
};
}
TEST(kb_computer_keys_play_every_key_in_every_octave) {
    for (int base = kCkBaseMin; base <= kCkBaseMax; base += 12) {
        ComputerKeys ck; ck.setBase(base); Log lg; const char* keys = "awsedftgyhujkolp;'";
        auto on = [&](int n, int v) { lg.on.push_back({ n, v }); ++lg.sounding; };
        auto off = [&](int) { --lg.sounding; };
        for (int i = 0; i < 18; ++i) CHECK(ck.press(1000 + i, (uint32_t) keys[i], on, off));
        CHECK(lg.on.size() == 18 && lg.sounding == 18);
        for (int i = 0; i < 18; ++i) CHECK(lg.on[(size_t) i].first == base + i && lg.on[(size_t) i].second == kKeyVelocity);
        CHECK(ck.lowNote() == base && ck.highNote() == base + 17 && ck.highNote() <= 127);
        for (int i = 0; i < 18; ++i) ck.release(1000 + i, off);
        CHECK(lg.sounding == 0 && ck.keysDown() == 0);
    }
}

TEST(kb_computer_keys_octave_change_hands_off_the_held_notes) {
    ComputerKeys ck; Log lg; std::vector<int> sounding;
    auto on = [&](int n, int v) { lg.on.push_back({ n, v }); sounding.push_back(n); };
    auto off = [&](int n) { auto it = std::find(sounding.begin(), sounding.end(), n); CHECK(it != sounding.end()); if (it != sounding.end()) sounding.erase(it); lg.off.push_back(n); };
    ck.press('a', 'a', on, off); ck.press('d', 'd', on, off);                      // C3 and E3 held
    CHECK(sounding.size() == 2);
    CHECK(ck.press('x', 'x', on, off));                                            // octave up: the old notes end first
    CHECK(ck.base() == 60 && sounding.empty() && lg.off.size() == 2);
    ck.press('s', 's', on, off);                                                    // new octave: D4
    CHECK(sounding.size() == 1 && sounding[0] == 62);
    ck.release('a', off); ck.release('d', off);                                     // the old keys come up: nothing more to end
    CHECK(sounding.size() == 1 && lg.off.size() == 2);
    ck.press('a', 'a', on, off);                                                    // pressed again: C4 plays
    CHECK(sounding.size() == 2 && sounding[1] == 60);
    ck.release('s', off); ck.release('a', off); ck.release('x', off);
    CHECK(sounding.empty() && ck.keysDown() == 0);
    // octave down twice: 60 -> 48 -> 36
    ck.press('q', 'z', on, off); ck.release('q', off); ck.press('q', 'z', on, off); ck.release('q', off);
    CHECK(ck.base() == 36);
}

TEST(kb_computer_keys_auto_repeat_and_bounds) {
    ComputerKeys ck; int ons = 0, offs = 0, base0 = ck.base();
    auto on = [&](int, int) { ++ons; }; auto off = [&](int) { ++offs; };
    CHECK(ck.press(1, 'a', on, off) && ons == 1);
    for (int i = 0; i < 10; ++i) CHECK(ck.press(1, 'a', on, off));                 // key repeat: handled, nothing retriggers
    CHECK(ons == 1 && offs == 0);
    ck.press(2, 'x', on, off); for (int i = 0; i < 5; ++i) ck.press(2, 'x', on, off);   // a held octave key does not repeat the shift
    CHECK(ck.base() == base0 + 12 && offs == 1);
    ck.release(2, off); ck.release(1, off); CHECK(offs == 1 && ck.keysDown() == 0);
    // hold Z down at the bottom and X at the top: the base stops
    for (int i = 0; i < 20; ++i) { ck.press(3, 'z', on, off); ck.release(3, off); }
    CHECK(ck.base() == 0);
    ck.press(4, 'a', on, off); const int o = offs; ck.press(5, 'z', on, off);      // already at the bottom: the held note keeps sounding
    CHECK(ck.base() == 0 && offs == o);
    ck.releaseAll(off); CHECK(ck.keysDown() == 0 && offs == o + 1);
    for (int i = 0; i < 20; ++i) { ck.press(3, 'x', on, off); ck.release(3, off); }
    CHECK(ck.base() == 108);
    // velocity: C and V change the velocity of the next notes only
    ck.reset(); std::vector<int> vels; auto on2 = [&](int, int v) { vels.push_back(v); };
    ck.press(1, 'a', on2, off); ck.release(1, off);
    ck.press(2, 'v', on2, off); ck.release(2, off); ck.press(1, 'a', on2, off); ck.release(1, off);
    ck.press(2, 'c', on2, off); ck.release(2, off); ck.press(2, 'c', on2, off); ck.release(2, off); ck.press(1, 'a', on2, off); ck.release(1, off);
    CHECK(vels.size() == 3 && vels[0] == 100 && vels[1] == 120 && vels[2] == 80 && ck.velocity() == 80);
    // a key that is not ours is not handled; releaseIf lets go of the keys that are up
    CHECK(!ck.press(9, 'b', on, off) && ck.keysDown() == 0);
    ck.press(10, 'a', on, off); ck.press(11, 's', on, off); const int o2 = offs;
    ck.releaseIf([](int code) { return code == 10; }, off);
    CHECK(ck.keysDown() == 1 && ck.isDown(11) && !ck.isDown(10) && offs == o2 + 1);
}

TEST(kb_wheel_conversions) {
    CHECK(std::fabs(bendWheelNorm(0) - 0.5f) < 1e-6f && std::fabs(bendWheelNorm(63) - 1.f) < 1e-6f && std::fabs(bendWheelNorm(-64) - 0.f) < 1e-6f);
    for (int b = -64; b <= 63; ++b) CHECK(bendFromWheelNorm(bendWheelNorm(b)) == b);       // round trip
    CHECK(bendFromWheelNorm(0.5f) == 0 && bendFromWheelNorm(1.f) == 63 && bendFromWheelNorm(0.f) == -64);
    CHECK(bendFromWheelNorm(2.f) == 63 && bendFromWheelNorm(-1.f) == -64);                   // clamped
    for (int m = 0; m <= 127; ++m) CHECK(modFromWheelNorm(modWheelNorm(m)) == m);
    CHECK(modFromWheelNorm(1.5f) == 127 && modFromWheelNorm(-1.f) == 0);
    // range: BEND UP +2 st at the top, BEND DOWN -2 st at the bottom; ranges follow the params
    CHECK(std::fabs(bendWheelCents(63, 2, -2) - 200.f) < 1e-3f && std::fabs(bendWheelCents(-64, 2, -2) + 200.f) < 1e-3f);
    CHECK(std::fabs(bendWheelCents(63, 12, -2) - 1200.f) < 1e-3f && std::fabs(bendWheelCents(-64, 12, -7) + 700.f) < 1e-3f);
    CHECK(bendWheelCents(0, 12, -12) == 0.f);
}

TEST(kb_wheel_drag_scroll_keys) {
    // drag: 0.8 of the wheel height is the full travel
    CHECK(std::fabs(wheelDragNorm(0.5f, 10.24f, 128.f) - 0.6f) < 1e-5f);
    CHECK(wheelDragNorm(0.9f, 500.f, 128.f) == 1.f && wheelDragNorm(0.1f, -500.f, 128.f) == 0.f);
    CHECK(std::fabs(bendWheelStep(0.5f, 1, false) - 0.58f) < 1e-6f && std::fabs(bendWheelStep(0.5f, -1, true) - 0.25f) < 1e-6f);
    CHECK(bendWheelStep(0.95f, 1, false) == 1.f);
    CHECK(std::fabs(modWheelScroll(0.f, 1, false) - 6.f / 127) < 1e-6f && std::fabs(modWheelScroll(0.5f, -1, true) - (0.5f - 1.f / 127)) < 1e-6f);
    CHECK(modWheelScroll(0.f, -1, false) == 0.f);
    CHECK(std::fabs(modWheelKey(0.f, 1, false, false) - 4.f / 127) < 1e-6f && std::fabs(modWheelKey(0.f, 1, true, false) - 16.f / 127) < 1e-6f
          && std::fabs(modWheelKey(0.f, 1, false, true) - 1.f / 127) < 1e-6f);
}

TEST(kb_bend_spring_back) {
    CHECK(bendSpringValue(63, 0.f) == 63 && bendSpringValue(-64, 0.f) == -64);
    CHECK(bendSpringValue(63, kSpringMs) == 0 && bendSpringValue(-64, 1000.f) == 0);
    int prev = 63; for (float t = 0; t <= kSpringMs; t += 5) { const int v = bendSpringValue(63, t); CHECK(v <= prev && v >= 0); prev = v; }
    CHECK(std::abs(bendSpringValue(40, kSpringMs / 2) - 20) <= 1);
    CHECK(bendSpringValue(0, 50.f) == 0);
}

TEST(kb_handle_ease) {
    CHECK(std::fabs(handleEase(0.f, 1.f, 0.f)) < 1e-4f && std::fabs(handleEase(0.f, 1.f, kHandleMs) - 1.f) < 1e-4f && std::fabs(handleEase(2.f, 4.f, 999.f) - 4.f) < 1e-4f);
    float prev = 0.f; for (float t = 0; t <= kHandleMs; t += 6) { const float v = handleEase(0.f, 1.f, t); CHECK(v >= prev - 1e-6f); prev = v; }
    CHECK(handleEase(0.f, 1.f, kHandleMs / 2) > 0.5f);          // ease-out: past halfway at half time
}

TEST(kb_bubble_text) {
    char b[64];
    wheelBubbleText(true, 64, 2, -2, b, 64); CHECK(std::string(b) == "MOD  64");
    wheelBubbleText(false, 0, 2, -2, b, 64); CHECK(std::string(b) == "BEND  0.0 st");
    wheelBubbleText(false, 63, 2, -2, b, 64); CHECK(std::string(b) == "BEND  +2.0 st");
    wheelBubbleText(false, -64, 2, -2, b, 64); CHECK(std::string(b) == "BEND  -2.0 st");
    wheelBubbleText(false, 32, 12, -2, b, 64); CHECK(std::string(b) == "BEND  +6.1 st");
}

TEST(kb_held_notes) {
    HeldNotes h;
    CHECK(h.press(60) && !h.press(60) && h.count() == 1);        // a sounding note is not started again
    CHECK(h.press(61) && h.count() == 2 && h.isDown(61));
    CHECK(!h.release(70) && h.count() == 2);                      // releasing a note that is not sounding does nothing
    CHECK(h.release(60) && !h.isDown(60) && h.count() == 1);
    CHECK(!h.press(-1) && !h.press(128));
    h.press(40); h.press(96);
    std::vector<int> off; h.releaseAll([&](int n) { off.push_back(n); });
    CHECK(off.size() == 3 && off[0] == 40 && off[1] == 61 && off[2] == 96 && h.count() == 0 && !h.isDown(61));
}

TEST(kb_key_hit_test) {
    // two white keys, a black key over the seam
    const KeyBox keys[] = { { 36, false, 0, 0, 10, 20 }, { 38, false, 10, 0, 10, 20 }, { 37, true, 7, 0, 6, 12 } };
    CHECK(keyAtPoint(keys, 3, 2, 5) == 36 && keyAtPoint(keys, 3, 15, 15) == 38);
    CHECK(keyAtPoint(keys, 3, 9, 5) == 37 && keyAtPoint(keys, 3, 12, 5) == 37);   // black wins where it overlaps
    CHECK(keyAtPoint(keys, 3, 9, 15) == 36);                                      // below the black key
    CHECK(keyAtPoint(keys, 3, 25, 5) == -1 && keyAtPoint(keys, 3, 5, -1) == -1);
}

TEST(kb_strip_notes_ignore_part_channels) {
    // Both parts listen to channel 5 only: a MIDI note on channel 1 is ignored, a strip note plays.
    Synth s; s.prepare(48000.0, 512);
    int p[kNumParams]; std::memcpy(p, factoryPatch(0).v, sizeof p);
    p[P_pch] = 5; s.setParams(p);
    std::vector<float> l(512), r(512);
    s.noteOn(0, 60, 100); s.process(l.data(), r.data(), 512);
    CHECK(s.activeVoices() == 0);
    s.stripNoteOn(60, kKeyVelocity);
    float pk = 0; for (int i = 0; i < 20; ++i) { s.process(l.data(), r.data(), 512); for (float v : l) pk = std::max(pk, std::fabs(v)); }
    CHECK(s.activeVoices() > 0 && s.heldVoices() > 0 && pk > 0.001f);
    s.stripNoteOff(60); s.process(l.data(), r.data(), 512);
    CHECK(s.heldVoices() == 0);
    // all notes off ends a held strip note too
    s.stripNoteOn(64, kKeyVelocity); s.process(l.data(), r.data(), 512); CHECK(s.heldVoices() > 0);
    s.allNotesOff(); s.process(l.data(), r.data(), 512); CHECK(s.heldVoices() == 0);
}
