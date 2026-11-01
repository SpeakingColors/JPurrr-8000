// OSC 2 SUPER SAW (the EXTRA switch): engine behavior and SysEx export.
#include "test_util.h"
#include "jpurrr/patch.h"
#include "jpurrr/synth.h"
#include "jpurrr/preset_io.h"
#include "../core/src/dsp/synth_voice.h"
#include <cmath>
#include <cstring>
#include <vector>
using namespace jpurrr;

namespace {
struct Rig2 {
    Synth s; double sr; std::vector<float> l, r; int p[kNumParams];
    explicit Rig2(double sampleRate = 48000.0) : sr(sampleRate), l(4096), r(4096) {
        s.prepare(sr, 4096);
        std::memcpy(p, factoryPatch(0).v, sizeof p);
        // OSC 2 only, open filter, no envelopes or LFO in the way
        p[P_ob] = 63; p[P_fc] = 127; p[P_fr] = 0; p[P_fe] = 0; p[P_ol] = 0; p[P_fl] = 0; p[P_xt] = 0; p[P_dl] = 0;
        s.setParams(p);
    }
    void set(int idx, int v) { p[idx] = v; s.setParams(p); }
    std::vector<float> play(int note, int n) {
        std::vector<float> o; o.reserve((size_t) n);
        s.noteOn(0, note, 100);
        int done = 0;
        while (done < n) { const int m = std::min(n - done, 512); s.process(l.data(), r.data(), m); for (int i = 0; i < m; ++i) o.push_back(l[(size_t) i]); done += m; }
        s.noteOff(0, note);
        return o;
    }
};
float rms(const std::vector<float>& v, size_t from) { double a = 0; for (size_t i = from; i < v.size(); ++i) a += (double) v[i] * v[i]; return (float) std::sqrt(a / (double) (v.size() - from)); }
bool allFinite(const std::vector<float>& v) { for (float x : v) if (!std::isfinite(x) || std::fabs(x) > 4.f) return false; return true; }
}  // namespace

TEST(osc2ss_needs_the_extra_switch) {
    // with EXTRA off, o2w = 4 falls back to SAW: exactly the same samples as o2w = 1
    Rig2 a; a.set(P_o2ss, 0); a.set(P_o2w, 4);
    Rig2 b; b.set(P_o2ss, 0); b.set(P_o2w, 1);
    const auto va = a.play(57, 9600), vb = b.play(57, 9600);
    CHECK(va == vb);
    CHECK(rms(va, 2000) > 0.01f);
}

TEST(osc2ss_is_a_seven_saw_stack) {
    // with EXTRA on, DETUNE spreads the stack: a detuned stack beats against itself, a plain saw does not
    auto wobble = [](int detune) {
        Rig2 g; g.set(P_o2ss, 1); g.set(P_o2w, 4); g.set(P_o2c1, detune); g.set(P_o2c2, 127);
        const auto v = g.play(57, 48000);
        // envelope of the signal over 100 windows: a detuned stack rises and falls, a single saw stays flat
        float mn = 1e9f, mx = 0.f;
        for (int w = 20; w < 100; ++w) {
            float pk = 0.f;
            for (int i = w * 480; i < (w + 1) * 480; ++i) pk = std::max(pk, std::fabs(v[(size_t) i]));
            mn = std::min(mn, pk); mx = std::max(mx, pk);
        }
        return mx / std::max(1e-6f, mn);
    };
    CHECK(wobble(100) > 1.2f);           // clearly beating
    CHECK(wobble(0) < wobble(100));      // no detune moves less
}

TEST(osc2ss_works_with_every_engine_option) {
    for (int style = 0; style <= 1; ++style)
        for (int aa = 0; aa <= 1; ++aa)
            for (int os : { 0, 1, 2, 4 }) {
                Rig2 g; EngineOptions o; o.style = style; o.antiAlias = aa; o.oversampling = os; g.s.setEngineOptions(o);
                g.set(P_o2ss, 1); g.set(P_o2w, 4); g.set(P_sync, 1);     // sync may not break the stack (it is ignored)
                const auto v = g.play(81, 9600);
                CHECK(allFinite(v));
                CHECK(rms(v, 2000) > 0.005f);
            }
}

TEST(osc2ss_deterministic_and_both_oscillators) {
    Rig2 a; a.set(P_ob, 0); a.set(P_o1w, 0); a.set(P_o2ss, 1); a.set(P_o2w, 4); a.set(P_o2r, -12);
    Rig2 b; b.set(P_ob, 0); b.set(P_o1w, 0); b.set(P_o2ss, 1); b.set(P_o2w, 4); b.set(P_o2r, -12);
    const auto va = a.play(69, 9600), vb = b.play(69, 9600);
    CHECK(va == vb);
    CHECK(allFinite(va) && rms(va, 2000) > 0.01f);
}

TEST(osc2ss_sysex_export_writes_saw_and_warns) {
    Patch p; p.reset(); p.name = "SS test";
    p.v[paramIndexById("o2ss")] = 1; p.v[paramIndexById("o2w")] = 4;
    const auto r = savePatchAsSysexEditBuffer(p, 17, SysexModel::JP8000);
    CHECK(r.ok);
    bool warned = false;
    for (const auto& w : r.warnings) if (w.find("OSC 2 SUPER SAW") != std::string::npos) warned = true;
    CHECK(warned);
    // a plain patch does not warn about it
    Patch q; q.reset(); q.name = "plain";
    const auto r2 = savePatchAsSysexEditBuffer(q, 17, SysexModel::JP8000);
    bool warned2 = false;
    for (const auto& w : r2.warnings) if (w.find("OSC 2 SUPER SAW") != std::string::npos) warned2 = true;
    CHECK(r2.ok && !warned2);
}

TEST(osc2ss_struct_key_tells_the_waves_apart) {
    PatchParams a, b;
    int v[kNumParams]; for (int i = 0; i < kNumParams; ++i) v[i] = kParams[i].def;
    v[P_o2ss] = 1;
    v[P_o2w] = 4; a.derive(v, 0.f);
    v[P_o2w] = 3; b.derive(v, 0.f);
    CHECK(a.structKey() != b.structKey());
    v[P_o2w] = 4; v[P_o2ss] = 0; b.derive(v, 0.f);       // falls back to SAW
    CHECK(b.o2w == 1);
}
