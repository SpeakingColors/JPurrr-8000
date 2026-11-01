// Melody generator: writes a new 8 bar uplifting trance melody in the style of the 22 melodies of the player library.
// The same algorithm as the mockup's tools/melody_gen.js: the same seed, key, mode and style give exactly the same notes
// (tests/test_melody_gen.cpp checks this against a fixture written by the JavaScript side).
// Nothing is copied from the 22 melodies: only their bar rhythms and chord loops are used, the notes are chosen new.
// Pure C++17, no allocation: safe to call on the message thread; the result is a value.
#pragma once
#include "player.h"

namespace jpurrr {

struct MelodyGenResult {
    bool ok = false;
    char name[128] = {};            // "Generated #<seed>, Am-F-C-G-..."
    char chords[112] = {};          // "Am F C G Am F G Am" for the info line
    int bpm = 138;
    double beats = 32.0;
    PlayEvent ev[136]; int n = 0;  // at most 8 bars of 16 sixteenths
    int seed = 1, key = 0, style = 1;   // style actually used, 1..5
    bool minor = true;
    int tries = 0;
};

// seed 1..16777215 (0 becomes 1), key 0..11 (C = 0), mode 0 minor / 1 major / 2 either,
// style 0 any / 1 anthem / 2 run / 3 roller / 4 stabs / 5 sparse.
MelodyGenResult melodyGenerate(int seed, int key, int mode, int style);

const char* melodyStyleName(int style);     // "Any", "Anthem", ... for style 0..5
const char* melodyKeyName(int key, bool minorNames);   // "C".."B" (Eb/C# spelling as the mockup)

}  // namespace jpurrr
