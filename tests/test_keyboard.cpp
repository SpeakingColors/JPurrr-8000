// Tests for the keyboard strip logic (core/src/keyboard.cpp) and the strip notes of the synth.
#include "test_util.h"
#include "jpurrr/keyboard.h"
#include "jpurrr/patch.h"
#include "jpurrr/synth.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
using namespace jpurrr;

TEST(kb_computer_keys_like_the_mockup) {
    const char* lower = "zsxdcvgbhnjm";
    for (int i = 0; i < 12; ++i) CHECK(keyboardNoteForChar((uint32_t) lower[i]) == 48 + i);
    CHECK(keyboardNoteForChar(',') == 60 && keyboardNoteForChar('q') == 60);
    const char* upper = "q2w3er5t6y7ui";
    for (int i = 0; i < 13; ++i) CHECK(keyboardNoteForChar((uint32_t) upper[i]) == 60 + i);
    CHECK(keyboardNoteForChar('Z') == 48 && keyboardNoteForChar('Q') == 60);   // upper case counts
    CHECK(keyboardNoteForChar('a') == -1 && keyboardNoteForChar(' ') == -1 && keyboardNoteForChar('4') == -1 && keyboardNoteForChar('@') == -1);
    CHECK(keyboardNoteForChar(0x00e9) == -1);
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
