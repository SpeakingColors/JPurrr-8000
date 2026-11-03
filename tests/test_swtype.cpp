// SUPER SAW TYPE of OSC 1 (SW 1 ours, SW 2 JE-8086 voicing, SW 3 thick Jup-8000 V voicing): engine behavior, defaults and SysEx export.
#include "test_util.h"
#include "jpurrr/patch.h"
#include "jpurrr/synth.h"
#include "jpurrr/preset_io.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace jpurrr;

namespace {
std::vector<float> render(int sw, int note, int o1w = 0) {
    Synth s; s.prepare(48000.0, 512);
    int p[kNumParams]; std::memcpy(p, factoryPatch(0).v, sizeof p);
    p[P_o1w] = o1w; p[P_o1sw] = sw; p[P_o1c1] = 99; p[P_o1c2] = 127; p[P_ob] = -64; p[P_fc] = 127; p[P_fr] = 0; p[P_fe] = 0; p[P_fk] = 0; p[P_fl] = 0; p[P_ol] = 0;
    p[P_qon] = 0; p[P_gon] = 0; p[P_xl] = 0; p[P_dl] = 0;
    s.setParams(p);
    std::vector<float> l(512), r(512), o;
    s.noteOn(0, note, 100);
    for (int b = 0; b < 100; ++b) { s.process(l.data(), r.data(), 512); if (b >= 20) o.insert(o.end(), l.begin(), l.end()); }
    return o;
}
double rmsOf(const std::vector<float>& v) { double a = 0; for (float x : v) a += (double) x * x; return std::sqrt(a / (double) v.size()); }
// energy above 4 kHz relative to the total, by a first order difference (rough but monotonic)
double brightness(const std::vector<float>& v) { double d = 0, t = 0; for (size_t i = 2; i < v.size(); ++i) { const double h = v[i] - 2 * v[i - 1] + v[i - 2]; d += h * h; t += (double) v[i] * v[i]; } return d / (t + 1e-30); }
}  // namespace

TEST(swtype_param_defaults_to_sw1_and_is_saved) {
    const int i = paramIndexById("o1sw");
    CHECK(i >= 0);
    CHECK(kParams[i].min == 0 && kParams[i].max == 2 && kParams[i].def == 0);
}

TEST(swtype_sw2_and_sw3_change_the_tone_not_the_loudness) {
    const auto a = render(0, 48), b = render(1, 48), c = render(2, 48);
    for (const auto* v : { &a, &b, &c }) { for (float x : *v) CHECK(std::isfinite(x) && std::fabs(x) < 2.f); }
    const double ra = rmsOf(a), rb = rmsOf(b), rc = rmsOf(c);
    CHECK(ra > 0.001);
    CHECK(std::fabs(20 * std::log10(rb / ra)) < 2.0);
    CHECK(std::fabs(20 * std::log10(rc / ra)) < 3.0);
    CHECK(brightness(b) > brightness(a));
    CHECK(brightness(c) > brightness(b));
}

TEST(swtype_is_ignored_when_osc1_is_not_super_saw) {
    const auto a = render(0, 48, 1), b = render(2, 48, 1);
    CHECK(a.size() == b.size());
    bool same = true; for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) same = false;
    CHECK(same);
}

TEST(swtype_sysex_export_writes_normal_super_saw_and_warns) {
    Patch p; p.reset(); p.name = "SW test"; p.v[paramIndexById("o1w")] = 0; p.v[paramIndexById("o1sw")] = 2;
    const auto r = savePatchAsSysexEditBuffer(p, 17, SysexModel::JP8000);
    CHECK(r.ok);
    bool warned = false; for (const auto& w : r.warnings) if (w.find("SUPER SAW type") != std::string::npos) warned = true;
    CHECK(warned);
    Patch q; q.reset(); q.name = "plain";
    const auto r2 = savePatchAsSysexEditBuffer(q, 17, SysexModel::JP8000);
    bool warned2 = false; for (const auto& w : r2.warnings) if (w.find("SUPER SAW type") != std::string::npos) warned2 = true;
    CHECK(r2.ok && !warned2);
}
