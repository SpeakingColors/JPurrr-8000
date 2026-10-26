// Renders a WAV file (or a generated saw chord stab pattern) through the mix-ready FxChain with a factory preset's
// settings and writes the result, so the chain can be listened to and compared with the mockup.
//   render_fx [--in in.wav] [--out out.wav] [--preset N | --type Lead|Pad|Bass|Pluck|Chord|FX] [--set id=value ...]
//             [--sr 48000] [--seconds 8] [--bpm 128] [--float] [--no-align] [--bench] [--list]
// Build: sh tools/build_fx_tool.sh  (writes build-tools/render_fx).
// The output is trimmed by the chain latency so it lines up with the input (use --no-align to keep the delay).
#include "jpurrr/fxchain.h"
#include "jpurrr/patch.h"
#include <algorithm>
#include <ctime>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace jpurrr;

static void put32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE* f, uint16_t v) { fwrite(&v, 2, 1, f); }
static bool writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r, double sr, bool flt) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    uint32_t n = (uint32_t) l.size(), bytes = flt ? 4 : 2, dataLen = n * 2 * bytes;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + dataLen); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, flt ? 3 : 1); put16(f, 2); put32(f, (uint32_t) sr); put32(f, (uint32_t) sr * 2 * bytes);
    put16(f, (uint16_t) (2 * bytes)); put16(f, (uint16_t) (bytes * 8));
    fwrite("data", 1, 4, f); put32(f, dataLen);
    for (uint32_t i = 0; i < n; ++i) {
        float v[2] = { l[i], r[i] };
        for (int c = 0; c < 2; ++c) {
            if (flt) fwrite(&v[c], 4, 1, f);
            else { float x = std::fmax(-1.f, std::fmin(1.f, v[c])); int16_t s = (int16_t) std::lrintf(x * 32767.f); fwrite(&s, 2, 1, f); }
        }
    }
    fclose(f);
    return true;
}

// Minimal WAV reader: PCM 16/24/32 and float 32, any channel count (uses the first two).
static bool readWav(const std::string& path, std::vector<float>& l, std::vector<float>& r, double& sr) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> d; uint8_t buf[65536]; size_t k;
    while ((k = fread(buf, 1, sizeof(buf), f)) > 0) d.insert(d.end(), buf, buf + k);
    fclose(f);
    if (d.size() < 44 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) return false;
    size_t p = 12; int fmt = 0, ch = 0, bits = 0; sr = 44100; const uint8_t* data = nullptr; size_t dlen = 0;
    while (p + 8 <= d.size()) {
        uint32_t len; std::memcpy(&len, &d[p + 4], 4);
        if (!std::memcmp(&d[p], "fmt ", 4)) {
            uint16_t a, c, b; uint32_t s; std::memcpy(&a, &d[p + 8], 2); std::memcpy(&c, &d[p + 10], 2); std::memcpy(&s, &d[p + 12], 4); std::memcpy(&b, &d[p + 22], 2);
            fmt = a; ch = c; sr = s; bits = b;
            if (fmt == 0xFFFE && len >= 40) { uint16_t sub; std::memcpy(&sub, &d[p + 32], 2); fmt = sub; }
        } else if (!std::memcmp(&d[p], "data", 4)) { data = &d[p + 8]; dlen = std::min<size_t>(len, d.size() - p - 8); }
        p += 8 + len + (len & 1);
    }
    if (!data || ch < 1 || !(fmt == 1 || fmt == 3)) return false;
    size_t bps = bits / 8, frames = dlen / (bps * ch);
    l.assign(frames, 0.f); r.assign(frames, 0.f);
    for (size_t i = 0; i < frames; ++i) for (int c = 0; c < std::min(ch, 2); ++c) {
        const uint8_t* q = data + (i * ch + c) * bps; float v = 0.f;
        if (fmt == 3 && bits == 32) std::memcpy(&v, q, 4);
        else if (bits == 16) { int16_t s; std::memcpy(&s, q, 2); v = s / 32768.f; }
        else if (bits == 24) { int32_t s = (q[0] << 8) | (q[1] << 16) | ((int32_t) q[2] << 24); v = (float) (s / 2147483648.0); }
        else if (bits == 32) { int32_t s; std::memcpy(&s, q, 4); v = (float) (s / 2147483648.0); }
        (c == 0 ? l : r)[i] = v;
    }
    if (ch == 1) r = l;
    return true;
}

static float polyblep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
// Detuned saw chord stabs, one chord per bar, eighth note pattern. Peak around -9 dBFS before the chain.
static void makeSource(std::vector<float>& l, std::vector<float>& r, double sr, double seconds, double bpm) {
    size_t n = (size_t) (seconds * sr); l.assign(n, 0.f); r.assign(n, 0.f);
    const double beat = 60.0 / bpm;
    const int chords[4][4] = { { 57, 64, 69, 72 }, { 53, 60, 65, 69 }, { 48, 55, 60, 64 }, { 55, 62, 67, 71 } };
    const double detune[5] = { -0.14, -0.06, 0.0, 0.06, 0.14 };
    for (int step = 0; step * beat * 0.5 < seconds; ++step) {
        int inBar = step % 8;
        if (!(inBar == 0 || inBar == 3 || inBar == 6)) continue;
        double t0 = step * beat * 0.5;
        const int* ch = chords[(step / 8) % 4];
        size_t s0 = (size_t) (t0 * sr), len = (size_t) (beat * (inBar == 6 ? 0.9 : 1.2) * sr);
        for (int v = 0; v < 4; ++v) for (int d = 0; d < 5; ++d) {
            double f = 440.0 * std::pow(2.0, (ch[v] - 69 + detune[d]) / 12.0);
            float dt = (float) (f / sr), ph = 0.17f * (float) (v * 5 + d);
            ph -= std::floor(ph);
            float pan = d / 4.f, lp = 0.f;
            for (size_t i = 0; i < len && s0 + i < n; ++i) {
                float saw = 2.f * ph - 1.f - polyblep(ph, dt);
                ph += dt; if (ph >= 1.f) ph -= 1.f;
                float t = (float) i / (float) sr;
                float env = (1.f - std::exp(-t / 0.003f)) * std::exp(-t / 0.55f);
                lp += (0.5f - 0.35f * std::exp(-t / 0.12f)) * (saw - lp);
                float y = lp * env * 0.03f;
                l[s0 + i] += y * (1.f - pan); r[s0 + i] += y * pan;
            }
        }
    }
}

int main(int argc, char** argv) {
    std::string in, out = "fx_out.wav";
    int preset = -1; std::string type; bool flt = false, align = true, bench = false;
    double sr = 48000, seconds = 8, bpm = 128; std::vector<std::pair<std::string, int>> sets;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--in") in = next(); else if (a == "--out") out = next();
        else if (a == "--preset") preset = std::atoi(next().c_str()); else if (a == "--type") type = next();
        else if (a == "--sr") sr = std::atof(next().c_str()); else if (a == "--seconds") seconds = std::atof(next().c_str());
        else if (a == "--bpm") bpm = std::atof(next().c_str()); else if (a == "--float") flt = true;
        else if (a == "--no-align") align = false; else if (a == "--bench") bench = true;
        else if (a == "--set") { std::string kv = next(); size_t e = kv.find('='); if (e != std::string::npos) sets.push_back({ kv.substr(0, e), std::atoi(kv.c_str() + e + 1) }); }
        else if (a == "--list") { for (int k = 0; k < factoryCount(); ++k) std::printf("%2d  %-20s %s\n", k, factoryPatch(k).name.c_str(), factoryPatch(k).type.c_str()); return 0; }
        else { std::printf("unknown option %s\n", a.c_str()); return 1; }
    }
    if (preset < 0 && !type.empty()) for (int k = 0; k < factoryCount(); ++k) if (factoryPatch(k).type == type) { preset = k; break; }
    if (preset < 0) preset = 0;
    Patch pt = factoryPatch(preset);
    for (auto& kv : sets) { int idx = paramIndexById(kv.first.c_str()); if (idx < 0) { std::printf("unknown parameter %s\n", kv.first.c_str()); return 1; } pt.v[idx] = kv.second; }
    std::vector<float> L, R;
    if (!in.empty()) { if (!readWav(in, L, R, sr)) { std::printf("cannot read %s\n", in.c_str()); return 1; } } else makeSource(L, R, sr, seconds, bpm);
    FxChain fx; fx.prepare(sr, 512); fx.setParams(pt.v); fx.setTempo(bpm);
    const int lat = fx.latencySamples();
    const size_t n = L.size();
    L.resize(n + lat, 0.f); R.resize(n + lat, 0.f);
    std::vector<float> oL = L, oR = R;
    double ppq = 0; const int B = 512;
    std::clock_t c0 = std::clock();
    for (size_t i = 0; i < L.size(); i += B) {
        int m = (int) std::min<size_t>(B, L.size() - i);
        fx.setTransport(true, ppq); ppq += m * bpm / 60.0 / sr;
        fx.process(oL.data() + i, oR.data() + i, m);
    }
    double secs = (double) (std::clock() - c0) / CLOCKS_PER_SEC;           // process CPU time, not wall time
    double pk = 0, pkIn = 0, e = 0, eIn = 0;
    for (size_t i = 0; i < n; ++i) {
        pkIn = std::max(pkIn, (double) std::max(std::fabs(L[i]), std::fabs(R[i])));
        eIn += (double) L[i] * L[i] + (double) R[i] * R[i];
        size_t j = align ? i + lat : i;
        pk = std::max(pk, (double) std::max(std::fabs(oL[j]), std::fabs(oR[j])));
        e += (double) oL[j] * oL[j] + (double) oR[j] * oR[j];
    }
    std::printf("preset %d \"%s\" (%s), sr %.0f, latency %d samples (%.2f ms)\n", preset, pt.name.c_str(), pt.type.c_str(), sr, lat, 1000.0 * lat / sr);
    std::printf("in : peak %.2f dBFS, rms %.2f dBFS\nout: peak %.2f dBFS, rms %.2f dBFS\n", 20 * std::log10(pkIn + 1e-12), 10 * std::log10(eIn / (2.0 * n) + 1e-12),
                20 * std::log10(pk + 1e-12), 10 * std::log10(e / (2.0 * n) + 1e-12));
    if (bench) std::printf("cpu: %.2f s for %.2f s of audio = %.2f %% of one core\n", secs, (double) L.size() / sr, 100.0 * secs / ((double) L.size() / sr));
    std::vector<float> wl, wr;
    size_t from = align ? (size_t) lat : 0;
    wl.assign(oL.begin() + from, oL.begin() + from + n); wr.assign(oR.begin() + from, oR.begin() + from + n);
    if (!writeWav(out, wl, wr, sr, flt)) { std::printf("cannot write %s\n", out.c_str()); return 1; }
    std::printf("wrote %s\n", out.c_str());
    return 0;
}
