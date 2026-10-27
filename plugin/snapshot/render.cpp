// Offline test of the real plugin processor: plays a chord through every factory sound (or one), checks level and
// finite output, writes WAVs. Not built by default:
//   cmake --build <dir> --target jpurrr_render && <dir>/plugin/jpurrr_render <outdir> [sr] [preset index]
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "jpurrr/patch.h"
#include <cmath>
#include <cstdio>
using namespace jpurrr;

static void writeWav(const juce::File& f, const std::vector<float>& l, const std::vector<float>& r, int sr) {
    juce::WavAudioFormat wav; f.deleteFile();
    std::unique_ptr<juce::FileOutputStream> out(f.createOutputStream());
    if (!out) return;
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(out.get(), sr, 2, 24, {}, 0));
    if (!w) return; out.release();
    const float* ch[2] = { l.data(), r.data() }; w->writeFromFloatArrays(ch, 2, (int) l.size());
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir(argc > 1 ? argv[1] : "render_out"); outDir.createDirectory();
    const int sr = argc > 2 ? std::atoi(argv[2]) : 48000;
    const int only = argc > 3 ? std::atoi(argv[3]) : -1;
    const int block = 256;
    int bad = 0;
    for (int pi = 0; pi < factoryCount(); ++pi) {
        if (only >= 0 && pi != only) continue;
        JPurrr8000AudioProcessor proc;
        proc.setPlayConfigDetails(0, 2, sr, block);
        proc.prepareToPlay(sr, block);
        proc.setAll(factoryPatch(pi).v);
        const int total = sr * 5;                       // 3 s held chord, 2 s release
        std::vector<float> L((size_t) total), R((size_t) total);
        juce::AudioBuffer<float> buf(2, block); juce::MidiBuffer midi;
        const int notes[3] = { 57, 60, 64 };
        for (int pos = 0; pos < total; pos += block) {
            midi.clear();
            if (pos == 0) for (int n : notes) midi.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 0);
            if (pos <= sr * 3 && pos + block > sr * 3) for (int n : notes) midi.addEvent(juce::MidiMessage::noteOff(1, n), 0);
            buf.clear(); proc.processBlock(buf, midi);
            const int len = juce::jmin(block, total - pos);
            for (int i = 0; i < len; ++i) { L[(size_t) (pos + i)] = buf.getSample(0, i); R[(size_t) (pos + i)] = buf.getSample(1, i); }
        }
        double pk = 0, e0 = 0, e1 = 0; bool finite = true;
        for (int i = 0; i < total; ++i) {
            const float a = L[(size_t) i], b = R[(size_t) i];
            if (!std::isfinite(a) || !std::isfinite(b)) finite = false;
            pk = std::max(pk, (double) std::max(std::fabs(a), std::fabs(b)));
            if (i < sr * 3) e0 += a * a + b * b; else e1 += a * a + b * b;
        }
        const double rms0 = std::sqrt(e0 / (2.0 * sr * 3)), rms1 = std::sqrt(e1 / (2.0 * sr * 2));
        const bool silent = rms0 < 1e-4, hot = pk > 1.0;
        std::printf("%2d %-24s peak %6.2f dBFS  rms %6.1f dB  tail %6.1f dB  %s%s%s\n", pi, factoryPatch(pi).name.c_str(),
            20 * std::log10(pk + 1e-9), 20 * std::log10(rms0 + 1e-9), 20 * std::log10(rms1 + 1e-9),
            finite ? "" : "NOT FINITE ", silent ? "SILENT " : "", hot ? "OVER " : "");
        if (!finite || silent || hot) ++bad;
        writeWav(outDir.getChildFile(juce::String::formatted("%02d_", pi) + juce::String(factoryPatch(pi).name).replaceCharacters(" /", "__") + ".wav"), L, R, sr);
    }
    std::printf("%d problem sounds\n", bad);
    return bad ? 1 : 0;
}
