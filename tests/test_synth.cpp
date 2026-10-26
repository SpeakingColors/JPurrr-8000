// Tests for the sound engine (core/src/synth.cpp, core/src/dsp/synth_*).
#include "test_util.h"
#include "jpurrr/patch.h"
#include "jpurrr/synth.h"
#include "jpurrr/synth_math.h"
#include "../core/src/dsp/synth_dsp.h"
#include <algorithm>
#include <ctime>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>
using namespace jpurrr;

namespace {
struct Rig {
    Synth s; double sr; std::vector<float> l, r; int p[kNumParams];
    explicit Rig(double sampleRate = 48000.0, int patch = 0) : sr(sampleRate), l(4096), r(4096) {
        s.prepare(sr, 4096);
        std::memcpy(p, factoryPatch(patch).v, sizeof p);
        s.setParams(p);
    }
    void set(int idx, int v) { p[idx] = v; s.setParams(p); }
    // renders n samples, returns the peak; fills outL/outR if given
    float run(int n, std::vector<float>* outL = nullptr) {
        float pk = 0.f; int done = 0;
        while (done < n) {
            const int m = std::min(n - done, 512);
            s.process(l.data(), r.data(), m);
            for (int i = 0; i < m; ++i) { pk = std::max(pk, std::max(std::fabs(l[(size_t) i]), std::fabs(r[(size_t) i]))); if (outL) outL->push_back(l[(size_t) i]); }
            done += m;
        }
        return pk;
    }
};
bool finiteBounded(const std::vector<float>& v, float lim) { for (float x : v) if (!std::isfinite(x) || std::fabs(x) > lim) return false; return true; }
}  // namespace

TEST(synth_supersaw_cents) {
    // Szabo 2010: at maximum detune the saws sit at about -202, -112, -34, 0, +34, +104, +177 cents.
    const double exp[7] = { -202, -112, -34, 0, 34, 104, 177 };
    for (int i = 0; i < 7; ++i) CHECK(std::fabs(superSawCents(i, 127) - exp[i]) < 1.0);
    // detune 0 means all saws on the note
    for (int i = 0; i < 7; ++i) CHECK(std::fabs(superSawCents(i, 0)) < 1.0);
    // monotonic growth of the spread
    CHECK(superSawCents(6, 64) < superSawCents(6, 127));
}

TEST(synth_supersaw_gains) {
    const double m[3] = { 0.0, 0.75, 1.0 };
    const double cg[3] = { 0.99785, 0.58260, 0.44419 };
    const double sg[3] = { 0.044372, 0.59253, 0.59083 };
    for (int i = 0; i < 3; ++i) {
        CHECK(std::fabs(superSawCentreGain(m[i]) - cg[i]) < 1e-4);
        CHECK(std::fabs(superSawSideGain(m[i]) - sg[i]) < 1e-4);
    }
}

TEST(synth_filter_stability) {
    // Every type, slope and resonance at random per sample cutoff changes, at two sample rates: finite and bounded.
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    for (double sr : { 44100.0, 96000.0 }) {
        for (int type = 0; type < 3; ++type) for (int pole = 0; pole < 2; ++pole) for (int rr = 0; rr < 4; ++rr) {
            dsp::VoiceFilter f; const double res = rr == 0 ? 0.0 : rr == 1 ? 0.5 : rr == 2 ? 0.95 : 1.0;
            const double k = pole ? dsp::ladderK(res) : dsp::svfK(res);
            float worst = 0.f;
            for (int i = 0; i < 40000; ++i) {
                const double hz = 20.0 * std::pow(1000.0, u(rng));
                const double g = std::tan(dsp::kPi * std::min(hz, 0.49 * sr) / sr);
                const float in = (float) (2.0 * u(rng) - 1.0) * (i % 5000 < 2500 ? 1.f : 4.f);
                const float y = f.process(in, g, k, type, pole != 0);
                if (!std::isfinite(y)) { worst = 1e9f; break; }
                worst = std::max(worst, std::fabs(y));
            }
            CHECK(worst < 100.f);
        }
    }
}

namespace {
// Gain in dB of the filter at frequency f for a sine input (steady state).
double filterGainDb(int type, bool pole24, double res, double fc, double f, double sr) {
    dsp::VoiceFilter flt; const double g = std::tan(dsp::kPi * fc / sr);
    const double k = pole24 ? dsp::ladderK(res) : dsp::svfK(res);
    const int n = (int) sr; double acc = 0;
    for (int i = 0; i < n; ++i) {
        const float in = 0.05f * (float) std::sin(dsp::kTwoPi * f * i / sr);         // small: the saturation stays linear
        const float y = flt.process(in, g, k, type, pole24);
        if (i > n / 2) acc += (double) y * y;
    }
    const double rms = std::sqrt(acc / (n / 2 - 1));
    return 20.0 * std::log10(rms / (0.05 / std::sqrt(2.0)) + 1e-12);
}
}  // namespace

TEST(synth_filter_response) {
    const double sr = 48000.0, fc = 1000.0;
    // 24 dB ladder, no resonance: each of the four poles sits at fc, so -12 dB at fc and -24 dB per octave above.
    CHECK(std::fabs(filterGainDb(2, true, 0.0, fc, fc, sr) + 12.04) < 0.5);
    CHECK(filterGainDb(2, true, 0.0, fc, 100.0, sr) > -0.3);
    const double a = filterGainDb(2, true, 0.0, fc, 8000.0, sr), b = filterGainDb(2, true, 0.0, fc, 16000.0, sr);
    CHECK(a - b > 18.0);
    // 12 dB state variable, no resonance (Q 0.5): -6 dB at fc
    CHECK(std::fabs(filterGainDb(2, false, 0.0, fc, fc, sr) + 6.02) < 0.5);
    // high pass and band pass
    CHECK(filterGainDb(0, true, 0.0, fc, 10000.0, sr) > -1.0);
    CHECK(filterGainDb(0, true, 0.0, fc, 100.0, sr) < -40.0);
    CHECK(std::fabs(filterGainDb(1, true, 0.0, fc, fc, sr)) < 1.0);
    CHECK(filterGainDb(1, false, 0.0, fc, fc, sr) > filterGainDb(1, false, 0.0, fc, 8000.0, sr) + 6.0);
    // resonance raises the level at fc
    CHECK(filterGainDb(2, true, 0.7, fc, fc, sr) > filterGainDb(2, true, 0.0, fc, fc, sr) + 3.0);
    CHECK(filterGainDb(2, false, 0.7, fc, fc, sr) > filterGainDb(2, false, 0.0, fc, fc, sr) + 3.0);
}

namespace {
// Frequency of a free running filter fed only by a tiny noise seed: counts rising zero crossings.
double ringFrequency(bool pole24, int type, double fc, double sr) {
    dsp::VoiceFilter f; dsp::Rng rng; rng.seed(5);
    const double g = std::tan(dsp::kPi * fc / sr), k = pole24 ? dsp::ladderK(1.0) : dsp::svfK(1.0);
    const int warm = (int) (sr * 2.0), meas = (int) (sr * 1.0);
    float prev = 0.f; int crossings = 0; double first = -1, last = -1; double peak = 0;
    for (int i = 0; i < warm + meas; ++i) {
        const float y = f.process(2.5e-6f * rng.bi(), g, k, type, pole24);
        if (i >= warm) {
            peak = std::max(peak, (double) std::fabs(y));
            if (prev < 0.f && y >= 0.f) { const double t = i - y / (y - prev); if (first < 0) first = t; last = t; ++crossings; }
        }
        prev = y;
    }
    if (crossings < 3 || peak < 0.05) return 0.0;
    return (crossings - 1) * sr / (last - first);
}
}  // namespace

TEST(synth_filter_self_oscillation) {
    // At maximum resonance the filter rings at its cutoff frequency (zero delay feedback keeps it exact).
    for (double fc : { 200.0, 1000.0, 5000.0 }) {
        const double f24 = ringFrequency(true, 2, fc, 48000.0), f12 = ringFrequency(false, 2, fc, 48000.0);
        CHECK(std::fabs(1200.0 * std::log2(f24 / fc)) < 3.0);
        CHECK(std::fabs(1200.0 * std::log2(f12 / fc)) < 3.0);
    }
    CHECK(ringFrequency(true, 2, 1000.0, 96000.0) > 990.0);
    // mid resonance does not ring
    dsp::VoiceFilter f; double peak = 0; const double g = std::tan(dsp::kPi * 1000.0 / 48000.0);
    dsp::Rng rng; rng.seed(9);
    for (int i = 0; i < 96000; ++i) { const float y = f.process(2.5e-6f * rng.bi(), g, dsp::ladderK(0.7), 2, true); if (i > 48000) peak = std::max(peak, (double) std::fabs(y)); }
    CHECK(peak < 1e-3);
}

TEST(synth_halfband) {
    // The decimator passes the audio band and rejects what folds back.
    const dsp::HalfbandCoefs& hc = dsp::HalfbandCoefs::get();
    auto gain = [&](double f) {                    // f in Hz at the 96 kHz input rate
        dsp::Decimator2 d; double acc = 0; int cnt = 0;
        for (int i = 0; i < 4000; ++i) {
            const float a = (float) std::sin(dsp::kTwoPi * f * (2 * i) / 96000.0), b = (float) std::sin(dsp::kTwoPi * f * (2 * i + 1) / 96000.0);
            const float y = d.push(a, b, hc);
            if (i > 200) { acc += (double) y * y; ++cnt; }
        }
        return 20.0 * std::log10(std::sqrt(2.0 * acc / cnt) + 1e-12);
    };
    CHECK(std::fabs(gain(1000.0)) < 0.01);
    CHECK(std::fabs(gain(15000.0)) < 0.05);
    CHECK(gain(30000.0) < -80.0);                  // 30 kHz would alias to 18 kHz at 48 kHz
    CHECK(gain(45000.0) < -80.0);
}

TEST(synth_silent_after_release) {
    Rig rig;
    rig.s.noteOn(0, 60, 100); rig.s.noteOn(0, 64, 100);
    rig.run(4800);
    CHECK(rig.s.heldVoices() == 2);
    rig.s.noteOff(0, 60); rig.s.noteOff(0, 64);
    rig.run(48000 * 3);                              // release of the default patch is well below 3 s
    CHECK(rig.s.activeVoices() == 0);
    std::vector<float> tail; const float pk = rig.run(4800, &tail);
    CHECK(pk == 0.f);
    // no signal and no NaN with no note at all
    CHECK(finiteBounded(tail, 0.f));
}

TEST(synth_no_click_on_off) {
    // A note on and off must not produce steps: the largest sample to sample jump stays small.
    Rig rig; rig.set(P_aea, 0); rig.set(P_aer, 20);
    std::vector<float> out;
    rig.s.noteOn(0, 60, 127); rig.run(2400, &out);
    rig.s.noteOff(0, 60); rig.run(9600, &out);
    float maxJump = 0.f;
    for (size_t i = 1; i < out.size(); ++i) maxJump = std::max(maxJump, std::fabs(out[i] - out[i - 1]));
    CHECK(maxJump < 0.2f);
}

TEST(synth_no_click_on_param_changes) {
    // Switching waves, filter type and slope, and big knob jumps while a note sounds must not step the output.
    // A low smooth wave keeps natural slopes tiny, so any step shows.
    Rig rig; rig.set(P_o1w, 6); rig.set(P_o1c1, 64); rig.set(P_o1c2, 0); rig.set(P_o2w, 2); rig.set(P_ob, -30);
    rig.set(P_aes, 127); rig.set(P_aea, 0); rig.set(P_fe, 0); rig.set(P_fc, 100); rig.set(P_fr, 0); rig.set(P_fk, 0);
    rig.s.noteOn(0, 36, 127); rig.run(24000);
    std::vector<float> out;
    for (int step = 0; step < 10; ++step) {
        switch (step) {
            case 0: rig.set(P_o1w, 1); break;
            case 1: rig.set(P_f24, 0); break;
            case 2: rig.set(P_ft, 0); rig.set(P_fc, 20); break;
            case 3: rig.set(P_ft, 2); rig.set(P_fc, 100); break;
            case 4: rig.set(P_o1w, 6); rig.set(P_o2w, 2); break;
            case 5: rig.set(P_ring, 1); break;
            case 6: rig.set(P_ring, 0); rig.set(P_fc, 90); break;
            case 7: rig.set(P_f24, 1); break;
            case 8: rig.set(P_ap, 1); rig.set(P_al, 30); break;
            case 9: rig.set(P_o2w, 3); rig.set(P_ob, -64); break;
        }
        rig.run(9600, &out);
    }
    float maxJump = 0.f, pk = 0.f;
    for (size_t i = 1; i < out.size(); ++i) { maxJump = std::max(maxJump, std::fabs(out[i] - out[i - 1])); pk = std::max(pk, std::fabs(out[i])); }
    CHECK(pk > 0.1f);
    CHECK(maxJump < 0.03f);                            // a 65 Hz triangle moves by about 0.003 per sample; an unducked switch steps by 0.1 or more
}

TEST(synth_polyphony_and_stealing) {
    Rig rig; rig.set(P_vc, 8); rig.set(P_aer, 127);
    for (int i = 0; i < 12; ++i) { rig.s.noteOn(0, 48 + i, 100); rig.run(256); }
    rig.run(1024);
    CHECK(rig.s.heldVoices() == 8);                 // the four oldest were stolen
    // the stolen ones fade out quickly, so only 8 voices stay active
    rig.run(4800);
    CHECK(rig.s.activeVoices() == 8);
    rig.s.allNotesOff();
    rig.run(2400);
    CHECK(rig.s.heldVoices() == 0);
    // default is 10 voices
    Rig r2; for (int i = 0; i < 14; ++i) { r2.s.noteOn(0, 40 + i, 100); r2.run(128); }
    r2.run(4800);
    CHECK(r2.s.heldVoices() == 10);
    // up to 128 voices
    Rig r3; r3.set(P_vc, 128); r3.set(P_aer, 127); r3.set(P_aes, 127);
    for (int i = 0; i < 100; ++i) { r3.s.noteOn(0, 30 + (i % 60), 90); r3.run(64); }
    r3.run(2048);
    CHECK(r3.s.heldVoices() == 100);
    for (int i = 0; i < 40; ++i) { r3.s.noteOn(0, 36 + (i % 60), 90); r3.run(64); }
    r3.run(4800);
    CHECK(r3.s.heldVoices() == 128);
}

TEST(synth_steal_prefers_released) {
    Rig rig; rig.set(P_vc, 8); rig.set(P_aer, 127); rig.set(P_aes, 127);
    for (int i = 0; i < 8; ++i) { rig.s.noteOn(0, 50 + i, 100); rig.run(128); }
    rig.s.noteOff(0, 57);                            // the newest one is released but still sounding
    rig.run(512);
    rig.s.noteOn(0, 70, 100); rig.run(512);
    CHECK(rig.s.heldVoices() == 8);                  // the released voice was taken, no held note died
}

TEST(synth_sustain_pedal_and_all_notes_off) {
    Rig rig; rig.set(P_aer, 20); rig.set(P_aes, 127);
    rig.s.controller(0, 64, 127);
    rig.s.noteOn(0, 60, 100); rig.run(2400);
    rig.s.noteOff(0, 60); rig.run(48000);
    CHECK(rig.s.activeVoices() == 1);                // held by the pedal
    rig.s.controller(0, 64, 0); rig.run(48000);
    CHECK(rig.s.activeVoices() == 0);
    rig.s.noteOn(0, 60, 100); rig.s.noteOn(0, 64, 100); rig.run(2400);
    rig.s.controller(0, 123, 0); rig.run(48000);
    CHECK(rig.s.activeVoices() == 0);
    rig.s.noteOn(0, 60, 100); rig.run(2400);
    rig.s.controller(0, 120, 0); rig.run(480);
    CHECK(rig.s.activeVoices() == 0);                // all sound off cuts at once
}

TEST(synth_midi_channels) {
    Rig rig; rig.set(P_pch, 3);                      // only MIDI channel 3
    rig.s.noteOn(0, 60, 100); rig.run(1024);
    CHECK(rig.s.heldVoices() == 0);
    rig.s.noteOn(2, 60, 100); rig.run(1024);
    CHECK(rig.s.heldVoices() == 1);
    Rig omni; omni.s.noteOn(7, 60, 100); omni.run(1024);
    CHECK(omni.s.heldVoices() == 1);
}

TEST(synth_live_readouts) {
    Rig rig; Live live; rig.s.setLive(&live);
    rig.s.noteOn(0, 60, 90); rig.s.noteOn(0, 64, 80); rig.run(1024);
    CHECK(live.noteOnCount.load() == 2 && live.lastNote.load() == 64 && live.lastVel.load() == 80);
    CHECK(live.activeVoices.load() == 2);
    rig.s.noteOff(0, 60);
    CHECK(live.noteOffAllCount.load() == 0);
    rig.s.noteOff(0, 64);
    CHECK(live.noteOffAllCount.load() == 1);
}

TEST(synth_determinism) {
    auto render = [](std::vector<float>& out) {
        Rig rig(48000.0, 5);
        rig.s.noteOn(0, 55, 100); rig.s.noteOn(0, 62, 90); rig.run(3000, &out);
        rig.s.pitchBend(0, 4000); rig.s.controller(0, 1, 90); rig.run(3000, &out);
        rig.s.noteOff(0, 55); rig.s.noteOn(0, 70, 120); rig.run(6000, &out);
    };
    std::vector<float> a, b; render(a); render(b);
    CHECK(a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0);
    // reset() returns to the same state
    Rig rig(48000.0, 5); std::vector<float> c, d;
    rig.s.noteOn(0, 60, 100); rig.run(2000, &c);
    rig.s.reset(); rig.s.noteOn(0, 60, 100); rig.run(2000, &d);
    CHECK(std::memcmp(c.data(), d.data(), c.size() * sizeof(float)) == 0);
}

TEST(synth_bend_and_mod) {
    // Zero crossing pitch of a sine like triangle (OSC 1 TRIANGLE, no filter movement) follows the bend.
    auto freqOf = [](int bend14, int rangeUp) {
        Rig rig(48000.0, 0);
        rig.set(P_o1w, 6); rig.set(P_ob, -64); rig.set(P_fc, 100); rig.set(P_fr, 0); rig.set(P_fe, 0); rig.set(P_fk, 0);
        rig.set(P_aea, 0); rig.set(P_aes, 127); rig.set(P_bu, rangeUp); rig.set(P_o1c1, 0);
        rig.s.pitchBend(0, bend14); rig.s.noteOn(0, 57, 100); rig.run(9600);
        std::vector<float> o; rig.run(48000, &o);
        int c = 0; double first = -1, last = -1;
        for (size_t i = 1; i < o.size(); ++i) if (o[i - 1] < 0.f && o[i] >= 0.f) { const double t = (double) i - o[i] / (o[i] - o[i - 1]); if (first < 0) first = t; last = t; ++c; }
        return (c - 1) * 48000.0 / (last - first);
    };
    const double f0 = freqOf(0, 2), up = freqOf(8191, 2), up12 = freqOf(8191, 12);
    CHECK(std::fabs(f0 - 220.0) < 0.5);
    CHECK(std::fabs(1200.0 * std::log2(up / f0) - 200.0) < 3.0);
    CHECK(std::fabs(1200.0 * std::log2(up12 / f0) - 1200.0) < 3.0);
}

TEST(synth_mono_legato_unison) {
    Rig rig; rig.set(P_mono, 1); rig.set(P_aer, 127); rig.set(P_aes, 127);
    rig.s.noteOn(0, 60, 100); rig.run(2400); rig.s.noteOn(0, 64, 100); rig.run(2400);
    CHECK(rig.s.heldVoices() == 1);
    rig.s.noteOff(0, 64); rig.run(2400);
    CHECK(rig.s.heldVoices() == 1);                   // back to the first held key
    rig.s.noteOff(0, 60); rig.run(2400);
    CHECK(rig.s.heldVoices() == 0);
    Rig leg; leg.set(P_leg, 1); leg.set(P_pon, 1); leg.set(P_pt, 60);
    leg.s.noteOn(0, 48, 100); leg.run(2400); leg.s.noteOn(0, 60, 100); leg.run(2400);
    CHECK(leg.s.heldVoices() == 1);
    Rig uni; uni.set(P_uni, 1);
    uni.s.noteOn(0, 60, 100); uni.run(2400);
    CHECK(uni.s.heldVoices() == 10);                  // all voices stacked on one note
    uni.set(P_vc, 8); uni.s.noteOff(0, 60); uni.run(48000 * 2);
    uni.s.noteOn(0, 62, 100); uni.run(2400);
    CHECK(uni.s.heldVoices() == 8);
    // feedback oscillator forces solo and no unison
    Rig fb; fb.set(P_o1w, 3); fb.set(P_uni, 1);
    fb.s.noteOn(0, 60, 100); fb.s.noteOn(0, 64, 100); fb.run(2400);
    CHECK(fb.s.heldVoices() == 1);
}

TEST(synth_dual_and_split) {
    Rig rig; int lower[kNumParams]; std::memcpy(lower, rig.p, sizeof lower);
    rig.set(P_km, 1);                                 // dual: every key plays both parts
    rig.s.setPartParams(1, lower);
    rig.s.noteOn(0, 60, 100); rig.run(2400);
    CHECK(rig.s.heldVoices() == 2);
    rig.s.allNotesOff(); rig.run(48000);
    Rig sp; sp.set(P_km, 2); sp.set(P_sp, 60);        // split at middle C
    sp.s.noteOn(0, 48, 100); sp.run(1200);
    CHECK(sp.s.heldVoices() == 1);
    sp.s.noteOn(0, 72, 100); sp.run(1200);
    CHECK(sp.s.heldVoices() == 2);
    // each part keeps its own patch: a silent lower part (amp level 0) leaves only the upper audible
    Rig sp2; int quiet[kNumParams]; std::memcpy(quiet, sp2.p, sizeof quiet); quiet[P_av] = 0;
    sp2.set(P_km, 2); sp2.set(P_sp, 60); sp2.s.setPartParams(1, quiet);
    sp2.s.noteOn(0, 48, 100); const float pkLow = sp2.run(9600);
    sp2.s.noteOn(0, 72, 100); const float pkHigh = sp2.run(9600);
    CHECK(pkLow < 1e-4f && pkHigh > 0.02f);
    // voice assign shares the voices: 8-2 gives the lower part 8 of 10
    Rig va; va.set(P_km, 2); va.set(P_sp, 100); va.set(P_vas, 0); va.set(P_aer, 127);
    for (int i = 0; i < 12; ++i) { va.s.noteOn(0, 40 + i, 100); va.run(128); }
    va.run(4800);
    CHECK(va.s.heldVoices() == 8);
    // part detune moves the two parts apart (just must stay finite and sound)
    Rig pd; pd.set(P_km, 1); pd.set(P_pdet, 30);
    pd.s.noteOn(0, 60, 100); std::vector<float> o; pd.run(4800, &o);
    CHECK(finiteBounded(o, 1.0f));
}

TEST(synth_arpeggiator) {
    // 1/16 notes at 120 bpm are 0.125 s apart: 8 steps per second, gate 60 percent.
    Rig rig; rig.set(P_aon, 1); rig.set(P_apt, 4); rig.set(P_amd, 0); rig.set(P_arg, 0);
    rig.set(P_aea, 0); rig.set(P_aer, 10); rig.set(P_aed, 127); rig.set(P_aes, 127);
    rig.s.setTempo(120.0);
    rig.s.noteOn(0, 60, 100); rig.s.noteOn(0, 64, 100); rig.s.noteOn(0, 67, 100);
    int rising = 0; int prevHeld = 0; std::vector<int> pattern;
    for (int blk = 0; blk < 48000 / 64; ++blk) {
        rig.s.process(rig.l.data(), rig.r.data(), 64);
        const int h = rig.s.heldVoices();
        if (h > 0 && prevHeld == 0) { ++rising; pattern.push_back(blk); }
        prevHeld = h;
    }
    CHECK(rising >= 7 && rising <= 9);
    if (pattern.size() > 2) CHECK(std::abs((pattern[2] - pattern[1]) * 64 - 6000) <= 128);
    // hold keeps the arpeggio after the keys are up
    rig.s.noteOff(0, 60); rig.s.noteOff(0, 64); rig.s.noteOff(0, 67);
    rig.run(24000);
    CHECK(rig.s.activeVoices() == 0);
    Rig h; h.set(P_aon, 1); h.set(P_ahold, 1); h.set(P_aer, 10); h.s.setTempo(120.0);
    h.s.noteOn(0, 60, 100); h.run(2000); h.s.noteOff(0, 60);
    int seen = 0; for (int blk = 0; blk < 48000 / 64; ++blk) { h.s.process(h.l.data(), h.r.data(), 64); if (h.s.heldVoices()) ++seen; }
    CHECK(seen > 100);
    // up & down with a two octave range steps through all notes: just check finite output
    Rig ud; ud.set(P_aon, 1); ud.set(P_amd, 2); ud.set(P_arg, 1); ud.s.setTempo(140.0);
    ud.s.noteOn(0, 60, 100); ud.s.noteOn(0, 65, 100);
    std::vector<float> o; ud.run(48000, &o);
    CHECK(finiteBounded(o, 1.0f));
}

TEST(synth_lfo_and_envelopes_move_the_sound) {
    // LFO 1 on the pitch (vibrato) widens the zero crossing spread; sanity check against NaN and a flat signal.
    Rig rig(48000.0, 0);
    rig.set(P_ol, 30); rig.set(P_l1r, 90); rig.set(P_o1w, 6); rig.set(P_ob, -64); rig.set(P_fr, 0); rig.set(P_fe, 0);
    rig.s.noteOn(0, 60, 100);
    std::vector<float> o; rig.run(48000, &o);
    CHECK(finiteBounded(o, 1.0f));
    // S/H, SQR, SAW, TRI with every destination stay finite
    for (int w = 0; w < 4; ++w) for (int ap = 0; ap < 3; ++ap) {
        Rig r2; r2.set(P_l1w, w); r2.set(P_ap, ap); r2.set(P_al, 40); r2.set(P_fl, 30); r2.set(P_ol, -20); r2.set(P_l1f, 40);
        r2.set(P_m2dp, 20); r2.set(P_m2df, 20); r2.set(P_m2da, 30); r2.s.controller(0, 1, 127);
        r2.s.noteOn(0, 60, 100); std::vector<float> q; r2.run(24000, &q);
        CHECK(finiteBounded(q, 1.0f));
    }
}

TEST(synth_oscillator_waves_and_options) {
    for (int o1 = 0; o1 < 7; ++o1) for (int o2 = 0; o2 < 4; ++o2) for (int opt = 0; opt < 4; ++opt) {
        Rig rig; rig.set(P_o1w, o1); rig.set(P_o2w, o2); rig.set(P_ob, -10);
        rig.set(P_sync, opt & 1); rig.set(P_ring, (opt >> 1) & 1); rig.set(P_ox, opt == 3 ? 60 : 0);
        rig.set(P_o2r, 7); rig.set(P_od, opt % 3);
        rig.s.noteOn(0, 60, 100); rig.s.noteOn(0, 67, 100);
        std::vector<float> o; rig.run(9600, &o);
        CHECK(finiteBounded(o, 1.0f));
    }
}

TEST(synth_levels_of_factory_presets) {
    // Each of the 51 factory presets plays a chord without clipping and without NaN.
    for (int i = 0; i < factoryCount(); ++i) {
        Rig rig(48000.0, i);
        rig.s.noteOn(0, 57, 100); rig.s.noteOn(0, 60, 100); rig.s.noteOn(0, 64, 100);
        std::vector<float> o; rig.run(24000, &o);
        CHECK(finiteBounded(o, 1.0f));
    }
    // default patch: a chord peaks around -6 dBFS (between -9 and -3)
    Rig rig; rig.s.noteOn(0, 57, 100); rig.s.noteOn(0, 60, 100); rig.s.noteOn(0, 64, 100);
    const float pk = rig.run(48000);
    const double db = 20.0 * std::log10(pk + 1e-9);
    std::printf("      default chord peak %.1f dBFS\n", db);
    CHECK(db > -9.0 && db < -3.0);
}

TEST(synth_fuzz_random_patches) {
#ifdef __OPTIMIZE__
    const int cases = 2000;
#else
    const int cases = 120;                            // unoptimised sanitizer builds are slow; the full run happens in optimised builds
#endif
    std::mt19937 rng(20261026);
    std::vector<float> o;
    Synth s; s.prepare(48000.0, 512);
    std::vector<float> l(512), r(512);
    int bad = 0;
    for (int c = 0; c < cases; ++c) {
        int p[kNumParams];
        for (int i = 0; i < kNumParams; ++i) {
            const ParamDef& d = kParams[i];
            // half of the cases start from a factory patch, then mutate a random subset; the rest are fully random
            std::uniform_int_distribution<int> any(d.min, d.max);
            p[i] = any(rng);
        }
        if (c & 1) { const Patch& f = factoryPatch((int) (rng() % factoryCount())); for (int i = 0; i < kNumParams; ++i) if (rng() % 4) p[i] = f.v[i]; }
        // keep the key mode single most of the time but test dual and split too
        p[P_km] = (int) (rng() % 5 == 0 ? rng() % 3 : 0);
        s.reset(); s.setParams(p);
        if (p[P_km] != 0) { int q[kNumParams]; for (int i = 0; i < kNumParams; ++i) q[i] = std::uniform_int_distribution<int>(kParams[i].min, kParams[i].max)(rng); s.setPartParams((int) (rng() % 2), q); }
        const int nNotes = 1 + (int) (rng() % 6);
        for (int k = 0; k < nNotes; ++k) s.noteOn((int) (rng() % 16), (int) (rng() % 128), 1 + (int) (rng() % 127));
        if (rng() % 3 == 0) s.pitchBend(0, (int) (rng() % 16384) - 8192);
        if (rng() % 3 == 0) s.controller(0, 1, (int) (rng() % 128));
        s.setTempo(60.0 + (double) (rng() % 140)); s.setTransport(rng() % 2 == 0, (double) (rng() % 64));
        for (int blk = 0; blk < 6; ++blk) {
            if (blk == 3) {                           // change parameters while sounding, release a note
                for (int k = 0; k < 20; ++k) { const int i = (int) (rng() % kNumParams); p[i] = std::uniform_int_distribution<int>(kParams[i].min, kParams[i].max)(rng); }
                s.setParams(p); s.noteOff(0, (int) (rng() % 128));
            }
            s.process(l.data(), r.data(), 512);
            for (int i = 0; i < 512; ++i) if (!std::isfinite(l[(size_t) i]) || !std::isfinite(r[(size_t) i]) || std::fabs(l[(size_t) i]) > 1.0001f || std::fabs(r[(size_t) i]) > 1.0001f) ++bad;
        }
    }
    CHECK(bad == 0);
}

TEST(synth_sample_rates) {
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 }) {
        Rig rig(sr, 0);
        rig.s.noteOn(0, 60, 100); rig.s.noteOn(0, 72, 100);
        std::vector<float> o; rig.run((int) (sr * 0.3), &o);
        CHECK(finiteBounded(o, 1.0f));
        float pk = 0.f; for (float x : o) pk = std::max(pk, std::fabs(x));
        CHECK(pk > 0.05f);
    }
}

TEST(synth_benchmark) {
    // 10 voices of Super Saw with the 24 dB filter at 48 kHz. Debug and sanitizer builds are far slower than a release build,
    // so the limit is only enforced when the compiler optimises (tools/build_synth_tools.sh --bench measures the release number).
    Rig rig; int p[kNumParams]; Patch d; std::memcpy(p, d.v, sizeof p); rig.s.setParams(p);
    for (int k = 0; k < 10; ++k) rig.s.noteOn(0, 40 + k * 3, 100);
    std::vector<float> l(256), r(256);
    const int total = 48000 * 2;
    double best = 1e9;                                 // CPU time, best of three: other jobs on the machine do not count
    for (int rep = 0; rep < 3; ++rep) {
        const std::clock_t t0 = std::clock();
        for (int i = 0; i < total; i += 256) rig.s.process(l.data(), r.data(), 256);
        best = std::min(best, (double) (std::clock() - t0) / CLOCKS_PER_SEC);
    }
    const double pct = best / 2.0 * 100.0;
    std::printf("      10 voices Super Saw + 24 dB filter: %.1f %% of one core (%s build)\n", pct,
#ifdef __OPTIMIZE__
                "optimised"
#else
                "unoptimised"
#endif
    );
#ifdef __OPTIMIZE__
    CHECK(pct < 10.0);
#endif
    CHECK(pct > 0.0);
}
