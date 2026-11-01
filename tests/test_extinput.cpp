// The EXT switch on OSC 2: the external input plays through the voice (balance, filter, envelopes), gated by the keys.
#include "test_util.h"
#include "jpurrr/patch.h"
#include "jpurrr/synth.h"
#include <cmath>
#include <cstring>
#include <vector>
using namespace jpurrr;

namespace {
struct RigX {
    Synth s; std::vector<float> l, r, ext; int p[kNumParams];
    explicit RigX(double sr = 48000.0) : l(512), r(512), ext(512) {
        s.prepare(sr, 512);
        std::memcpy(p, factoryPatch(0).v, sizeof p);
        p[P_ob] = 63; p[P_fc] = 127; p[P_fr] = 0; p[P_fe] = 0; p[P_ol] = 0; p[P_fl] = 0; p[P_xt] = 0; p[P_dl] = 0;
        p[P_o2ext] = 1;
        p[P_aea] = 0; p[P_aer] = 20;
        s.setParams(p);
    }
    void set(int idx, int v) { p[idx] = v; s.setParams(p); }
    // renders blocks of 512 with a 220.5 Hz sine as the external input; returns rms per block
    std::vector<float> run(int blocks, bool feed, float amp = 0.5f) {
        std::vector<float> out;
        double ph = 0;
        for (int b = 0; b < blocks; ++b) {
            for (int i = 0; i < 512; ++i) { ext[(size_t) i] = amp * (float) std::sin(ph); ph += 2.0 * 3.14159265358979 * 220.5 / 48000.0; }
            if (feed) s.setExternalInput(ext.data());
            s.process(l.data(), r.data(), 512);
            double a = 0; for (int i = 0; i < 512; ++i) a += (double) l[(size_t) i] * l[(size_t) i];
            out.push_back((float) std::sqrt(a / 512.0));
        }
        return out;
    }
};
}  // namespace

TEST(ext_input_plays_through_the_keys) {
    RigX g;
    auto quiet = g.run(4, true);                       // no key down: nothing sounds
    for (float v : quiet) CHECK(v < 1e-4f);
    g.s.noteOn(0, 69, 100);
    auto held = g.run(8, true);                        // key down: the sine comes through
    CHECK(held.back() > 0.02f);
    g.s.noteOff(0, 69);
    auto rel = g.run(8, true);                         // released: it fades out
    CHECK(rel.back() < held.back() * 0.2f);
}

TEST(ext_input_without_a_buffer_is_silent) {
    RigX g;
    g.s.noteOn(0, 69, 100);
    auto held = g.run(8, false);                       // EXT on but no audio arriving: OSC 2 gives nothing
    CHECK(held.back() < 1e-3f);
    // and the pointer from one block is never reused for the next: feed one block, then stop feeding
    auto one = g.run(1, true);
    auto after = g.run(4, false);
    CHECK(after.back() < 1e-3f);
    (void) one;
}

TEST(ext_input_follows_the_filter) {
    RigX open12; open12.set(P_fc, 127); open12.s.noteOn(0, 69, 100);
    auto a = open12.run(8, true);
    RigX closed; closed.set(P_fc, 10); closed.s.noteOn(0, 69, 100);    // cutoff far below 220 Hz
    auto b = closed.run(8, true);
    CHECK(b.back() < a.back() * 0.25f);
}

TEST(ext_input_every_engine_option_is_finite) {
    for (int os : { 0, 1, 2, 4 }) {
        RigX g; EngineOptions o; o.oversampling = os; g.s.setEngineOptions(o);
        g.s.noteOn(0, 60, 100); g.s.noteOn(0, 64, 100);
        auto v = g.run(6, true, 1.0f);
        for (float x : v) CHECK(std::isfinite(x) && x < 2.0f);
        CHECK(v.back() > 0.01f);
    }
}
