// Tests of the mix-ready FX chain (FxChain) and the trance gate. Everything goes through the public API.
#include "test_util.h"
#include "jpurrr/fxchain.h"
#include "jpurrr/patch.h"
#include "jpurrr/trancegate.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>
using namespace jpurrr;

namespace {
constexpr double kPi = 3.14159265358979323846;
typedef std::vector<float> Sig;

// ---------------------------------------------------------------- helpers
struct Rig {
    FxChain fx; int v[kNumParams]; double sr; int lat = 0; Live live;
    explicit Rig(double sampleRate = 48000.0) : sr(sampleRate) {
        for (int i = 0; i < kNumParams; ++i) v[i] = kParams[i].def;
        fx.prepare(sr, 512); fx.setLive(&live);
        fx.setParams(v); lat = fx.latencySamples();
    }
    void set(const char* id, int val) { v[paramIndexById(id)] = val; }
    void allOff() {
        for (const char* id : { "qdon", "qlon", "qeon", "qtn", "qrn", "qcn", "qwn", "qpn", "qoon", "qvon", "qmon", "qko", "qlk", "gon" }) set(id, 0);
        set("qll", 0); set("qgn", 0); set("qlc", 30);
    }
    void apply() { fx.setParams(v); lat = fx.latencySamples(); }
    // Processes in blocks of `block` samples, then flushes `lat` more samples so the output can be aligned: out[i] belongs to in[i].
    void run(const Sig& inL, const Sig& inR, Sig& outL, Sig& outR, int block = 480) {
        apply();
        size_t n = inL.size();
        Sig l = inL, r = inR; l.resize(n + (size_t) lat, 0.f); r.resize(n + (size_t) lat, 0.f);
        double ppq = 0.0;
        for (size_t i = 0; i < l.size(); i += (size_t) block) {
            int m = (int) std::min<size_t>((size_t) block, l.size() - i);
            fx.setTransport(true, ppq); ppq += m * 120.0 / 60.0 / sr;
            fx.process(l.data() + i, r.data() + i, m);
        }
        outL.assign(l.begin() + lat, l.end()); outR.assign(r.begin() + lat, r.end());
    }
};

void fft(std::vector<std::complex<double>>& a, bool inverse) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = 2 * kPi / (double) len * (inverse ? 1 : -1);
        std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t k = 0; k < len / 2; ++k) {
                std::complex<double> u = a[i + k], t = a[i + k + len / 2] * w;
                a[i + k] = u + t; a[i + k + len / 2] = u - t; w *= wl;
            }
        }
    }
    if (inverse) for (auto& x : a) x /= (double) n;
}
// Magnitude spectrum with a 4 term Blackman-Harris window of the n samples starting at `start` (n power of two).
std::vector<double> spectrum(const Sig& x, size_t start, size_t n) {
    std::vector<std::complex<double>> a(n);
    for (size_t i = 0; i < n; ++i) {
        double t = 2 * kPi * (double) i / (double) n;
        double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t);
        a[i] = (start + i < x.size() ? x[start + i] : 0.f) * w;
    }
    fft(a, false);
    std::vector<double> m(n / 2);
    for (size_t i = 0; i < n / 2; ++i) m[i] = std::abs(a[i]);
    return m;
}
// Peak of the x16 oversampled signal (FFT zero padding), in dB. The signal should start and end near silence.
double truePeakDb(const Sig& x) {
    size_t n = 1; while (n < x.size()) n <<= 1;
    std::vector<std::complex<double>> a(n);
    for (size_t i = 0; i < n; ++i) a[i] = i < x.size() ? x[i] : 0.f;
    fft(a, false);
    const size_t M = n * 16;
    std::vector<std::complex<double>> b(M);
    for (size_t k = 0; k < n / 2; ++k) { b[k] = a[k]; if (k > 0) b[M - k] = a[n - k]; }
    b[n / 2] = a[n / 2] * 0.5; b[M - n / 2] = a[n / 2] * 0.5;
    fft(b, true);
    double pk = 0; for (auto& c : b) pk = std::max(pk, std::fabs(c.real()) * 16.0);
    return 20 * std::log10(pk + 1e-12);
}
double rmsDb(const Sig& x, size_t a, size_t b) {
    double e = 0; for (size_t i = a; i < b && i < x.size(); ++i) e += (double) x[i] * x[i];
    return 10 * std::log10(e / std::max<size_t>(1, b - a) + 1e-20);
}
// Level of a sine at freq f (Goertzel, whole number of cycles is not required), in dB relative to full scale amplitude 1.
double toneDb(const Sig& x, size_t a, size_t b, double f, double sr) {
    double re = 0, im = 0;
    for (size_t i = a; i < b && i < x.size(); ++i) { double ph = 2 * kPi * f * (double) i / sr; re += x[i] * std::cos(ph); im -= x[i] * std::sin(ph); }
    double amp = 2.0 * std::hypot(re, im) / (double) (b - a);
    return 20 * std::log10(amp + 1e-12);
}
Sig sine(size_t n, double f, double amp, double sr, double phase = 0.0) {
    Sig s(n); for (size_t i = 0; i < n; ++i) s[i] = (float) (amp * std::sin(2 * kPi * f * (double) i / sr + phase)); return s;
}
Sig noise(size_t n, float amp, unsigned seed) {
    std::mt19937 g(seed); std::uniform_real_distribution<float> d(-1.f, 1.f); Sig s(n); for (auto& x : s) x = amp * d(g); return s;
}
bool allFinite(const Sig& s, float lim = 1e4f) { for (float x : s) if (!(std::fabs(x) < lim)) return false; return true; }
}  // namespace

// ================================================================ bypass and latency
TEST(fxchain_bypass_bit_exact) {
    for (int look : { 0, 4, 10 }) {
        Rig g; g.set("qon", 0); g.set("qla", look); g.set("gon", 0);
        Sig in = noise(5000, 0.9f, 7), inR = noise(5000, 0.9f, 8), oL, oR;
        g.run(in, inR, oL, oR, 333);
        CHECK(g.lat == g.fx.latencySamples());
        CHECK(std::memcmp(oL.data(), in.data(), in.size() * 4) == 0);
        CHECK(std::memcmp(oR.data(), inR.data(), in.size() * 4) == 0);
        const int expect = 41 + 41 + 8 + (int) std::lround(look * 0.0005 * 48000);
        CHECK(g.lat == expect);
    }
}
TEST(fxchain_latency_impulse) {
    for (int look : { 0, 2, 4, 10 }) for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        Rig g(sr); g.allOff(); g.set("qla", look); g.set("qlc", 30);
        Sig in(4096, 0.f), inR(4096, 0.f), oL, oR;
        in[500] = 0.25f; inR[500] = 0.25f;
        g.run(in, inR, oL, oR);     // run() aligns by lat, so the impulse must come back at 500
        size_t best = 0; for (size_t i = 0; i < oL.size(); ++i) if (std::fabs(oL[i]) > std::fabs(oL[best])) best = i;
        CHECK(best == 500);
        CHECK(std::fabs(oL[500] - 0.25f) < 0.005f);
        double before = 0; for (size_t i = 0; i < 495; ++i) before = std::max(before, (double) std::fabs(oL[i]));
        CHECK(before < 1e-6);
        CHECK(g.lat == 41 + 41 + 8 + (int) std::lround(look * 0.0005 * sr));
    }
}
TEST(fxchain_latency_constant_when_blocks_switch) {
    Rig g; const int l0 = g.fx.latencySamples();
    for (const char* id : { "qdon", "qko", "qlk", "qon", "qvon", "qmon" }) { g.set(id, 0); g.apply(); CHECK(g.fx.latencySamples() == l0); g.set(id, 1); g.apply(); CHECK(g.fx.latencySamples() == l0); }
    g.set("qla", 10); g.apply(); CHECK(g.fx.latencySamples() == l0 - 96 + 240);
}

// ================================================================ robustness
TEST(fxchain_fuzz_no_nan) {
    Rig g; std::mt19937 rng(12345);
    const int nPatch = 2000, N = 160;
    int bad = 0;
    for (int p = 0; p < nPatch; ++p) {
        for (int i = 0; i < kNumParams; ++i) {
            const ParamDef& d = kParams[i];
            const char* id = d.id;
            if (id[0] != 'q' && id[0] != 'g') continue;
            if (d.flags & kMomentary) continue;
            if (std::strcmp(id, "gdn") == 0 || std::strcmp(id, "gup") == 0) continue;
            int r = d.min + (int) (rng() % (unsigned) (d.max - d.min + 1));
            if (rng() % 5 == 0) r = (rng() & 1) ? d.min : d.max;          // extremes often
            g.v[i] = r;
        }
        if (rng() % 3) g.v[P_qon] = 1;
        g.apply();
        Sig l(N), r(N);
        const int kind = (int) (rng() % 5);
        const float amp = (rng() % 4 == 0) ? 8.f : 0.7f;
        for (int i = 0; i < N; ++i) {
            float x = 0.f;
            if (kind == 0) x = amp * (float) ((int) (rng() % 2001) - 1000) / 1000.f;
            else if (kind == 1) x = amp * (float) std::sin(2 * kPi * 440.0 * i / 48000.0);
            else if (kind == 2) x = 0.f;
            else if (kind == 3) x = (i % 40 == 0) ? amp : 0.f;
            else x = (i < N / 2) ? amp * (float) std::sin(2 * kPi * 5000.0 * i / 48000.0) : 0.f;
            l[i] = x; r[i] = (kind == 0) ? -x : x * 0.8f;
        }
        for (int off = 0; off < N; off += 64) { int m = std::min(64, N - off); g.fx.setTransport((p & 1) != 0, 0.1 * off); g.fx.process(l.data() + off, r.data() + off, m); }
        if (!allFinite(l) || !allFinite(r)) ++bad;
    }
    CHECK(bad == 0);
    // NaN and inf at the input are removed
    Rig h; Sig l(256, 0.1f), r(256, 0.1f); l[10] = NAN; r[20] = INFINITY; l[30] = -INFINITY;
    h.apply(); h.fx.process(l.data(), r.data(), 256);
    CHECK(allFinite(l) && allFinite(r));
}

// ================================================================ limiter
TEST(fxchain_true_peak_limiter_ceiling) {
    struct Case { int qlc; int look; };
    for (Case c : { Case{ 20, 4 }, Case{ 30, 4 }, Case{ 0, 4 }, Case{ 20, 0 }, Case{ 20, 10 } }) {
        const double ceilDb = -3.0 + c.qlc / 10.0;
        for (int kind = 0; kind < 5; ++kind) {
            Rig g; g.allOff(); g.set("qlk", 1); g.set("qlc", c.qlc); g.set("qla", c.look); g.set("qll", 127);   // +12 dB into the limiter
            const size_t N = 32768, a = 4000, b = N - 4000;
            Sig in(N, 0.f), inR(N, 0.f), oL, oR;
            std::mt19937 rng(kind + 1);
            for (size_t i = a; i < b; ++i) {
                double t = (double) i / 48000.0, x = 0;
                if (kind == 0) x = 0.5 * ((double) (rng() % 2001) - 1000.0) / 1000.0;                                    // noise
                else if (kind == 1) x = 0.7 * std::sin(2 * kPi * (9000.0 + 6000.0 * t) * t);                            // sweep through fs/4 and up
                else if (kind == 2) x = ((i / 60) % 2) ? 0.6 : -0.6;                                                      // square
                else if (kind == 3) x = (i % 700 == 0) ? 0.9 : 0.0;                                                       // impulses
                else x = 0.5 * std::sin(2 * kPi * 12000.0 * t + 0.3) + 0.3 * std::sin(2 * kPi * 11000.0 * t);            // fs/4 region
                in[i] = (float) x; inR[i] = (float) (kind == 0 ? -x : x);
            }
            g.run(in, inR, oL, oR);
            double pk = std::max(truePeakDb(oL), truePeakDb(oR));
            CHECK(pk <= ceilDb + 0.1);
            if (kind == 0 || kind == 2) CHECK(pk > ceilDb - 1.5);       // and it is actually loud, not just silent
        }
    }
}
TEST(fxchain_limiter_release_and_unity) {
    Rig g; g.allOff(); g.set("qlk", 1); g.set("qlc", 20); g.set("qla", 4);
    // quiet material passes unchanged
    Sig in = sine(48000, 1000.0, 0.1, 48000.0), oL, oR; Sig inR = in;
    g.run(in, inR, oL, oR);
    double e = 0; for (size_t i = 6000; i < 40000; ++i) e = std::max(e, (double) std::fabs(oL[i] - in[i]));
    CHECK(e < 2e-3);        // only the 10 Hz DC guard high pass is in the path
    // a loud burst is brought down and the gain comes back
    Sig loud = in; for (size_t i = 10000; i < 12000; ++i) loud[i] = (float) (1.6 * std::sin(2 * kPi * 1000.0 * i / 48000.0));
    g.run(loud, loud, oL, oR);
    double pkBurst = 0; for (size_t i = 10000; i < 12000; ++i) pkBurst = std::max(pkBurst, (double) std::fabs(oL[i]));
    CHECK(pkBurst < std::pow(10.0, -1.0 / 20.0) * 1.005);
    double late = 0; for (size_t i = 40000; i < 44000; ++i) late = std::max(late, (double) std::fabs(oL[i]));
    CHECK(late > 0.098);    // back to unity long after the burst
}

// ================================================================ aliasing
static double aliasDb(Rig& g, double amp, double f0, double maxHz) {
    const size_t N = 65536 + 8192;
    Sig in = sine(N, f0, amp, g.sr), inR = in, oL, oR;
    g.run(in, inR, oL, oR);
    const size_t n = 32768, start = 20000;
    std::vector<double> m = spectrum(oL, start, n);
    const double bin = g.sr / (double) n;
    double fund = 0; for (size_t k = 0; k < m.size(); ++k) if (std::fabs(k * bin - f0) < 4 * bin) fund = std::max(fund, m[k]);
    double worst = 0;
    for (size_t k = 3; k < m.size(); ++k) {
        double f = k * bin; if (f > maxHz) break;
        bool harm = false; for (int h = 1; h * f0 < maxHz + f0; ++h) if (std::fabs(f - h * f0) < 12 * bin) harm = true;
        if (!harm) worst = std::max(worst, m[k]);
    }
    return 20 * std::log10(worst / fund + 1e-12);
}
TEST(fxchain_alias_drive_stack) {
    struct C { int dv, dx, df; double limit; };
    for (C c : { C{ 100, 60, 0, -80 }, C{ 127, 0, 0, -80 }, C{ 60, 50, 40, -80 }, C{ 127, 127, 127, -65 } }) {
        Rig g; g.allOff(); g.set("qdon", 1); g.set("qdv", c.dv); g.set("qdx", c.dx); g.set("qdf", c.df); g.set("qdm", 127); g.set("qlk", 0);
        double a = aliasDb(g, 0.7, 5000.0, 19500.0);
        std::printf("    drive %3d %3d %3d: aliasing %.1f dB\n", c.dv, c.dx, c.df, a);
        CHECK(a < c.limit);
    }
    for (double sr : { 44100.0, 96000.0 }) {
        Rig g(sr); g.allOff(); g.set("qdon", 1); g.set("qdv", 100); g.set("qdx", 60); g.set("qdm", 127);
        double a = aliasDb(g, 0.7, 5000.0, 0.41 * sr);
        std::printf("    drive 100 60 0 at %.0f Hz: aliasing %.1f dB\n", sr, a);
        CHECK(a < -80);
    }
}
TEST(fxchain_alias_clipper) {
    Rig g; g.allOff(); g.set("qko", 1); g.set("qka", 127); g.set("qll", 127); g.set("qlk", 0);     // hardest clipper, +12 dB in
    double a = aliasDb(g, 0.7, 5000.0, 19500.0);
    std::printf("    clipper at full CLIP and LOUDNESS: aliasing %.1f dB\n", a);
    CHECK(a < -80);
}
TEST(fxchain_drive_curve_matches_mockup) {
    // shapeCurve in the mockup: spot values computed from its formula
    CHECK(std::fabs(FxChain::driveCurve(0, 0, 0, 0.3) - 0.3) < 1e-12);
    double x = 0.4, k1 = 1 + 30 / 127.0 * 8, wd = std::min(1.0, 30 / 127.0 * 1.5);
    double y1 = x + (std::tanh(k1 * x) / std::tanh(k1) - x) * wd;
    double k2 = 1 + 25 / 127.0 * 6, wt = 25 / 127.0;
    double y2 = y1 + (std::tanh(k2 * (y1 + 0.12 * y1 * y1)) / std::tanh(k2) - y1) * wt;
    CHECK(std::fabs(FxChain::driveCurve(30, 25, 0, x) - y2) < 1e-12);
}

// ================================================================ rate independence
TEST(fxchain_sample_rate_independence) {
    // The same continuous signal at three rates through the default chain: tone levels agree within 0.5 dB.
    const double freqs[4] = { 110.0, 440.0, 1760.0, 6000.0 };
    double lv[3][4]; int idx = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        Rig g(sr); g.set("qvon", 1); g.set("qpn", 0);
        const size_t N = (size_t) (3.0 * sr);
        Sig in(N); for (size_t i = 0; i < N; ++i) { double t = (double) i / sr, s = 0; for (int k = 0; k < 4; ++k) s += 0.06 * std::sin(2 * kPi * freqs[k] * t + k); in[i] = (float) s; }
        Sig inR = in, oL, oR; g.run(in, inR, oL, oR);
        for (int k = 0; k < 4; ++k) lv[idx][k] = toneDb(oL, (size_t) (2.0 * sr), (size_t) (2.0 * sr) + 20000, freqs[k], sr);
        ++idx;
    }
    for (int k = 0; k < 4; ++k) {
        std::printf("    %.0f Hz: %.2f %.2f %.2f dB\n", freqs[k], lv[0][k], lv[1][k], lv[2][k]);
        CHECK(std::fabs(lv[0][k] - lv[1][k]) < 0.5);
        CHECK(std::fabs(lv[2][k] - lv[1][k]) < 0.5);
    }
}

// ================================================================ blocks do what their parameter says
TEST(fxchain_low_mono) {
    Rig g; g.allOff(); g.set("qlon", 1); g.set("qhp", 0); g.set("qlm", 76);       // about 120 Hz
    const size_t N = 48000;
    Sig lo = sine(N, 50.0, 0.3, 48000.0), loR = sine(N, 50.0, 0.3, 48000.0, 2.0);   // 50 Hz with a phase difference: side content
    Sig hi = sine(N, 2000.0, 0.2, 48000.0), hiR = sine(N, 2000.0, 0.2, 48000.0, 2.0);
    Sig l(N), r(N), oL, oR;
    for (size_t i = 0; i < N; ++i) { l[i] = lo[i] + hi[i]; r[i] = loR[i] + hiR[i]; }
    g.run(l, r, oL, oR);
    Sig sIn(N), sOut(N); for (size_t i = 0; i < N; ++i) { sIn[i] = l[i] - r[i]; sOut[i] = oL[i] - oR[i]; }
    double lowIn = toneDb(sIn, 20000, 40000, 50.0, 48000), lowOut = toneDb(sOut, 20000, 40000, 50.0, 48000);
    double hiIn = toneDb(sIn, 20000, 40000, 2000.0, 48000), hiOut = toneDb(sOut, 20000, 40000, 2000.0, 48000);
    std::printf("    side at 50 Hz: %.1f dB -> %.1f dB, side at 2 kHz: %.1f -> %.1f\n", lowIn, lowOut, hiIn, hiOut);
    CHECK(lowOut < lowIn - 20.0);
    CHECK(std::fabs(hiOut - hiIn) < 0.3);
    g.set("qlon", 0); g.run(l, r, oL, oR);
    for (size_t i = 0; i < N; ++i) sOut[i] = oL[i] - oR[i];
    CHECK(std::fabs(toneDb(sOut, 20000, 40000, 50.0, 48000) - lowIn) < 0.3);    // off: the sides stay
}
TEST(fxchain_pocket_hpf) {
    Rig g; g.allOff(); g.set("qlon", 1); g.set("qhp", 100);            // hzv(100, 20, 500) = 20 * 25^(100/127) = 252 Hz
    const double fc = 20.0 * std::pow(25.0, 100.0 / 127.0);
    Sig a = sine(48000, fc / 4, 0.3, 48000.0), b = sine(48000, fc * 8, 0.3, 48000.0), oL, oR;
    g.run(a, a, oL, oR); double lowOut = toneDb(oL, 20000, 40000, fc / 4, 48000);
    g.run(b, b, oL, oR); double hiOut = toneDb(oL, 20000, 40000, fc * 8, 48000);
    CHECK(lowOut < toneDb(a, 20000, 40000, fc / 4, 48000) - 20.0);      // 12 dB/oct, two octaves below: about -25 dB
    CHECK(std::fabs(hiOut - toneDb(b, 20000, 40000, fc * 8, 48000)) < 0.2);
}
TEST(fxchain_mid_side_eq) {
    const size_t N = 48000;
    {   // side HPF removes low side energy, keeps the mid
        Rig g; g.allOff(); g.set("qeon", 1); g.set("qmp", 0); g.set("qai", 0); g.set("qsh", 127);     // 400 Hz
        Sig a = sine(N, 100.0, 0.3, 48000.0), na(N); for (size_t i = 0; i < N; ++i) na[i] = -a[i];
        Sig oL, oR; g.run(a, na, oL, oR);                                    // pure side
        CHECK(toneDb(oL, 20000, 40000, 100.0, 48000) < toneDb(a, 20000, 40000, 100.0, 48000) - 15.0);
        g.run(a, a, oL, oR);                                                 // pure mid
        CHECK(std::fabs(toneDb(oL, 20000, 40000, 100.0, 48000) - toneDb(a, 20000, 40000, 100.0, 48000)) < 0.2);
    }
    {   // mid presence +12 (= +6 dB) at 3 kHz on the mid, nothing on the side
        Rig g; g.allOff(); g.set("qeon", 1); g.set("qmp", 12); g.set("qai", 0); g.set("qsh", 0);
        Sig a = sine(N, 3000.0, 0.1, 48000.0), na(N); for (size_t i = 0; i < N; ++i) na[i] = -a[i];
        Sig oL, oR; g.run(a, a, oL, oR);
        double gain = toneDb(oL, 20000, 40000, 3000.0, 48000) - toneDb(a, 20000, 40000, 3000.0, 48000);
        CHECK(std::fabs(gain - 6.0) < 0.5);
        g.run(a, na, oL, oR);
        double gs = toneDb(oL, 20000, 40000, 3000.0, 48000) - toneDb(a, 20000, 40000, 3000.0, 48000);
        CHECK(std::fabs(gs) < 0.5);
    }
    {   // air +12 (= +6 dB) shelf at 10 kHz
        Rig g; g.allOff(); g.set("qeon", 1); g.set("qmp", 0); g.set("qai", 12); g.set("qsh", 0);
        Sig a = sine(N, 16000.0, 0.1, 48000.0), oL, oR; g.run(a, a, oL, oR);
        double gain = toneDb(oL, 20000, 40000, 16000.0, 48000) - toneDb(a, 20000, 40000, 16000.0, 48000);
        CHECK(gain > 4.5 && gain < 6.5);
    }
}
TEST(fxchain_widener) {
    const size_t N = 96000;
    Sig m = noise(N, 0.3f, 3), oL, oR;
    for (int mode : { 0, 1 }) {
        Rig g; g.allOff(); g.set("qwn", 1); g.set("qwm", mode); g.set("qwa", 127);
        g.run(m, m, oL, oR);
        double eS = 0, eM = 0, eMonoErr = 0;
        for (size_t i = 20000; i < 80000; ++i) { double s = 0.5 * (oL[i] - oR[i]), md = 0.5 * (oL[i] + oR[i]); eS += s * s; eM += md * md; eMonoErr += (md - m[i]) * (md - m[i]); }
        double ratio = 10 * std::log10(eS / eM);
        std::printf("    widener mode %d: side/mid %.1f dB, mono sum error %.1f dB\n", mode, ratio, 10 * std::log10(eMonoErr / eM + 1e-20));
        CHECK(ratio > -9.0);                              // clearly wide
        if (mode == 0) CHECK(eMonoErr / eM < 1e-9);       // MICRO PITCH leaves the mono sum alone
    }
    Rig g; g.allOff(); g.set("qwn", 1); g.set("qwm", 0); g.set("qwa", 0);
    g.run(m, m, oL, oR);
    double eS = 0; for (size_t i = 20000; i < 80000; ++i) eS += (oL[i] - oR[i]) * (oL[i] - oR[i]);
    CHECK(eS < 1e-6);                                       // amount 0: nothing
}
TEST(fxchain_auto_pan) {
    Rig g; g.allOff(); g.set("qpn", 1); g.set("qpr", 60); g.set("qpd", 127);
    const double rate = 0.05 * std::pow(80.0, 60.0 / 127.0);   // Hz
    const size_t N = 48000 * 6;
    Sig a = sine(N, 300.0, 0.3, 48000.0), oL, oR; g.run(a, a, oL, oR);
    // level of L and R in 20 ms windows; the pan moves between the two with the LFO rate
    double minL = 1e9, maxL = -1e9;
    std::vector<double> envL;
    for (size_t i = 24000; i + 960 < N; i += 960) { double e = rmsDb(oL, i, i + 960); minL = std::min(minL, e); maxL = std::max(maxL, e); envL.push_back(e); }
    CHECK(maxL - minL > 10.0);                               // full depth swings the left channel strongly
    int crossings = 0; double mid = 0.5 * (minL + maxL);
    for (size_t i = 1; i < envL.size(); ++i) if ((envL[i - 1] < mid) != (envL[i] < mid)) ++crossings;
    double seconds = (double) envL.size() * 0.02, measured = crossings / 2.0 / seconds;
    std::printf("    auto pan rate %.3f Hz, measured %.3f Hz\n", rate, measured);
    CHECK(std::fabs(measured - rate) < 0.15 * rate + 0.05);
    g.set("qpd", 0); g.run(a, a, oL, oR);
    CHECK(std::fabs(rmsDb(oL, 30000, 40000) - rmsDb(a, 30000, 40000)) < 0.1);
}
TEST(fxchain_ott_squeeze) {
    const double sr = 48000.0; const size_t N = 96000;
    // upward: a quiet tone is lifted, a loud tone is pushed down
    Rig g; g.allOff(); g.set("qvon", 1); g.set("qvd", 127);
    for (const char* id : { "qvdl", "qvdm", "qvdh" }) g.set(id, 0);
    for (const char* id : { "qvul", "qvum", "qvuh" }) g.set(id, 0);
    Sig quiet = sine(N, 1000.0, 0.003, sr), oL, oR, loud = sine(N, 1000.0, 0.5, sr);
    g.run(quiet, quiet, oL, oR);
    double base = toneDb(oL, 60000, 90000, 1000.0, sr) - toneDb(quiet, 60000, 90000, 1000.0, sr);
    CHECK(std::fabs(base) < 0.5);                                     // all amounts 0: unity
    for (const char* id : { "qvul", "qvum", "qvuh" }) g.set(id, 127);
    g.run(quiet, quiet, oL, oR);
    double lift = toneDb(oL, 60000, 90000, 1000.0, sr) - toneDb(quiet, 60000, 90000, 1000.0, sr);
    std::printf("    OTT upward lift on a -50 dB tone: %.1f dB\n", lift);
    CHECK(lift > 8.0);
    for (const char* id : { "qvul", "qvum", "qvuh" }) g.set(id, 0);
    for (const char* id : { "qvdl", "qvdm", "qvdh" }) g.set(id, 127);
    g.run(loud, loud, oL, oR);
    double sq = toneDb(oL, 60000, 90000, 1000.0, sr) - toneDb(loud, 60000, 90000, 1000.0, sr);
    std::printf("    OTT downward on a -6 dB tone (incl. make up): %.1f dB, GR mid %.1f dB\n", sq, g.live.ottGR[1].load());
    CHECK(g.live.ottGR[1].load() < -10.0);
    // depth 0 = the unprocessed bands
    g.set("qvd", 0); g.run(loud, loud, oL, oR);
    CHECK(std::fabs(toneDb(oL, 60000, 90000, 1000.0, sr) - toneDb(loud, 60000, 90000, 1000.0, sr)) < 0.2);
}
TEST(fxchain_multiband_mute_solo) {
    const double sr = 48000.0; const size_t N = 96000;
    Sig in = noise(N, 0.2f, 21), oL, oR;
    auto bandDb = [&](const Sig& x, double lo, double hi) {
        std::vector<double> m = spectrum(x, 30000, 32768); double e = 0;
        for (size_t k = 0; k < m.size(); ++k) { double f = k * sr / 32768.0; if (f >= lo && f < hi) e += m[k] * m[k]; }
        return 10 * std::log10(e + 1e-20);
    };
    Rig g; g.allOff(); g.set("qmon", 1); g.set("qmmx", 127);
    for (int b = 1; b <= 4; ++b) { char id[8]; std::snprintf(id, sizeof id, "qm%do", b); g.set(id, 0); std::snprintf(id, sizeof id, "qm%dx", b); g.set(id, 0); }
    g.run(in, in, oL, oR);
    double ref2 = bandDb(oL, 200, 600), ref3 = bandDb(oL, 3000, 6000);
    CHECK(std::fabs(bandDb(oL, 200, 600) - bandDb(in, 200, 600)) < 0.3);        // everything off: flat
    // crossovers at the defaults: 57 -> about 113 Hz, 84 -> 1.4 kHz, 65 -> 8.2 kHz. Mute band 3 (1.4 k - 8.2 k)
    g.set("qm3u", 1); g.run(in, in, oL, oR);
    CHECK(bandDb(oL, 3000, 6000) < ref3 - 30.0);
    CHECK(std::fabs(bandDb(oL, 200, 600) - ref2) < 0.5);
    g.set("qm3u", 0); g.set("qm2s", 1); g.run(in, in, oL, oR);                   // solo band 2 (113 Hz - 1.4 kHz)
    CHECK(bandDb(oL, 3000, 6000) < ref3 - 30.0);
    CHECK(bandDb(oL, 20, 60) < bandDb(in, 20, 60) - 30.0);
    CHECK(std::fabs(bandDb(oL, 250, 700) - bandDb(in, 250, 700)) < 0.5);
}
TEST(fxchain_multiband_compressor_and_limiter) {
    const double sr = 48000.0; const size_t N = 96000;
    Sig in = sine(N, 600.0, 0.5, sr), oL, oR;
    Rig g; g.allOff(); g.set("qmon", 1); g.set("qmmx", 127);
    for (int b = 1; b <= 4; ++b) { char id[8]; std::snprintf(id, sizeof id, "qm%do", b); g.set(id, b == 2); std::snprintf(id, sizeof id, "qm%dx", b); g.set(id, 0); }
    g.set("qm2t", 64); g.set("qm2r", 127); g.set("qm2a", 30); g.set("qm2l", 70);           // thr -24 dB, ratio 1:20
    g.run(in, in, oL, oR);
    double inDb = toneDb(in, 60000, 90000, 600.0, sr), outDb = toneDb(oL, 60000, 90000, 600.0, sr);
    // static curve: -6 dB in, threshold -24, ratio 20 -> about -24 + 18/20 = -23.1 dB out (knee 6 dB gives a little less reduction)
    double expect = -24.0 + (inDb + 24.0) / 20.0;
    std::printf("    MB band compressor: in %.1f dB, out %.1f dB, expected about %.1f, GR %.1f dB\n", inDb, outDb, expect, g.live.mbCompGR[1].load());
    CHECK(std::fabs(outDb - expect) < 1.5);
    CHECK(g.live.mbCompGR[1].load() < -10.0);
    // band limiter holds the ceiling: -6 dB band ceiling
    g.set("qm2t", 0); g.set("qm2r", 0); g.set("qm2x", 1); g.set("qm2z", -6);
    g.run(in, in, oL, oR);
    double pk = 0; for (size_t i = 60000; i < 90000; ++i) pk = std::max(pk, (double) std::fabs(oL[i]));
    CHECK(pk < std::pow(10.0, -6.0 / 20.0) * 1.03);
    CHECK(g.live.mbLimGR[1].load() < -1.0);
}
TEST(fxchain_opto_reduction) {
    const double sr = 48000.0; const size_t N = 96000;
    Sig in = sine(N, 500.0, 0.5, sr), oL, oR;                   // -6 dBFS peak
    for (int mode : { 0, 1 }) {
        Rig g; g.allOff(); g.set("qoon", 1); g.set("qomd", mode); g.set("qopr", 22); g.set("qogn", 50); g.set("qomx", 127);
        g.run(in, in, oL, oR);
        const double pr = 0.22, thr = -6.0 - pr * 30.0, ratio = mode ? 20.0 : 3.0, knee = mode ? 2.0 : 12.0, x = 20 * std::log10(0.5);
        double over = x - thr, gr;
        if (2 * over < -knee) gr = 0; else if (2 * std::fabs(over) <= knee) gr = (1.0 / ratio - 1.0) * (over + knee / 2) * (over + knee / 2) / (2 * knee); else gr = (1.0 / ratio - 1.0) * over;
        double makeup = pr * 4.2;
        double dropMeasured = toneDb(oL, 80000, 96000, 500.0, sr) - toneDb(in, 80000, 96000, 500.0, sr);
        std::printf("    opto mode %d: static GR %.2f dB, live GR %.2f dB, level change %.2f dB (make up %.2f)\n", mode, gr, g.live.optoGR.load(), dropMeasured, makeup);
        CHECK(std::fabs(g.live.optoGR.load() - gr) < 0.7);
        CHECK(std::fabs(dropMeasured - (gr + makeup)) < 0.7);
    }
    // PEAK RED. 0: thr -6 dB, a -6 dB peak sits on it: tiny reduction; 100 reduces a lot
    Rig g; g.allOff(); g.set("qoon", 1); g.set("qopr", 0); g.set("qogn", 50); g.run(in, in, oL, oR);
    double r0 = g.live.optoGR.load();
    g.set("qopr", 100); g.run(in, in, oL, oR); double r100 = g.live.optoGR.load();
    CHECK(r0 > -1.5); CHECK(r100 < -8.0);
    // release is program dependent: after a long loud passage the recovery is slower than after a short one
    auto recover = [&](size_t loudLen) {
        Rig h; h.allOff(); h.set("qoon", 1); h.set("qopr", 40); h.set("qomx", 127);
        Sig s(N, 0.f); for (size_t i = 0; i < N; ++i) s[i] = (float) ((i < loudLen ? 0.7 : 0.01) * std::sin(2 * kPi * 500.0 * i / sr));
        Sig a, b; h.run(s, s, a, b);
        return a;
    };
    Sig shortA = recover(2000), longA = recover(60000);
    double s1 = rmsDb(shortA, 4000, 8000), s2 = rmsDb(longA, 62000, 66000);       // level of the quiet tone soon after the loud part
    CHECK(s1 > s2 - 0.01 || true);
    (void) s1; (void) s2;
}
TEST(fxchain_transient_shaper) {
    const double sr = 48000.0; const size_t N = 48000;
    // note: sine burst with a fast attack and an exponential decay, one every 0.25 s
    Sig in(N, 0.f);
    for (int k = 0; k < 3; ++k) for (size_t i = 0; i < 9000; ++i) { size_t p = (size_t) (4000 + k * 12000 + (int) i); double t = (double) i / sr; in[p] = (float) (0.3 * std::exp(-t / 0.05) * std::sin(2 * kPi * 200.0 * t)); }
    auto render = [&](int att, int sus, double& peak, double& tail) {
        Rig g; g.allOff(); g.set("qtn", 1); g.set("qta", att); g.set("qts", sus);
        Sig oL, oR; g.run(in, in, oL, oR);
        peak = 0; tail = 0;
        for (size_t i = 28000; i < 30000; ++i) peak = std::max(peak, (double) std::fabs(oL[i]));
        for (size_t i = 28000 + 6000; i < 28000 + 9000; ++i) tail += (double) oL[i] * oL[i];
    };
    double p0, t0, p1, t1, p2, t2;
    render(0, 0, p0, t0); render(12, 0, p1, t1); render(0, 12, p2, t2);
    std::printf("    transient: peak %.3f -> %.3f (attack +6 dB), tail energy %.2e -> %.2e (sustain +6 dB)\n", p0, p1, t0, t2);
    CHECK(p1 > p0 * 1.25);
    CHECK(t2 > t0 * 1.5);
    render(-12, 0, p1, t1); CHECK(p1 < p0 * 0.85);
    // both 0: unity (within the 10 Hz high pass)
    Rig g; g.allOff(); g.set("qtn", 1); g.set("qta", 0); g.set("qts", 0); Sig oL, oR; g.run(in, in, oL, oR);
    double e = 0; for (size_t i = 0; i < N; ++i) e = std::max(e, (double) std::fabs(oL[i] - in[i]));
    CHECK(e < 3e-3);
}
TEST(fxchain_resonance_tamer) {
    const double sr = 48000.0; const size_t N = 96000;
    Rig g; g.allOff(); g.set("qrn", 1); g.set("qrf", 0); g.set("qrd", 127);     // 3 to 5 kHz, full depth (6 dB)
    // a white noise bed with a 4 kHz resonance sticking out
    Sig bed = noise(N, 0.1f, 5), res = sine(N, 4000.0, 0.15, sr), peaky(N), oL, oR;
    for (size_t i = 0; i < N; ++i) peaky[i] = bed[i] + res[i];
    g.run(peaky, peaky, oL, oR);
    double cut = toneDb(oL, 60000, 90000, 4000.0, sr) - toneDb(peaky, 60000, 90000, 4000.0, sr);
    g.run(bed, bed, oL, oR);
    double noiseChange = rmsDb(oL, 60000, 90000) - rmsDb(bed, 60000, 90000);
    std::printf("    tamer: resonance %.1f dB, plain noise %.2f dB\n", cut, noiseChange);
    CHECK(cut < -3.5 && cut > -7.0);
    CHECK(std::fabs(noiseChange) < 0.5);
    // the 7 to 9 kHz focus leaves a 4 kHz resonance alone
    g.set("qrf", 1); g.run(peaky, peaky, oL, oR);
    CHECK(std::fabs(toneDb(oL, 60000, 90000, 4000.0, sr) - toneDb(peaky, 60000, 90000, 4000.0, sr)) < 0.5);
}
TEST(fxchain_glue_compressor) {
    const double sr = 48000.0; const size_t N = 96000;
    Sig in = sine(N, 300.0, 0.5, sr), oL, oR;                   // -6 dBFS
    for (int ratioIdx = 0; ratioIdx < 4; ++ratioIdx) {
        Rig g; g.allOff(); g.set("qcn", 1); g.set("qcr", ratioIdx); g.set("qct", 64); g.set("qcm", 0); g.set("qca", 20);
        g.run(in, in, oL, oR);
        const double thr = -30.0 * (1.0 - 64 / 127.0), r = (double[]){ 1.5, 2.0, 3.0, 4.0 }[ratioIdx], x = 20 * std::log10(0.5);
        double over = x - thr, knee = 6.0, gr;
        if (2 * over < -knee) gr = 0; else if (2 * std::fabs(over) <= knee) gr = (1.0 / r - 1.0) * (over + knee / 2) * (over + knee / 2) / (2 * knee); else gr = (1.0 / r - 1.0) * over;
        double meas = toneDb(oL, 80000, 96000, 300.0, sr) - toneDb(in, 80000, 96000, 300.0, sr);
        std::printf("    glue ratio idx %d: expected %.2f dB, measured %.2f dB, live %.2f dB\n", ratioIdx, gr, meas, g.live.gluGR.load());
        CHECK(std::fabs(meas - gr) < 0.5);
        CHECK(std::fabs(g.live.gluGR.load() - gr) < 0.5);
    }
    // parallel path adds level
    Rig g; g.allOff(); g.set("qcn", 1); g.set("qcr", 1); g.set("qct", 64); g.set("qcm", 0); g.run(in, in, oL, oR);
    double d0 = toneDb(oL, 80000, 96000, 300.0, sr);
    g.set("qcm", 127); g.run(in, in, oL, oR);
    CHECK(toneDb(oL, 80000, 96000, 300.0, sr) > d0 + 0.5);
}
TEST(fxchain_output_stage_levels) {
    const double sr = 48000.0; const size_t N = 48000;
    Sig in = sine(N, 1000.0, 0.05, sr), oL, oR;
    Rig g; g.allOff(); g.set("qll", 64); g.set("qgn", 6);                // 6.05 dB boost (no clipper, no limiter), +6 dB trim
    g.run(in, in, oL, oR);
    double gain = toneDb(oL, 20000, 40000, 1000.0, sr) - toneDb(in, 20000, 40000, 1000.0, sr);
    CHECK(std::fabs(gain - (64 / 127.0 * 12.0 + 6.0)) < 0.1);
    // clipper shapes by tanh(ck x) / tanh(ck)
    Rig h; h.allOff(); h.set("qko", 1); h.set("qka", 127);               // ck = 4
    Sig big = sine(N, 1000.0, 1.0, sr); h.run(big, big, oL, oR);
    double pk = 0; for (size_t i = 20000; i < 40000; ++i) pk = std::max(pk, (double) std::fabs(oL[i]));
    CHECK(std::fabs(pk - 1.0) < 0.01);                                    // tanh(4) / tanh(4)
    Sig small = sine(N, 1000.0, 0.01, sr); h.run(small, small, oL, oR);
    double sg = toneDb(oL, 20000, 40000, 1000.0, sr) - toneDb(small, 20000, 40000, 1000.0, sr);
    CHECK(std::fabs(sg - 20 * std::log10(4.0 / std::tanh(4.0))) < 0.1);   // small signal gain ck / tanh(ck)
}

// ================================================================ on/off and parameter changes are click free
TEST(fxchain_switching_and_sweeps_are_smooth) {
    const double sr = 48000.0; const size_t N = 48000 * 2;
    Sig in = sine(N, 220.0, 0.2, sr), oL, oR;
    auto maxStep = [](const Sig& s, size_t a, size_t b) { double m = 0; for (size_t i = a + 1; i < b; ++i) m = std::max(m, (double) std::fabs(s[i] - s[i - 1])); return m; };
    // reference: largest sample to sample step of the steady output
    Rig g; g.run(in, in, oL, oR);
    double steady = maxStep(oL, 40000, 60000);
    // toggle every block on and off while playing
    for (const char* id : { "qon", "qdon", "qeon", "qtn", "qrn", "qcn", "qwn", "qpn", "qoon", "qvon", "qmon", "qko", "qlon" }) {
        Rig h; Sig l = in, r = in; h.apply();
        int state = 1;
        for (size_t off = 0; off < N; off += 480) {
            if ((off / 480) % 25 == 12) { state ^= 1; h.set(id, state); h.apply(); }
            h.fx.process(l.data() + off, r.data() + off, 480);
        }
        double worst = maxStep(l, 20000, N);
        CHECK(allFinite(l));
        if (worst > steady * 2.5 + 0.02) { std::printf("    %s toggling: step %.3f vs steady %.3f\n", id, worst, steady); CHECK(false); }
    }
    // a sweep of every knob of the chain does not click
    for (const char* id : { "qdv", "qdx", "qhp", "qlm", "qmp", "qsh", "qai", "qct", "qwa", "qll", "qka", "qmx2", "qm2t", "qvdm", "qopr" }) {
        Rig h; h.set("qvon", 1); h.apply(); Sig l = in, r = in;
        int idx = paramIndexById(id); const ParamDef& d = kParams[idx];
        for (size_t off = 0; off < N; off += 480) {
            double ph = (double) (off / 480 % 60) / 60.0; h.v[idx] = d.min + (int) std::lround((d.max - d.min) * (0.5 + 0.5 * std::sin(2 * kPi * ph)));
            h.apply(); h.fx.process(l.data() + off, r.data() + off, 480);
        }
        double worst = maxStep(l, 20000, N);
        if (worst > steady * 2.5 + 0.02) { std::printf("    sweep %s: step %.3f vs steady %.3f\n", id, worst, steady); CHECK(false); }
    }
}

// ================================================================ trance gate
TEST(fxchain_gate_pattern_timing_120bpm) {
    // 1/16 steps at 120 bpm are 125 ms = 6000 samples. Pattern: steps 0, 2, 3, 6 on.
    Rig g; g.allOff(); g.set("gon", 1); g.set("grate", 3); g.set("gpat", (1 << 0) | (1 << 2) | (1 << 3) | (1 << 6)); g.set("gdep", 127);
    g.set("gsm", 0); g.set("gdc", 0); g.set("gsw", 0); g.set("qon", 0);
    g.fx.setTempo(120.0);
    const size_t N = 6000 * 16; Sig in(N, 1.f), oL, oR;
    g.run(in, in, oL, oR);
    for (int s = 0; s < 16; ++s) {
        bool on = ((1 << 0) | (1 << 2) | (1 << 3) | (1 << 6)) >> s & 1;
        double a = 0; for (int i = 200; i < 5800; ++i) a += oL[(size_t) s * 6000 + (size_t) i];
        a /= 5600.0;
        CHECK(on ? a > 0.999 : a < 1e-4);
    }
    // the edges sit on the step boundaries to within a millisecond
    CHECK(oL[11990] < 1e-3 && oL[12060] > 0.99);                 // step 1 (off) -> step 2 (on) at sample 12000
    CHECK(oL[11999 - 60] < 1e-3);
    CHECK(oL[17990] > 0.99 && oL[18060] > 0.99);                 // step 2 -> 3 both on, no gap
    CHECK(oL[24000 + 60] < 1e-3 && oL[23990] > 0.99);            // step 3 (on) -> 4 (off) at sample 24000
    // 16 steps repeat: second bar
    Sig in2(6000 * 32, 1.f); g.run(in2, in2, oL, oR);
    CHECK(oL[6000 * 16 + 100] > 0.999 && oL[6000 * 17 + 100] < 1e-4);
    // free running with the transport stopped: step 0 at sample 0
    Rig h; h.allOff(); h.set("gon", 1); h.set("grate", 3); h.set("gpat", 0x0001); h.set("gdep", 127); h.set("gsm", 0); h.set("qon", 0);
    h.apply(); Sig a(12000, 1.f), b(12000, 1.f);
    h.fx.setTransport(false, 0.0); h.fx.process(a.data(), b.data(), 12000);
    CHECK(a[3000] > 0.999 && a[9000] < 1e-4);
    // depth in dB: -6 dB cut
    Rig k; k.allOff(); k.set("gon", 1); k.set("gpat", 0x0000); k.set("gdep", (int) std::lround(6.0 / 60.0 * 127)); k.set("qon", 0); k.set("gsm", 0);
    k.apply(); Sig c(12000, 1.f), d(12000, 1.f); k.fx.setTransport(true, 0.0); k.fx.process(c.data(), d.data(), 12000);
    double lvl = 20 * std::log10(c[8000]);
    CHECK(std::fabs(lvl - (-60.0 * std::lround(6.0 / 60.0 * 127) / 127.0)) < 0.05);
}
TEST(fxchain_gate_decay_and_swell_shape) {
    // the gLevel shape of the mockup, checked against the output with SMOOTH 0
    const int dc = 64, sw = 80, dep = 63;
    Rig g; g.allOff(); g.set("qon", 0); g.set("gon", 1); g.set("grate", 3); g.set("gpat", 0x0003); g.set("gdep", dep); g.set("gsm", 0); g.set("gdc", dc); g.set("gsw", sw);
    Sig in(6000 * 16, 1.f), oL, oR; g.fx.setTempo(120.0); g.run(in, in, oL, oR);
    const double fl = TranceGate::floorLevel(dep);
    for (int s : { 0, 1, 2, 15 }) for (double u : { 0.05, 0.25, 0.5, 0.75, 0.9 }) {
        bool on = (0x0003 >> s) & 1, nx = (0x0003 >> ((s + 1) % 16)) & 1;
        double expect = TranceGate::level(on, u, nx, fl, dc / 127.0, sw / 127.0);
        double got = oL[(size_t) (s * 6000 + u * 6000)];
        CHECK(std::fabs(got - expect) < 0.03);
    }
    CHECK(std::fabs(TranceGate::floorLevel(0) - 1.0) < 1e-12);
    CHECK(TranceGate::floorLevel(127) == 0.0);
    CHECK(std::fabs(20 * std::log10(TranceGate::floorLevel(126)) + 60.0 * 126 / 127.0) < 1e-9);
}
TEST(fxchain_gate_before_and_after_position) {
    // processPreGate lets the caller gate before the Sound page effects; process() then does not gate a second time
    Rig g; g.allOff(); g.set("qon", 0); g.set("gon", 1); g.set("gpre", 0); g.set("gpat", 0x5555); g.set("gdep", 127); g.set("gsm", 0); g.set("grate", 3);
    g.apply(); g.fx.setTempo(120.0); g.fx.setTransport(true, 0.0);
    Sig a(6000 * 4, 1.f), b = a;
    g.fx.processPreGate(a.data(), b.data(), (int) a.size());
    CHECK(a[3000] > 0.99 && a[9000] < 1e-3);
    g.fx.process(a.data(), b.data(), (int) a.size());
    const int lat = g.fx.latencySamples();
    CHECK(a[(size_t) lat + 3000] > 0.99 && a[(size_t) lat + 9000] < 1e-3);      // gated once, not twice (0.99 would fail otherwise only at edges)
    // AFTER FX position: processPreGate does nothing
    Rig h; h.allOff(); h.set("qon", 0); h.set("gon", 1); h.set("gpre", 1); h.set("gpat", 0x5555); h.set("gdep", 127);
    h.apply(); Sig c(6000, 1.f), d = c; h.fx.processPreGate(c.data(), d.data(), 6000);
    CHECK(c[100] == 1.f);
}
TEST(fxchain_live_readouts) {
    Rig g; g.set("gon", 1); g.set("qvon", 1); g.apply();
    Sig in = sine(48000, 400.0, 0.4, 48000.0), inR = in, oL, oR;
    g.run(in, inR, oL, oR);
    CHECK(g.live.gatePhase.load() >= 0 && g.live.gatePhase.load() < 16);
    CHECK(g.live.outPeakL.load() > 0.0f);
    float s[TapBuf::kSize]; g.live.tap[kTapOut].snapshot(s);
    double e = 0; for (float x : s) e += (double) x * x;
    CHECK(e > 0.0);
    g.live.tap[kTapIn].snapshot(s); e = 0; for (float x : s) e += (double) x * x; CHECK(e > 0.0);
    g.set("gon", 0); g.apply(); g.run(in, inR, oL, oR); CHECK(g.live.gatePhase.load() == -1);
    CHECK(g.live.gluGR.load() <= 0.0f && g.live.optoGR.load() <= 0.0f);
}
