// The factory bank: names, order, and a level check of every factory sound on a low, a middle and a high note.
// The level check is the same one tools/render_synth does by hand (--preset N --notes "NOTE@0+2" --len 3, velocity 100).
#include "test_util.h"
#include "jpurrr/patch.h"
#include "jpurrr/synth.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <vector>
using namespace jpurrr;

namespace {
// The 51 sounds of the first release, in their slots. They must stay where they are (program changes and saved sessions point at them).
const char* const kFirstFifty[51] = {
    "Anthem Cat", "Purrfect Climax", "Sharp Claws", "Goa Gato", "Tom Cat Hook", "Alley Cat Scream", "Feedback Fang", "Soft Triangle Tale", "Moonlit Meow Pad",
    "Nine Lives Pad", "Catnap Strings", "Velvet Paws", "Breakdown Fog", "Midnight Window", "Sunrise Stack", "Purr Cloud", "Rolling Tom", "Sub Whisker",
    "Acid Tail", "Psy Purr", "Offbeat Panther", "Gated Growler", "Rubber Whisker", "Click Claw", "Paw Arp", "Kitten Pluck", "Yarn Bell", "Trance Tickle",
    "Chasing Tails", "Laser Mouse", "Glass Whisker", "Hollow Hum", "Stab Claw", "Pounce Chord", "Trance Gate Chord", "Saw Stack Stab", "Organ Rave Stab",
    "Ceiling Chords", "Rave Scoop", "Ooh Pluck", "Hiss and Rise", "Falling Meow", "Thump Whisker", "Siren Alley", "Zap Whisker", "Chopper Cat", "Echo Spiral",
    "Glass Rain", "Pipe Cat Organ", "Knock Whisker", "Gated Cloud" };
constexpr int kOldCount = 51;

struct Level { double peakDb = -200, dc = 0; bool finite = true; };

// One note through the synth alone (the MIX READY chain of the plugin is not part of it): 2 s held, 0.3 s of release, 48 kHz.
Level renderNote(const Patch& p, int note) {
    const int sr = 48000, block = 512, total = sr * 23 / 10, off = 2 * sr;
    Synth s; s.prepare((double) sr, block); s.setParams(p.v);
    std::vector<float> l((size_t) block), r((size_t) block);
    Level lv; double pk = 0, sum = 0;
    s.noteOn(0, note, 100);
    for (int pos = 0; pos < total; pos += block) {
        if (pos < off && pos + block > off) s.noteOff(0, note);
        s.process(l.data(), r.data(), block);
        for (int i = 0; i < block; ++i) {
            if (!std::isfinite(l[(size_t) i]) || !std::isfinite(r[(size_t) i])) { lv.finite = false; continue; }
            pk = std::max(pk, (double) std::max(std::fabs(l[(size_t) i]), std::fabs(r[(size_t) i])));
            sum += 0.5 * ((double) l[(size_t) i] + (double) r[(size_t) i]);
        }
    }
    lv.peakDb = 20.0 * std::log10(pk + 1e-12);
    lv.dc = sum / (double) total;
    return lv;
}
}  // namespace

TEST(factory_bank_names_and_order) {
    CHECK(factoryCount() >= kOldCount);
    std::set<std::string> seen;
    for (int i = 0; i < factoryCount(); ++i) {
        const std::string& n = factoryPatch(i).name;
        CHECK(!n.empty());
        if (i >= kOldCount) CHECK(n.size() <= 16);              // the JP-8000 patch name has 16 characters (two of the first 51 are longer; a SysEx export shortens them)
        for (unsigned char c : n) CHECK(c >= 0x20 && c <= 0x7D);  // and its character set ends at 7Dh (sysex.cpp encodeName)
        CHECK(seen.insert(n).second);                            // names are unique
        if (i < kOldCount) CHECK(n == kFirstFifty[i]);
    }
}

TEST(factory_bank_levels) {
    // Every factory sound on C2 (36), C4 (60) and C6 (84): finite, no clipping, no DC, not silent. The sounds added after the first
    // fifty-one must also peak between -12 and -3 dBFS on every one of the three notes and stay within 6 dB of each other.
    // (The first fifty-one are left as they were; the quiet ones among them are listed in ROADMAP.md.)
    int bad = 0;
    for (int i = 0; i < factoryCount(); ++i) {
        const Patch& p = factoryPatch(i);
        double lo = 1e9, hi = -1e9;
        for (int note : { 36, 60, 84 }) {
            const Level lv = renderNote(p, note);
            lo = std::min(lo, lv.peakDb); hi = std::max(hi, lv.peakDb);
            bool ok = lv.finite && lv.peakDb < -0.5 && lv.peakDb > -60.0 && std::fabs(lv.dc) < 0.05;   // the first 51: only gross DC (Feedback Fang has 0.02 to 0.03)
            if (i >= kOldCount) ok = ok && lv.peakDb >= -12.0 && lv.peakDb <= -3.0 && std::fabs(lv.dc) < 0.01;
            if (!ok) { std::printf("      level: #%d '%s' note %d: peak %.1f dBFS, dc %.4f%s\n", i + 1, p.name.c_str(), note, lv.peakDb, lv.dc, lv.finite ? "" : ", NaN/inf"); ++bad; }
        }
        if (i >= kOldCount && hi - lo > 6.0) { std::printf("      level: #%d '%s' differs by %.1f dB between C2, C4 and C6\n", i + 1, p.name.c_str(), hi - lo); ++bad; }
    }
    CHECK(bad == 0);
}
