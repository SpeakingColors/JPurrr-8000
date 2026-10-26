// Offline renderer for the synth engine: plays notes on a factory preset (or default patch) and writes a WAV.
//   render_synth [--preset N|NAME] [--set id=value ...] [--notes "60@0+1.5,64@0+1.5"] [--len 3] [--sr 48000]
//                [--block 256] [--out file.wav] [--pcm16] [--bench] [--list]
// Notes are MIDI note @ start seconds + length seconds, optional :velocity (default 100).
// Build with tools/build_synth_tools.sh.
#include "jpurrr/synth.h"
#include "jpurrr/patch.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace jpurrr;

struct Ev { double t; int note, vel; bool on; };

static void writeWav(const char* path, const std::vector<float>& l, const std::vector<float>& r, int sr, bool pcm16) {
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::perror(path); return; }
    const uint32_t n = (uint32_t) l.size(), bytes = pcm16 ? 2 : 4, dataLen = n * 2 * bytes;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32(36 + dataLen); std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(pcm16 ? 1 : 3); w16(2); w32((uint32_t) sr); w32((uint32_t) sr * 2 * bytes); w16((uint16_t) (2 * bytes)); w16((uint16_t) (bytes * 8));
    std::fwrite("data", 1, 4, f); w32(dataLen);
    for (uint32_t i = 0; i < n; ++i) {
        if (pcm16) {
            int16_t a = (int16_t) std::lround(std::fmax(-1.f, std::fmin(1.f, l[i])) * 32767.f), b = (int16_t) std::lround(std::fmax(-1.f, std::fmin(1.f, r[i])) * 32767.f);
            std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
        } else { std::fwrite(&l[i], 4, 1, f); std::fwrite(&r[i], 4, 1, f); }
    }
    std::fclose(f);
}

int main(int argc, char** argv) {
    int preset = 0, sr = 48000, block = 256; double len = 3.0; const char* out = "render.wav"; bool pcm16 = false, bench = false;
    std::string notes = "60@0+2"; std::vector<std::pair<int, int>> sets; std::string presetName;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--preset") { std::string s = next(); if (!s.empty() && std::isdigit((unsigned char) s[0])) preset = std::atoi(s.c_str()); else presetName = s; }
        else if (a == "--set") { std::string s = next(); size_t e = s.find('='); int idx = paramIndexById(s.substr(0, e).c_str()); if (idx < 0 || e == std::string::npos) { std::fprintf(stderr, "unknown param %s\n", s.c_str()); return 1; } sets.push_back({ idx, std::atoi(s.c_str() + e + 1) }); }
        else if (a == "--notes") notes = next();
        else if (a == "--len") len = std::atof(next());
        else if (a == "--sr") sr = std::atoi(next());
        else if (a == "--block") block = std::atoi(next());
        else if (a == "--out") out = next();
        else if (a == "--pcm16") pcm16 = true;
        else if (a == "--bench") bench = true;
        else if (a == "--list") { for (int k = 0; k < factoryCount(); ++k) std::printf("%2d %s\n", k, factoryPatch(k).name.c_str()); return 0; }
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 1; }
    }
    if (!presetName.empty()) { preset = -1; for (int k = 0; k < factoryCount(); ++k) if (factoryPatch(k).name == presetName) preset = k; if (preset < 0) { std::fprintf(stderr, "no preset %s\n", presetName.c_str()); return 1; } }
    Patch p = factoryPatch(preset);
    for (auto& s : sets) p.v[s.first] = s.second;
    p.clampAll();

    std::vector<Ev> evs;
    {
        size_t pos = 0;
        while (pos < notes.size()) {
            size_t e = notes.find(',', pos); if (e == std::string::npos) e = notes.size();
            std::string tok = notes.substr(pos, e - pos); pos = e + 1;
            int note = 60, vel = 100; double st = 0, ln = 1;
            size_t at = tok.find('@'), pl = tok.find('+'), co = tok.find(':');
            note = std::atoi(tok.c_str());
            if (at != std::string::npos) st = std::atof(tok.c_str() + at + 1);
            if (pl != std::string::npos) ln = std::atof(tok.c_str() + pl + 1);
            if (co != std::string::npos) vel = std::atoi(tok.c_str() + co + 1);
            evs.push_back({ st, note, vel, true }); evs.push_back({ st + ln, note, vel, false });
        }
    }
    if (bench) {                                        // 10 voices of Super Saw, 24 dB filter, 10 s at 48 kHz
        p.reset(); p.v[P_vc] = 10;
        Synth s; s.prepare(sr, block); s.setParams(p.v);
        const int total = sr * 10; std::vector<float> l((size_t) block), r((size_t) block);
        for (int k = 0; k < 10; ++k) s.noteOn(0, 40 + k * 3, 100);
        auto t0 = std::chrono::steady_clock::now();
        double sink = 0;
        for (int d = 0; d < total; d += block) { s.process(l.data(), r.data(), block); sink += l[0]; }
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("bench: 10 voices Super Saw + 24 dB filter, %d Hz (oversampling %dx): %.2f %% of one core (sink %g)\n", sr, s.oversampling(), sec / 10.0 * 100.0, sink);
        return 0;
    }
    Synth s; s.prepare(sr, block); s.setParams(p.v);
    const int total = (int) (len * sr);
    std::vector<float> L((size_t) total), R((size_t) total);
    size_t ei = 0; int done = 0;
    std::vector<Ev> sorted = evs; std::stable_sort(sorted.begin(), sorted.end(), [](const Ev& a, const Ev& b) { return a.t < b.t; });
    while (done < total) {
        int n = std::min(block, total - done);
        while (ei < sorted.size() && sorted[ei].t * sr < done + n) {
            int off = std::max(0, (int) (sorted[ei].t * sr) - done);
            if (off > 0) { s.process(L.data() + done, R.data() + done, off); done += off; n -= off; }
            if (sorted[ei].on) s.noteOn(0, sorted[ei].note, sorted[ei].vel); else s.noteOff(0, sorted[ei].note);
            ++ei;
        }
        if (n > 0) { s.process(L.data() + done, R.data() + done, n); done += n; }
    }
    double pk = 0, e = 0;
    for (int i = 0; i < total; ++i) { pk = std::fmax(pk, std::fmax(std::fabs(L[(size_t) i]), std::fabs(R[(size_t) i]))); e += (double) L[(size_t) i] * L[(size_t) i] + (double) R[(size_t) i] * R[(size_t) i]; }
    std::printf("preset %d '%s': peak %.3f (%.1f dBFS) rms %.1f dBFS, %d samples at %d Hz -> %s\n", preset, factoryPatch(preset).name.c_str(), pk, 20 * std::log10(pk + 1e-12),
                10 * std::log10(e / (2.0 * total) + 1e-12), total, sr, out);
    writeWav(out, L, R, sr, pcm16);
    return 0;
}
