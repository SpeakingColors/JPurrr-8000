// Renders the Sound page effects (SoundFx) to WAV files so they can be listened to and analysed.
//   render_soundfx [outdir] [--float]
// Source: a stab pattern of detuned saws (polyBLEP) at 128 bpm, 8 seconds, 44.1 kHz stereo.
// Writes dry.wav, tone_*.wav, delay_*.wav and fx_*.wav. 16 bit PCM by default, 32 bit float with --float.
#include "jpurrr/soundfx.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>

using namespace jpurrr;

static const double kSr = 44100.0;
static bool g_float = false;

static void put32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE* f, uint16_t v) { fwrite(&v, 2, 1, f); }
static void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { std::printf("cannot write %s\n", path.c_str()); return; }
    uint32_t n = (uint32_t)l.size(), bytes = g_float ? 4 : 2, dataLen = n * 2 * bytes;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + dataLen); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, g_float ? 3 : 1); put16(f, 2); put32(f, (uint32_t)kSr); put32(f, (uint32_t)kSr * 2 * bytes);
    put16(f, (uint16_t)(2 * bytes)); put16(f, (uint16_t)(bytes * 8));
    fwrite("data", 1, 4, f); put32(f, dataLen);
    for (uint32_t i = 0; i < n; ++i) {
        float v[2] = { l[i], r[i] };
        for (int c = 0; c < 2; ++c) {
            if (g_float) fwrite(&v[c], 4, 1, f);
            else { float x = std::fmax(-1.f, std::fmin(1.f, v[c])); int16_t s = (int16_t)std::lrintf(x * 32767.f); fwrite(&s, 2, 1, f); }
        }
    }
    fclose(f);
}

static float polyblep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

static void makeSource(std::vector<float>& l, std::vector<float>& r, double seconds) {
    size_t n = (size_t)(seconds * kSr);
    l.assign(n, 0.f); r.assign(n, 0.f);
    const double beat = 60.0 / 128.0;
    const int notes[2][4] = { { 45, 52, 57, 60 }, { 43, 50, 55, 59 } };    // Am, G over the bars
    const double detune[3] = { -0.12, 0.0, 0.12 };
    for (int step = 0; step < 16; ++step) {                                   // eighth notes, stab on 1, 2+, 4 of each bar
        int inBar = step % 8;
        if (!(inBar == 0 || inBar == 3 || inBar == 6)) continue;
        double t0 = step * beat * 0.5;
        const int* ch = notes[(step / 8) % 2];
        size_t s0 = (size_t)(t0 * kSr), len = (size_t)(0.45 * kSr);
        for (int v = 0; v < 4; ++v) for (int d = 0; d < 3; ++d) {
            double f = 440.0 * std::pow(2.0, (ch[v] - 69 + detune[d]) / 12.0);
            float dt = (float)(f / kSr), ph = 0.13f * (float)(v * 3 + d);
            float lp = 0.f;
            float pan = (d - 1) * 0.5f + 0.5f;                                // 0..1
            for (size_t i = 0; i < len && s0 + i < n; ++i) {
                float saw = 2.f * ph - 1.f - polyblep(ph, dt);
                ph += dt; if (ph >= 1.f) ph -= 1.f;
                float env = std::exp(-(float)i / (0.18f * (float)kSr)) * (1.f - std::exp(-(float)i / (0.002f * (float)kSr)));
                lp += (0.35f - 0.25f * std::exp(-(float)i / (0.08f * (float)kSr))) * (saw - lp);
                float y = lp * env * 0.025f;
                l[s0 + i] += y * (1.f - pan); r[s0 + i] += y * pan;
            }
        }
    }
}

struct Params { int v[kNumParams]; Params() { for (int i = 0; i < kNumParams; ++i) v[i] = kParams[i].def; v[P_dl] = 0; v[P_xl] = 0; } };

static void render(const std::string& dir, const std::string& name, const std::vector<float>& sl, const std::vector<float>& sr,
                   const Params& p, double bpm = 128.0, double lfo1 = 0.0) {
    SoundFx fx;
    fx.prepare(kSr, 512);
    fx.setTempo(bpm); fx.setLfo1Hz(lfo1);
    std::vector<float> l = sl, r = sr;
    for (size_t pos = 0; pos < l.size(); pos += 512) {
        fx.setParams(p.v);
        fx.process(l.data() + pos, r.data() + pos, (int)std::min<size_t>(512, l.size() - pos));
    }
    writeWav(dir + "/" + name + ".wav", l, r);
    double pk = 0, e = 0;
    for (size_t i = 0; i < l.size(); ++i) { pk = std::fmax(pk, std::fmax(std::fabs(l[i]), std::fabs(r[i]))); e += l[i] * l[i] + r[i] * r[i]; }
    std::printf("%-28s peak %.3f  rms %.2f dB\n", name.c_str(), pk, 10.0 * std::log10(e / (2.0 * l.size()) + 1e-20));
}

int main(int argc, char** argv) {
    std::string dir = "soundfx_out";
    for (int i = 1; i < argc; ++i) { if (!std::strcmp(argv[i], "--float")) g_float = true; else dir = argv[i]; }
    mkdir(dir.c_str(), 0755);
    std::vector<float> sl, sr;
    makeSource(sl, sr, 8.0);
    Params base;
    render(dir, "dry", sl, sr, base);

    { Params p = base; p.v[P_tb] = 40; render(dir, "tone_bass_up", sl, sr, p); }
    { Params p = base; p.v[P_tt] = 40; render(dir, "tone_treble_up", sl, sr, p); }
    { Params p = base; p.v[P_tb] = -50; p.v[P_tt] = -50; render(dir, "tone_both_down", sl, sr, p); }

    const char* dn[5] = { "panning_lr", "panning_rl", "panning_short", "mono_short", "mono_long" };
    for (int t = 0; t < 5; ++t) {
        Params p = base; p.v[P_dty] = t; p.v[P_dl] = 80; p.v[P_dfb] = 70; p.v[P_dt] = t == 2 || t == 3 ? 100 : (t == 4 ? 50 : 60);
        render(dir, std::string("delay_") + dn[t], sl, sr, p);
    }
    { Params p = base; p.v[P_dty] = 0; p.v[P_dl] = 80; p.v[P_dfb] = 70; p.v[P_dsy] = 6; render(dir, "delay_sync_eighth_dotted", sl, sr, p); }

    const char* fn[13] = { "super_chorus_slw", "super_chorus_mid", "super_chorus_fst", "super_chorus_clr", "flanger_slow", "flanger_deep",
                           "flanger_fast", "deep_phasing_slw", "jet_phasing", "twisting", "freeze_phase_1", "freeze_phase_2", "distortion" };
    for (int t = 0; t < 13; ++t) {
        Params p = base; p.v[P_xt] = t; p.v[P_xl] = t == 12 ? 80 : 90;
        render(dir, std::string("fx_") + fn[t], sl, sr, p);
    }
    { Params p = base; p.v[P_xt] = 1; p.v[P_xl] = 100; p.v[P_csy] = 7; render(dir, "fx_chorus_sync_quarter", sl, sr, p); }
    { Params p = base; p.v[P_xt] = 12; p.v[P_xl] = 127; render(dir, "fx_distortion_full", sl, sr, p); }
    { Params p = base; p.v[P_tb] = 10; p.v[P_xt] = 1; p.v[P_xl] = 55; p.v[P_dl] = 22; p.v[P_dt] = 70; p.v[P_dfb] = 40; render(dir, "defaults_chain", sl, sr, p); }
    std::printf("written to %s/\n", dir.c_str());
    return 0;
}
