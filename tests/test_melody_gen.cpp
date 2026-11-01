// The melody generator: exactly the same notes as the mockup's JavaScript (fixture written by tools/make_melody_fixture.js),
// deterministic, and valid over many seeds (the rules of the 22 hand written melodies).
#include "jpurrr/melody_gen.h"
#include "test_util.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
using namespace jpurrr;

TEST(melody_gen_matches_the_mockup) {
    std::ifstream f(JPURRR_SOURCE_DIR "/tests/data/melody_gen_fixture.txt");
    CHECK(f.good());
    std::string line; int cases = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        ++cases;
        std::istringstream ss(line);
        int seed, key, mode, style; char bar;
        ss >> seed >> key >> mode >> style >> bar;
        int bpm, minor, styleUsed, tries; ss >> bpm >> minor >> styleUsed >> tries >> bar;
        std::string name; std::getline(ss, name, '|');
        while (!name.empty() && (name.front() == ' ')) name.erase(0, 1);
        while (!name.empty() && (name.back() == ' ')) name.pop_back();
        const auto g = melodyGenerate(seed, key, mode, style);
        CHECK(g.ok);
        if (!g.ok) continue;
        CHECK(g.bpm == bpm);
        CHECK((g.minor ? 1 : 0) == minor);
        CHECK(g.style == styleUsed);
        CHECK(g.tries == tries);
        CHECK(std::string(g.name) == name);
        int k = 0; bool evOk = true;
        std::string tok;
        while (ss >> tok) {
            int bq, lm, nt, vl;
            if (std::sscanf(tok.c_str(), "%d:%d:%d:%d", &bq, &lm, &nt, &vl) != 4) { evOk = false; break; }
            if (k >= g.n) { evOk = false; break; }
            const PlayEvent& e = g.ev[k++];
            if ((int) std::lround(e.beat * 4) != bq || (int) std::lround(e.len * 1000) != lm || e.note != nt || e.vel != vl) { evOk = false; break; }
        }
        CHECK(evOk);
        CHECK(k == g.n);
    }
    CHECK(cases >= 50);
}

TEST(melody_gen_valid_and_deterministic) {
    for (int i = 1; i <= 400; ++i) {
        const int seed = (i * 104729) % 16777216 + 1, key = i % 12, mode = i % 3, style = i % 6;
        const auto a = melodyGenerate(seed, key, mode, style);
        CHECK(a.ok);
        if (!a.ok) continue;
        const auto b = melodyGenerate(seed, key, mode, style);
        CHECK(a.n == b.n && std::memcmp(a.ev, b.ev, sizeof(PlayEvent) * (size_t) a.n) == 0 && std::strcmp(a.name, b.name) == 0);
        CHECK(a.n >= 14 && a.n <= 136);
        CHECK(a.beats == 32.0);
        CHECK(a.bpm >= 136 && a.bpm <= 140);
        int prev = -1;
        bool fine = true;
        for (int k = 0; k < a.n; ++k) {
            const PlayEvent& e = a.ev[k];
            if (!(e.beat >= 0.0f && e.beat < 32.0f && e.len > 0.0f && e.vel >= 1 && e.vel <= 127 && e.note >= 48 && e.note <= 98)) fine = false;
            if (prev >= 0 && std::abs((int) e.note - prev) > 12) fine = false;
            prev = (int) e.note;
        }
        CHECK(fine);
        if (mode == 0) CHECK(a.minor);
        if (mode == 1) CHECK(!a.minor);
        if (style >= 1) CHECK(a.style == style);
    }
}

TEST(melody_gen_bad_input_is_clamped) {
    const auto a = melodyGenerate(0, -5, 7, 99);          // seed 0 -> 1, key wraps, style clamps
    CHECK(a.ok && a.seed == 1 && a.key == 7 && a.style == 5);
    const auto b = melodyGenerate(16777216 + 3, 0, 0, 0); // seed wraps to 3
    CHECK(b.ok && b.seed == 3);
}
