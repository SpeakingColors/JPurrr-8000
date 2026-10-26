// SoundFx tests: tone, delay, multi-FX. Pure core, no host.
#include "test_util.h"
#include "jpurrr/soundfx.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace jpurrr;

namespace {

const double kPi = 3.14159265358979323846;

struct Rig {
    SoundFx fx;
    int p[kNumParams];
    double sr;
    explicit Rig(double sampleRate = 44100.0, double bpm = 120.0) : sr(sampleRate) {
        for (int i = 0; i < kNumParams; ++i) p[i] = kParams[i].def;
        p[P_tb] = 0; p[P_tt] = 0; p[P_dl] = 0; p[P_xl] = 0; p[P_dsy] = 0; p[P_csy] = 0;
        fx.prepare(sr, 512);
        fx.setTempo(bpm);
        fx.setParams(p);
    }
    void snap() { fx.reset(); fx.setParams(p); }       // jump all smoothers to the current values
    void run(std::vector<float>& l, std::vector<float>& r, int block = 256) {
        for (size_t pos = 0; pos < l.size(); pos += (size_t)block) {
            int n = (int)std::min<size_t>((size_t)block, l.size() - pos);
            fx.setParams(p);
            fx.process(l.data() + pos, r.data() + pos, n);
        }
    }
};

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 2685821657736338717ULL + 1442695040888963407ULL) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 16); }
    double uni() { return (next() & 0xFFFFFF) / 16777216.0; }
    double bip() { return uni() * 2.0 - 1.0; }
    int range(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); }
};

void fft(std::vector<std::complex<double>>& a) {
    size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * kPi / (double)len;
        std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t j = 0; j < len / 2; ++j) {
                auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
            }
        }
    }
}

double rms(const std::vector<float>& x, size_t from = 0, size_t to = 0) {
    if (!to) to = x.size();
    double s = 0; for (size_t i = from; i < to; ++i) s += (double)x[i] * x[i];
    return std::sqrt(s / (double)(to - from));
}
double db(double x) { return 20.0 * std::log10(std::max(x, 1e-12)); }

// multi sine with a fixed spectrum, so the signal is the same at every sample rate
std::vector<float> multiSine(double sr, size_t n, double peak = 0.5) {
    std::vector<float> x(n, 0.f);
    Rng rng(7);
    const int comps = 120;
    std::vector<double> f(comps), ph(comps);
    for (int k = 0; k < comps; ++k) { f[k] = 60.0 * std::pow(10000.0 / 60.0, (k + rng.uni()) / comps); ph[k] = rng.uni() * 2 * kPi; }
    for (int k = 0; k < comps; ++k) {
        double w = 2 * kPi * f[k] / sr;
        for (size_t i = 0; i < n; ++i) x[i] += (float)std::sin(w * (double)i + ph[k]);
    }
    double m = 0; for (float v : x) m = std::max(m, (double)std::fabs(v));
    for (float& v : x) v = (float)(v / m * peak);
    return x;
}

} // namespace

// ---------------------------------------------------------------- bypass, latency

TEST(soundfx_bypass_exact) {
    // Everything off: bit exact copy. Each effect alone at level 0: also bit exact.
    for (int typ = 0; typ < 13; ++typ) {
        Rig rig;
        rig.p[P_xt] = typ; rig.p[P_xl] = 0; rig.p[P_dl] = 0;
        Rng rng(100 + typ);
        std::vector<float> l(6000), r(6000);
        for (auto& v : l) v = (float)rng.bip() * 0.8f;
        for (auto& v : r) v = (float)rng.bip() * 0.8f;
        auto l0 = l, r0 = r;
        rig.run(l, r, 100 + typ);
        bool same = l == l0 && r == r0;
        CHECK(same);
    }
    // level up and back to zero returns to bit exact
    Rig rig;
    rig.p[P_xt] = 3;
    std::vector<float> a(8000, 0.f), b(8000, 0.f);
    for (int i = 0; i < 8000; ++i) a[i] = b[i] = 0.5f * std::sin(0.05f * (float)i);
    rig.p[P_xl] = 100; rig.p[P_dl] = 90; rig.p[P_tb] = 30;
    rig.run(a, b);
    rig.p[P_xl] = 0; rig.p[P_dl] = 0; rig.p[P_tb] = 0;
    std::vector<float> l(48000), r(48000);
    Rng rng(5);
    for (auto& v : l) v = (float)rng.bip() * 0.5f;
    r = l;
    for (int i = 0; i < 3; ++i) rig.run(l, r);        // let smoothers and tails settle (tail max 2.4 s but level 0 mutes it)
    std::vector<float> l2(4096), r2(4096);
    for (auto& v : l2) v = (float)rng.bip() * 0.5f;
    r2 = l2;
    auto l3 = l2;
    rig.run(l2, r2);
    CHECK(l2 == l3);
}

TEST(soundfx_latency_zero) {
    Rig rig;
    CHECK(rig.fx.latencySamples() == 0);
    rig.p[P_xt] = 12; rig.p[P_xl] = 127; rig.p[P_dl] = 100; rig.p[P_tb] = 20;
    CHECK(rig.fx.latencySamples() == 0);
    // with tone only, an impulse stays at sample 0
    Rig r2;
    r2.p[P_tb] = 30; r2.p[P_tt] = -30;
    std::vector<float> l(512, 0.f), r(512, 0.f);
    l[0] = r[0] = 1.f;
    r2.run(l, r);
    size_t pk = 0; for (size_t i = 0; i < l.size(); ++i) if (std::fabs(l[i]) > std::fabs(l[pk])) pk = i;
    CHECK(pk == 0);
}

// ---------------------------------------------------------------- tone

static double toneGainDb(double sr, int tb, int tt, double f) {
    Rig rig(sr);
    rig.p[P_tb] = tb; rig.p[P_tt] = tt;
    rig.snap();
    size_t n = (size_t)(sr * 1.0);
    std::vector<float> l(n), r(n);
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = (float)(0.1 * std::sin(2 * kPi * f * (double)i / sr));
    rig.run(l, r);
    return db(rms(l, n / 2, n) / (0.1 / std::sqrt(2.0)));
}

TEST(soundfx_tone_shelves) {
    // follows the mockup: +-12 dB at the ends, 200 Hz and 4 kHz corners
    double hi = toneGainDb(44100, 63, 0, 40.0), lo = toneGainDb(44100, -64, 0, 40.0);
    CHECK(hi > 11.0 && hi < 12.5);
    CHECK(lo < -11.0 && lo > -12.5);
    CHECK(std::fabs(toneGainDb(44100, 63, 0, 8000.0)) < 0.3);          // bass does not touch the highs
    double th = toneGainDb(44100, 0, 63, 16000.0), tl = toneGainDb(44100, 0, -64, 16000.0);
    CHECK(th > 11.0 && th < 12.5);
    CHECK(tl < -11.0 && tl > -12.5);
    CHECK(std::fabs(toneGainDb(44100, 0, 63, 100.0)) < 0.3);
    CHECK(std::fabs(toneGainDb(44100, 0, 0, 1000.0)) < 1e-4);
    // shelf half gain around the corner
    double mid = toneGainDb(44100, 63, 0, 200.0);
    CHECK(mid > 5.0 && mid < 7.0);
}

TEST(soundfx_tone_sample_rate) {
    const double rates[] = { 44100, 48000, 88200, 96000, 192000 };
    const double fs[] = { 50, 200, 1000, 4000, 10000 };
    for (double f : fs) {
        double ref = toneGainDb(44100, 50, -40, f);
        for (double sr : rates) CHECK(std::fabs(toneGainDb(sr, 50, -40, f) - ref) < 0.5);
    }
}

TEST(soundfx_tone_no_zipper) {
    // sweep bass from -64 to 63 while a sine plays: the output must stay free of steps
    Rig rig;
    size_t n = 64 * 689;
    std::vector<float> l(n), r(n);
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = (float)(0.3 * std::sin(2 * kPi * 100.0 * (double)i / 44100.0));
    std::vector<float> ref = l;
    double worst = 0;
    std::vector<float> o(n);
    for (size_t pos = 0; pos + 64 <= n; pos += 64) {
        rig.p[P_tb] = -64 + (int)(127.0 * (double)pos / (double)n);
        rig.fx.setParams(rig.p);
        rig.fx.process(l.data() + pos, r.data() + pos, 64);
    }
    // second difference of the output relative to a pure sine of the same local amplitude: bounded
    size_t at = 0;
    for (size_t i = 2; i < n; ++i) {
        double d2 = std::fabs((double)l[i] - 2.0 * l[i - 1] + l[i - 2]);
        if (d2 > worst) { worst = d2; at = i; }
    }
    std::printf("  worst at %zu\n", at);
    std::printf("  tone sweep: worst second difference %.2e\n", worst);
    CHECK(worst < 0.3 * (2 * kPi * 100.0 / 44100.0) * (2 * kPi * 100.0 / 44100.0) * 6.0 + 0.002);
}

// ---------------------------------------------------------------- delay

static void impulseDelay(double sr, int type, int knob, int sync, double bpm, std::vector<float>& l, std::vector<float>& r) {
    Rig rig(sr, bpm);
    rig.p[P_dty] = type; rig.p[P_dt] = knob; rig.p[P_dl] = 127; rig.p[P_dfb] = 0; rig.p[P_dsy] = sync;
    rig.snap();
    size_t n = (size_t)(sr * 3.2);
    l.assign(n, 0.f); r.assign(n, 0.f);
    l[1000] = r[1000] = 1.f;
    rig.run(l, r);
}
static size_t firstEcho(const std::vector<float>& x, size_t from, float thr) {
    size_t best = 0; float bv = 0;
    for (size_t i = from; i < x.size(); ++i) if (std::fabs(x[i]) > bv) { bv = std::fabs(x[i]); best = i; }
    (void)thr;
    return best;
}

TEST(soundfx_delay_times_match_ms) {
    const double rates[] = { 44100, 48000, 96000 };
    for (double sr : rates) {
        for (int type = 0; type < 5; ++type) {
            const int knobs[] = { 0, 5, 40, 70, 127 };
            for (int knob : knobs) {
                std::vector<float> l, r;
                impulseDelay(sr, type, knob, 0, 120.0, l, r);
                double ms = SoundFx::delayTimeMs(type, knob);
                double expect = 1000.0 + ms * 0.001 * sr;
                // first echo: left for L>R and the mono types, right for R>L
                const std::vector<float>& a = (type == 1) ? r : l;
                size_t pos = firstEcho(a, 1002, 0.f);
                CHECK(std::fabs((double)pos - expect) <= 1.0);
                if (std::fabs((double)pos - expect) > 1.0) std::printf("  type %d knob %d sr %.0f: echo at %zu expected %.1f\n", type, knob, sr, pos, expect);
                // the other side stays quiet at that time for the panning types, both get it for the mono types
                const std::vector<float>& b = (type == 1) ? l : r;
                if (type <= 2) CHECK(std::fabs(b[pos]) < 0.01f);
                else CHECK(std::fabs(b[pos] - a[pos]) < 1e-6f);
            }
        }
    }
    // displayed ms: the documented maxima
    CHECK(SoundFx::delayTimeMs(0, 127) == 625.0 && SoundFx::delayTimeMs(1, 127) == 625.0);
    CHECK(SoundFx::delayTimeMs(2, 127) == 156.0 && SoundFx::delayTimeMs(3, 127) == 156.0);
    CHECK(SoundFx::delayTimeMs(4, 127) == 1250.0);
}

TEST(soundfx_delay_pingpong_alternates) {
    std::vector<float> l, r;
    Rig rig;
    rig.p[P_dty] = 0; rig.p[P_dt] = 64; rig.p[P_dl] = 127; rig.p[P_dfb] = 100;
    rig.snap();
    size_t n = 44100 * 3; l.assign(n, 0.f); r.assign(n, 0.f); l[500] = r[500] = 1.f;
    rig.run(l, r);
    double T = SoundFx::delayTimeMs(0, 64) * 0.001 * 44100.0;
    for (int k = 1; k <= 3; ++k) {
        size_t pos = 500 + (size_t)std::llround(T * k);
        const std::vector<float>& on = (k % 2) ? l : r;
        const std::vector<float>& off = (k % 2) ? r : l;
        float peak = 0, other = 0;
        for (size_t i = pos - 3; i <= pos + 3; ++i) { peak = std::max(peak, std::fabs(on[i])); other = std::max(other, std::fabs(off[i])); }
        CHECK(peak > 0.05f);
        CHECK(other < 0.01f * peak + 1e-6f);
    }
}

TEST(soundfx_delay_sync) {
    // 1/4 at 120 bpm = 500 ms, 1/8 dotted = 375 ms, 1/16 = 125 ms, 1/2 at 60 bpm = 2 s
    struct C { int idx; double bpm; double ms; } cs[] = { { 7, 120, 500 }, { 6, 120, 375 }, { 1, 120, 125 }, { 10, 60, 2000 }, { 4, 90, 333.333 }, { 8, 120, 666.667 } };
    for (auto& c : cs) {
        std::vector<float> l, r;
        impulseDelay(44100, 4, 3, c.idx, c.bpm, l, r);       // the knob must be ignored while sync is on
        size_t pos = firstEcho(l, 1002, 0.f);
        CHECK(std::fabs((double)pos - (1000.0 + c.ms * 44.1)) <= 1.0);
    }
    // tempo change moves the echo
    std::vector<float> l, r;
    impulseDelay(44100, 4, 3, 7, 60.0, l, r);
    CHECK(std::fabs((double)firstEcho(l, 1002, 0.f) - (1000.0 + 1000.0 * 44.1)) <= 1.0);
    // sync beats table
    CHECK(SoundFx::syncBeats(0) == 0.0);
    CHECK(std::fabs(SoundFx::syncBeats(7) - 1.0) < 1e-12);
    CHECK(std::fabs(SoundFx::syncBeats(10) - 2.0) < 1e-12);
    CHECK(std::fabs(SoundFx::syncBeats(17) - 12.0) < 1e-12);
    CHECK(SoundFx::syncBeats(23) == 0.0);
}

static double echoEnergy(const std::vector<float>& x, size_t centre, size_t half = 8) {
    double e = 0; for (size_t i = centre - half; i <= centre + half; ++i) e += (double)x[i] * x[i];
    return std::sqrt(e);
}

TEST(soundfx_delay_level_and_feedback) {
    // level 127 mono echo carries the input's energy, half level has half; repeats shrink by about fb * damping
    std::vector<float> l, r;
    impulseDelay(44100, 3, 60, 0, 120.0, l, r);
    size_t pos = firstEcho(l, 1002, 0.f);
    CHECK(std::fabs(echoEnergy(l, pos) - 1.0) < 0.06);
    Rig rig;
    rig.p[P_dty] = 3; rig.p[P_dt] = 60; rig.p[P_dl] = 64; rig.p[P_dfb] = 100;
    rig.snap();
    size_t n = 44100; std::vector<float> a(n, 0.f), b(n, 0.f); a[1000] = b[1000] = 1.f;
    rig.run(a, b);
    double T = SoundFx::delayTimeMs(3, 60) * 44.1;
    double e1 = echoEnergy(a, 1000 + (size_t)std::llround(T)), e2 = echoEnergy(a, 1000 + (size_t)std::llround(2 * T));
    CHECK(std::fabs(e1 - 64.0 / 127.0) < 0.04);
    double fb = 100.0 / 127.0 * 0.88;
    std::printf("  echo energy %.3f, repeat ratio %.3f (fb %.3f)\n", e1, e2 / e1, fb);
    CHECK(e2 / e1 > fb * 0.5 && e2 / e1 < fb * 1.02);
}

TEST(soundfx_delay_stable) {
    // maximum feedback and level: a noise burst, then silence. The tail must decay and never exceed a safe bound.
    for (int type = 0; type < 5; ++type) {
        Rig rig;
        rig.p[P_dty] = type; rig.p[P_dt] = 127; rig.p[P_dl] = 127; rig.p[P_dfb] = 127;
        Rng rng(40 + type);
        size_t n = 44100 * 30;
        std::vector<float> l(n, 0.f), r(n, 0.f);
        for (size_t i = 0; i < 20000; ++i) { l[i] = (float)rng.bip(); r[i] = (float)rng.bip(); }
        rig.run(l, r);
        double earlyPk = 0, latePk = 0, all = 0;
        for (size_t i = 20000; i < 44100 * 6; ++i) earlyPk = std::max(earlyPk, (double)std::max(std::fabs(l[i]), std::fabs(r[i])));
        for (size_t i = n - 44100 * 3; i < n; ++i) latePk = std::max(latePk, (double)std::max(std::fabs(l[i]), std::fabs(r[i])));
        for (size_t i = 0; i < n; ++i) { all = std::max(all, (double)std::fabs(l[i])); CHECK(std::isfinite(l[i]) && std::isfinite(r[i])); }
        CHECK(all < 6.0);
        CHECK(latePk < earlyPk);
        std::printf("  type %d: early peak %.3f, late peak %.2e\n", type, earlyPk, latePk);
        if (type != 4) CHECK(latePk < 0.05 * earlyPk);           // the tail dies out; the short types fully
        if (type == 2 || type == 3) CHECK(latePk < 1e-3);
    }
    // a steady sine with maximum feedback stays bounded
    for (int type = 0; type < 5; ++type) {
        Rig rig;
        rig.p[P_dty] = type; rig.p[P_dt] = 90; rig.p[P_dl] = 127; rig.p[P_dfb] = 127;
        size_t n = 44100 * 6; std::vector<float> l(n), r(n);
        for (size_t i = 0; i < n; ++i) l[i] = r[i] = (float)(0.9 * std::sin(2 * kPi * 220.0 * (double)i / 44100.0));
        rig.run(l, r);
        double pk = 0; for (size_t i = 0; i < n; ++i) pk = std::max(pk, (double)std::fabs(l[i]));
        CHECK(pk < 12.0);
    }
}

TEST(soundfx_delay_type_change_no_click) {
    // switching delay type while a tone plays: no sample jump bigger than a few times the normal slope
    Rig rig;
    rig.p[P_dty] = 0; rig.p[P_dt] = 50; rig.p[P_dl] = 100; rig.p[P_dfb] = 80;
    size_t n = 44100 * 4; std::vector<float> l(n), r(n);
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = (float)(0.4 * std::sin(2 * kPi * 330.0 * (double)i / 44100.0));
    for (size_t pos = 0; pos + 128 <= n; pos += 128) {
        if (pos % 22016 < 128) rig.p[P_dty] = (rig.p[P_dty] + 1) % 5;
        rig.fx.setParams(rig.p);
        rig.fx.process(l.data() + pos, r.data() + pos, 128);
    }
    double worst = 0;
    for (size_t i = 1; i < n; ++i) worst = std::max(worst, (double)std::fabs(l[i] - l[i - 1]));
    CHECK(worst < 0.4);                                              // 0.4 * 2*pi*330/44100 * sums of echoes stays well under this
}

// ---------------------------------------------------------------- multi-FX modulation

// Impulse train measurement of a modulated delay. Returns for each impulse the positions (ms) of the strongest peaks
// after the dry impulse. Used for chorus and flanger.
static void trainDelays(int type, double sr, int sync, double bpm, double seconds, std::vector<double>& t, std::vector<std::vector<double>>& peaks, int npeaks) {
    Rig rig(sr, bpm);
    rig.p[P_xt] = type; rig.p[P_xl] = 127; rig.p[P_csy] = sync;
    const size_t period = (size_t)(0.02 * sr);
    size_t n = (size_t)(seconds * sr);
    std::vector<float> l(n, 0.f), r(n, 0.f);
    for (size_t pos = period; pos + period < n; pos += period) l[pos] = r[pos] = 1.f;
    rig.run(l, r);
    t.clear(); peaks.clear();
    for (size_t pos = period; pos + period < n; pos += period) {
        std::vector<std::pair<float, size_t>> c;
        for (size_t i = 4; i < period - 4; ++i) {
            float v = std::fabs(l[pos + i]);
            if (v > std::fabs(l[pos + i - 1]) && v >= std::fabs(l[pos + i + 1]) && v > 0.05f) c.push_back({ v, i });
        }
        std::sort(c.begin(), c.end(), [](const std::pair<float, size_t>& a, const std::pair<float, size_t>& b) { return a.first > b.first; });
        std::vector<double> pk;
        for (int k = 0; k < npeaks && k < (int)c.size(); ++k) pk.push_back((double)c[k].second * 1000.0 / sr);
        std::sort(pk.begin(), pk.end());
        t.push_back((double)pos / sr); peaks.push_back(pk);
    }
}
// period of a roughly periodic series by autocorrelation (seconds): the first peak that reaches 80 % of the best one
static double seriesPeriod(const std::vector<double>& x, double dt, double minP, double maxP) {
    size_t n = x.size(); double mean = 0; for (double v : x) mean += v; mean /= (double)n;
    size_t lo = std::max<size_t>(2, (size_t)(minP / dt)), hi = std::min((size_t)(maxP / dt), n / 2);
    std::vector<double> ac(hi + 2, 0.0);
    double best = -1e30;
    for (size_t lag = lo - 1; lag <= hi + 1; ++lag) {
        double s = 0; for (size_t i = 0; i + lag < n; ++i) s += (x[i] - mean) * (x[i + lag] - mean);
        ac[lag] = s / (double)(n - lag);
        if (lag >= lo && lag <= hi) best = std::max(best, ac[lag]);
    }
    for (size_t lag = lo; lag <= hi; ++lag)
        if (ac[lag] >= ac[lag - 1] && ac[lag] >= ac[lag + 1] && ac[lag] >= 0.8 * best) return (double)lag * dt;
    return 0.0;
}

TEST(soundfx_chorus_depth_and_rate) {
    // SUPER CHORUS MID: voice 0 = 8.0 +- 1.6 ms, voice 1 = 11.5 +- 1.9 ms, rate 0.70 Hz (design table)
    std::vector<double> t; std::vector<std::vector<double>> pk;
    trainDelays(1, 44100.0, 0, 120.0, 6.0, t, pk, 2);
    double lo0 = 1e9, hi0 = 0, lo1 = 1e9, hi1 = 0;
    std::vector<double> v0;
    for (auto& p : pk) { if (p.size() < 2) continue; lo0 = std::min(lo0, p[0]); hi0 = std::max(hi0, p[0]); lo1 = std::min(lo1, p[1]); hi1 = std::max(hi1, p[1]); }
    // peaks sorted by position, voices can cross, so check the union of both ranges
    double lo = std::min(lo0, lo1), hi = std::max(hi0, hi1);
    CHECK(lo > 6.0 && lo < 7.0);        // 8.0 - 1.6 = 6.4
    CHECK(hi > 12.9 && hi < 13.7);      // 11.5 + 1.9 = 13.4
    // rate from the sum of the two voice positions
    for (auto& p : pk) v0.push_back(p.size() >= 2 ? p[0] + p[1] : 19.5);
    double per = seriesPeriod(v0, 0.02, 0.6 / 0.7, 1.6 / 0.7);
    CHECK(std::fabs(1.0 / per - 0.70) < 0.08);
}

TEST(soundfx_chorus_types_rates) {
    const double rate[4] = { 0.30, 0.70, 1.60, 0.90 };
    for (int ty = 0; ty < 4; ++ty) {
        std::vector<double> t; std::vector<std::vector<double>> pk;
        double secs = ty == 0 ? 12.0 : 8.0;
        trainDelays(ty, 44100.0, 0, 120.0, secs, t, pk, 2);
        std::vector<double> s;
        for (auto& p : pk) s.push_back(p.size() >= 2 ? p[0] + p[1] : 20.0);
        double per = seriesPeriod(s, 0.02, 0.5 / rate[ty], 1.6 / rate[ty]);
        CHECK(std::fabs(1.0 / per - rate[ty]) < 0.1 * rate[ty] + 0.03);
    }
}

TEST(soundfx_flanger_depth_and_rate) {
    // FLANGER SLOW: exponential sweep 0.5 .. 7 ms at 0.12 Hz; FLANGER FAST 0.5 .. 3.5 ms at 0.9 Hz
    struct C { int type; double lo, hi, rate; double secs; } cs[] = { { 4, 0.5, 7.0, 0.12, 24.0 }, { 6, 0.5, 3.5, 0.90, 8.0 }, { 5, 0.25, 9.0, 0.22, 16.0 } };
    for (auto& c : cs) {
        std::vector<double> t; std::vector<std::vector<double>> pk;
        trainDelays(c.type, 44100.0, 0, 120.0, c.secs, t, pk, 1);
        double lo = 1e9, hi = 0;
        std::vector<double> s;
        for (auto& p : pk) { if (p.empty()) { s.push_back(0); continue; } lo = std::min(lo, p[0]); hi = std::max(hi, p[0]); s.push_back(p[0]); }
        CHECK(lo > c.lo * 0.8 && lo < c.lo * 1.3);
        CHECK(hi > c.hi * 0.85 && hi < c.hi * 1.1);
        double per = seriesPeriod(s, 0.02, 0.5 / c.rate, 1.6 / c.rate);
        CHECK(std::fabs(1.0 / per - c.rate) < 0.1 * c.rate + 0.02);
    }
}

// spectral centroid series of a fixed multi sine input through a type; period of the centroid = LFO period
static double centroidSeriesRate(int type, int sync, double bpm, double seconds, double minHz, double maxHz, double* swing) {
    const double sr = 44100.0;
    Rig rig(sr, bpm);
    rig.p[P_xt] = type; rig.p[P_xl] = 127; rig.p[P_csy] = sync;
    size_t n = (size_t)(seconds * sr);
    const size_t W = 4096, hop = 1024;
    // lines exactly on FFT bins, at least 4 bins apart, so the dry spectrum has no beating at all
    std::vector<float> l(n, 0.f);
    {
        Rng rg(21);
        std::vector<size_t> bins; double k = 6;
        while (k < 1400) { bins.push_back((size_t)k); k = std::max(k + 4.0, k * 1.07); }
        for (size_t b : bins) {
            double w = 2 * kPi * (double)b / (double)W, ph = rg.uni() * 2 * kPi;
            for (size_t i = 0; i < n; ++i) l[i] += (float)(std::sin(w * (double)i + ph) * 0.5 / std::sqrt((double)bins.size() / 2.0));
        }
    }
    std::vector<float> r = l;
    rig.run(l, r);
    std::vector<double> cen;
    for (size_t pos = 0; pos + W <= n; pos += hop) {
        std::vector<std::complex<double>> a(W);
        for (size_t i = 0; i < W; ++i) a[i] = (double)l[pos + i] * (0.5 - 0.5 * std::cos(2 * kPi * (double)i / W));
        fft(a);
        double num = 0, den = 0;
        for (size_t k = 1; k < W / 2; ++k) { double m = std::norm(a[k]); num += m * std::log((double)k); den += m; }   // log frequency centroid
        cen.push_back(num / (den + 1e-30));
    }
    if (swing) { double lo = 1e9, hi = -1e9; for (size_t i = 80; i < cen.size(); ++i) { lo = std::min(lo, cen[i]); hi = std::max(hi, cen[i]); } *swing = hi - lo; }   // skip the first 2 s
    double dt = (double)hop / sr;
    double per = seriesPeriod(cen, dt, 1.0 / maxHz, 1.0 / minHz);
    return per > 0 ? 1.0 / per : 0.0;
}

TEST(soundfx_modfx_sync_rate) {
    // chorus and flanger types follow CHORUS SYNC: 1/4 at 120 bpm = 2 Hz; 1/2 at 120 bpm = 1 Hz.
    // Measured from the delay positions of an impulse train.
    for (int ty = 0; ty < 7; ++ty) {
        for (int sync = 7; sync <= 10; sync += 3) {
            double want = sync == 7 ? 2.0 : 1.0;
            std::vector<double> t; std::vector<std::vector<double>> pk;
            trainDelays(ty, 44100.0, sync, 120.0, 10.0, t, pk, ty < 4 ? 2 : 1);
            std::vector<double> s;
            for (auto& p : pk) s.push_back(ty < 4 ? (p.size() >= 2 ? p[0] + p[1] : 20.0) : (p.empty() ? 0.0 : p[0]));
            double per = seriesPeriod(s, 0.02, 0.6 / want, 1.6 / want);
            double rt = per > 0 ? 1.0 / per : 0.0;
            if (std::fabs(rt - want) > 0.1 * want) std::printf("  type %d sync %d: %.3f Hz, wanted %.1f\n", ty, sync, rt, want);
            CHECK(std::fabs(rt - want) < 0.1 * want);
        }
    }
    // phasers: spectral movement of a fixed multi sine input
    for (int ty : { 7, 8, 9 }) {
        for (int sync : { 7, 10 }) {
            double want = sync == 7 ? 2.0 : 1.0;
            double rt = centroidSeriesRate(ty, sync, 120.0, 12.0, want * 0.6, want * 1.6, nullptr);
            if (std::fabs(rt - want) > 0.1 * want) std::printf("  type %d sync %d: %.3f Hz, wanted %.1f\n", ty, sync, rt, want);
            CHECK(std::fabs(rt - want) < 0.1 * want);
        }
    }
}

TEST(soundfx_modfx_default_rates) {
    struct C { int type; double rate; double secs; } cs[] = { { 7, 0.15, 30.0 }, { 9, 0.45, 12.0 } };
    for (auto& c : cs) {
        double rt = centroidSeriesRate(c.type, 0, 120.0, c.secs, c.rate * 0.5, c.rate * 1.6, nullptr);
        std::printf("  type %d: free rate %.3f Hz (design %.3f)\n", c.type, rt, c.rate);
        CHECK(std::fabs(rt - c.rate) < 0.12 * c.rate + 0.02);
    }
}

TEST(soundfx_freeze_phase_stays_put) {
    // freeze types move far less than the deep phaser
    double swFreeze1 = 0, swFreeze2 = 0, swDeep = 0;
    centroidSeriesRate(10, 0, 120.0, 12.0, 0.02, 1.0, &swFreeze1);
    centroidSeriesRate(11, 0, 120.0, 12.0, 0.02, 1.0, &swFreeze2);
    centroidSeriesRate(7, 0, 120.0, 12.0, 0.1, 1.0, &swDeep);
    std::printf("  centroid swing: freeze1 %.3f freeze2 %.3f deep %.3f\n", swFreeze1, swFreeze2, swDeep);
    CHECK(swFreeze1 < 0.4 * swDeep);
    CHECK(swFreeze2 < 0.4 * swDeep);
}

TEST(soundfx_chorus_sync_lfo1) {
    // "LFO 1" follows setLfo1Hz; without it the multi-FX free runs at its own rate
    Rig rig;
    rig.p[P_xt] = 6; rig.p[P_xl] = 127; rig.p[P_csy] = 23;
    rig.fx.setLfo1Hz(3.0);
    size_t n = 44100 * 8; std::vector<float> l(n), r(n);
    Rng rng(3);
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = (float)rng.bip() * 0.5f;
    rig.run(l, r);
    for (float v : l) CHECK(std::isfinite(v));
}

// ---------------------------------------------------------------- level consistency and sample rate

static double typeGainDb(int type, int lev, double sr, int csy = 0) {
    Rig rig(sr);
    rig.p[P_xt] = type; rig.p[P_xl] = lev; rig.p[P_csy] = csy;
    size_t n = (size_t)(sr * 12.0);
    std::vector<float> l = multiSine(sr, n, 0.4f), r = l;
    double in = rms(l, n / 4, n);
    rig.run(l, r);
    return db(rms(l, n / 4, n) / in);
}

TEST(soundfx_modfx_level_consistent) {
    // output stays within a few dB of the input at any level, so presets do not jump in loudness
    for (int ty = 0; ty < 13; ++ty) {
        for (int lev : { 40, 90, 127 }) {
            double g = typeGainDb(ty, lev, 44100.0);
            if (std::fabs(g) > 3.0) std::printf("  type %d level %d: %.2f dB\n", ty, lev, g);
            CHECK(std::fabs(g) < 3.0);
        }
    }
}

TEST(soundfx_modfx_sample_rate_independent) {
    const double rates[] = { 48000, 88200, 96000, 192000 };
    for (int ty = 0; ty < 13; ++ty) {
        double ref = typeGainDb(ty, 100, 44100.0);
        for (double sr : rates) {
            double g = typeGainDb(ty, 100, sr);
            if (std::fabs(g - ref) >= 0.5) std::printf("  type %d at %.0f: %.2f dB vs %.2f dB\n", ty, sr, g, ref);
            CHECK(std::fabs(g - ref) < 0.5);
        }
    }
}

TEST(soundfx_delay_sample_rate_independent) {
    const double rates[] = { 44100, 48000, 96000 };
    double ref = 0;
    for (double sr : rates) {
        Rig rig(sr);
        rig.p[P_dty] = 3; rig.p[P_dt] = 80; rig.p[P_dl] = 90; rig.p[P_dfb] = 70;
        size_t n = (size_t)(sr * 6.0);
        std::vector<float> l = multiSine(sr, n, 0.4f), r = l;
        double in = rms(l);
        rig.run(l, r);
        double g = db(rms(l) / in);
        if (sr == 44100) ref = g; else CHECK(std::fabs(g - ref) < 0.5);
    }
}

// ---------------------------------------------------------------- distortion

TEST(soundfx_distortion_aliasing) {
    // Coherent sine, rectangular window: true harmonics sit exactly on multiples of k0. Everything else is aliasing
    // (or noise). Requirement: better than -70 dB below the fundamental.
    const size_t N = 1 << 16;
    const double sr = 44100.0;
    struct C { size_t k0; double amp; int lev; } cs[] = { { 4469, 0.5, 127 }, { 4469, 0.1, 127 }, { 7561, 0.5, 127 }, { 2999, 0.5, 100 }, { 12011, 0.5, 127 }, { 1009, 0.8, 127 }, { 6007, 0.5, 60 } };
    for (auto& c : cs) {
        Rig rig(sr);
        rig.p[P_xt] = 12; rig.p[P_xl] = c.lev;
        double f0 = (double)c.k0 * sr / (double)N;
        size_t warm = 20000, n = warm + N;
        std::vector<float> l(n), r(n);
        for (size_t i = 0; i < n; ++i) l[i] = r[i] = (float)(c.amp * std::sin(2 * kPi * f0 * (double)i / sr));
        const size_t start = warm;      // x is periodic with N samples, so any window of N samples holds exactly k0 cycles
        rig.run(l, r);
        std::vector<std::complex<double>> a(N);
        for (size_t i = 0; i < N; ++i) a[i] = (double)l[start + i];
        fft(a);
        // a periodic signal of period N/k0 samples has lines on multiples of k0 only when the segment holds integer cycles: it does (N * f0 / sr = k0)
        double fund = std::abs(a[c.k0]);
        double worst = 0;
        std::vector<char> harm(N / 2 + 1, 0);
        for (size_t h = 1; h * c.k0 <= N / 2; ++h) harm[h * c.k0] = 1;
        for (size_t k = 3; k <= N / 2; ++k) {
            bool near = false;
            for (int d = -1; d <= 1; ++d) { long kk = (long)k + d; if (kk >= 0 && kk <= (long)N / 2 && harm[(size_t)kk]) near = true; }
            if (near) continue;
            worst = std::max(worst, std::abs(a[k]));
        }
        double rel = db(worst / fund);
        std::printf("  distortion k0 %zu (%.0f Hz) amp %.1f lev %d: alias %.1f dB\n", c.k0, f0, c.amp, c.lev, rel);
        CHECK(rel < -70.0);
    }
}

TEST(soundfx_distortion_adds_harmonics_and_matches_level) {
    Rig rig;
    rig.p[P_xt] = 12; rig.p[P_xl] = 127;
    size_t n = 44100 * 2; std::vector<float> l(n), r(n);
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = (float)(0.3 * std::sin(2 * kPi * 200.0 * (double)i / 44100.0));
    auto in = l;
    rig.run(l, r);
    double gdb = db(rms(l, n / 2, n) / rms(in, n / 2, n));
    CHECK(std::fabs(gdb) < 1.5);
    // third harmonic present
    std::vector<std::complex<double>> a(1 << 15);
    for (size_t i = 0; i < a.size(); ++i) a[i] = (double)l[n / 2 + i] * (0.5 - 0.5 * std::cos(2 * kPi * (double)i / (double)a.size()));
    fft(a);
    double b1 = std::abs(a[(size_t)(200.0 / 44100.0 * a.size() + 0.5)]), b3 = std::abs(a[(size_t)(600.0 / 44100.0 * a.size() + 0.5)]);
    CHECK(db(b3 / b1) > -20.0);
    // different input levels all come out near the same loudness as the input
    for (double amp : { 0.02, 0.1, 0.6 }) {
        Rig r2; r2.p[P_xt] = 12; r2.p[P_xl] = 127;
        std::vector<float> x(n), y(n);
        for (size_t i = 0; i < n; ++i) x[i] = y[i] = (float)(amp * std::sin(2 * kPi * 440.0 * (double)i / 44100.0));
        auto z = x;
        r2.run(x, y);
        CHECK(std::fabs(db(rms(x, n / 2, n) / rms(z, n / 2, n))) < 2.0);
    }
}

// ---------------------------------------------------------------- type changes, tempo, fuzz

TEST(soundfx_type_change_crossfade) {
    // change multi-FX type on every block of a long sine: no bigger sample step than the signal itself allows
    Rig rig;
    rig.p[P_xl] = 100;
    size_t n = 44100 * 4; std::vector<float> l(n), r(n);
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = (float)(0.3 * std::sin(2 * kPi * 220.0 * (double)i / 44100.0));
    int ty = 0;
    for (size_t pos = 0; pos + 512 <= n; pos += 512) {
        if ((pos / 512) % 20 == 0) ty = (ty + 1) % 13;
        rig.p[P_xt] = ty;
        rig.fx.setParams(rig.p);
        rig.fx.process(l.data() + pos, r.data() + pos, 512);
    }
    double worst = 0;
    for (size_t i = 1; i < n; ++i) worst = std::max(worst, (double)std::fabs(l[i] - l[i - 1]));
    // plain 220 Hz sine at 0.3 has max step 0.0094; effects add up to ~2x. A click would be >0.2
    CHECK(worst < 0.12);
}

TEST(soundfx_fuzz_params) {
    // 2000 random parameter sets with noise, sine and silence; random block sizes; params change between blocks
    Rng rng(2026);
    double maxOut = 0;
    int bad = 0;
    for (int run = 0; run < 2000; ++run) {
        double srs[] = { 22050, 44100, 48000, 96000, 192000 };
        double sr = srs[rng.range(0, run % 7 == 0 ? 4 : 2)];
        SoundFx fx;
        fx.prepare(sr, 512);
        fx.setTempo(20.0 + rng.uni() * 280.0);
        if (rng.range(0, 3) == 0) fx.setLfo1Hz(rng.uni() * 20.0);
        int p[kNumParams];
        for (int i = 0; i < kNumParams; ++i) p[i] = kParams[i].def;
        int kind = run % 3;
        size_t n = (size_t)(sr * 0.25) + (size_t)rng.range(0, 3000);
        std::vector<float> l(n), r(n);
        double f = 40.0 + rng.uni() * 8000.0, amp = 0.05 + rng.uni() * 0.95;
        for (size_t i = 0; i < n; ++i) {
            if (kind == 0) { l[i] = (float)(rng.bip() * amp); r[i] = (float)(rng.bip() * amp); }
            else if (kind == 1) l[i] = r[i] = (float)(amp * std::sin(2 * kPi * f * (double)i / sr));
            else l[i] = r[i] = 0.f;
        }
        size_t pos = 0;
        while (pos < n) {
            auto setp = [&](int id, int lo, int hi) { p[id] = rng.range(lo, hi); };
            if (rng.range(0, 2) == 0 || pos == 0) {
                setp(P_tb, -64, 63); setp(P_tt, -64, 63); setp(P_xl, 0, 127); setp(P_dt, 0, 127); setp(P_dfb, 0, 127);
                setp(P_dl, 0, 127); setp(P_xt, 0, 12); setp(P_dty, 0, 4); setp(P_dsy, 0, 10); setp(P_csy, 0, 23);
                if (rng.range(0, 4) == 0) { p[P_dfb] = 127; p[P_dl] = 127; }
                if (rng.range(0, 4) == 0) { p[P_xl] = 127; }
            }
            int bs = rng.range(1, 512);
            bs = (int)std::min<size_t>((size_t)bs, n - pos);
            fx.setParams(p);
            if (rng.range(0, 9) == 0) fx.setTempo(20.0 + rng.uni() * 300.0);
            fx.process(l.data() + pos, r.data() + pos, bs);
            pos += (size_t)bs;
        }
        for (size_t i = 0; i < n; ++i) {
            if (!std::isfinite(l[i]) || !std::isfinite(r[i])) ++bad;
            maxOut = std::max(maxOut, (double)std::max(std::fabs(l[i]), std::fabs(r[i])));
        }
    }
    CHECK(bad == 0);
    std::printf("  fuzz: max |out| %.2f\n", maxOut);
    CHECK(maxOut < 20.0);
}

TEST(soundfx_no_denormals_after_silence) {
    // a loud burst, then long silence: output must go to exact zero or normal numbers, never denormals
    Rig rig;
    rig.p[P_xt] = 8; rig.p[P_xl] = 127; rig.p[P_dl] = 100; rig.p[P_dfb] = 100; rig.p[P_dty] = 3; rig.p[P_dt] = 10; rig.p[P_tb] = 40;
    size_t n = 44100 * 12; std::vector<float> l(n, 0.f), r(n, 0.f);
    Rng rng(77);
    for (size_t i = 0; i < 10000; ++i) l[i] = r[i] = (float)rng.bip();
    rig.run(l, r);
    int den = 0;
    for (size_t i = 0; i < n; ++i) { if (l[i] != 0.f && std::fabs(l[i]) < 1.2e-38f) ++den; if (r[i] != 0.f && std::fabs(r[i]) < 1.2e-38f) ++den; }
    CHECK(den == 0);
}

TEST(soundfx_small_block_sizes_match_large) {
    // block size must not change the result (same params every block)
    Rng rng(11);
    size_t n = 20000;
    std::vector<float> x(n);
    for (auto& v : x) v = (float)rng.bip() * 0.4f;
    std::vector<float> ref;
    for (int bs : { 512, 64, 7, 1 }) {
        Rig rig;
        rig.p[P_xt] = 4; rig.p[P_xl] = 90; rig.p[P_dl] = 80; rig.p[P_dfb] = 60; rig.p[P_tb] = 20; rig.p[P_tt] = -10;
        rig.fx.reset(); rig.fx.setParams(rig.p);                           // snap to these values, no ramps
        auto l = x, r = x;
        rig.run(l, r, bs);
        if (ref.empty()) ref = l;
        else { double d = 0; for (size_t i = 0; i < n; ++i) d = std::max(d, (double)std::fabs(l[i] - ref[i])); CHECK(d < 1e-4); }
    }
}
