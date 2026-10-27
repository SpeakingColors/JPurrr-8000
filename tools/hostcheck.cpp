// Loads the BUILT .vst3 the way a host does (JUCE VST3 hosting), plays a chord, checks state save and load.
//   cmake --build <dir> --target jpurrr_hostcheck && <dir>/plugin/jpurrr_hostcheck path/to/JPurrr-8000.vst3
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstdio>
int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s\n", msg); ++fails; } else std::printf("ok:   %s\n", msg); } while (0)
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf("usage: jpurrr_hostcheck <plugin.vst3>\n"); return 2; }
    juce::AudioPluginFormatManager fm; fm.addFormat(new juce::VST3PluginFormat());
    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* f : fm.getFormats()) f->findAllTypesForFile(types, argv[1]);
    CHECK(types.size() == 1, "the bundle exposes one plugin");
    if (types.isEmpty()) return 1;
    const auto& d = *types[0];
    std::printf("name=%s manufacturer=%s instrument=%d inputs=%d outputs=%d\n", d.name.toRawUTF8(), d.manufacturerName.toRawUTF8(), (int) d.isInstrument, d.numInputChannels, d.numOutputChannels);
    CHECK(d.isInstrument, "reported as an instrument");
    juce::String err;
    auto make = [&] { return fm.createPluginInstance(d, 48000.0, 256, err); };
    auto p1 = make();
    CHECK(p1 != nullptr, "instance created");
    if (!p1) { std::printf("%s\n", err.toRawUTF8()); return 1; }
    p1->setPlayConfigDetails(0, 2, 48000.0, 256); p1->prepareToPlay(48000.0, 256);
    const int np = p1->getParameters().size();
    std::printf("host parameters: %d, latency %d samples, accepts midi %d\n", np, p1->getLatencySamples(), (int) p1->acceptsMidi());
    CHECK(np > 200, "parameters exposed to the host");
    CHECK(p1->acceptsMidi(), "accepts MIDI");
    CHECK(p1->hasEditor(), "has an editor");
    auto render = [](juce::AudioPluginInstance& p, int blocks) {
        juce::AudioBuffer<float> b(2, 256); juce::MidiBuffer m; double pk = 0; bool fin = true;
        for (int i = 0; i < blocks; ++i) {
            m.clear(); if (i == 0) { m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0); m.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0); }
            b.clear(); p.processBlock(b, m);
            for (int c = 0; c < 2; ++c) for (int s = 0; s < 256; ++s) { const float v = b.getSample(c, s); if (!std::isfinite(v)) fin = false; pk = std::max(pk, (double) std::fabs(v)); }
        }
        return std::pair<double, bool>(pk, fin);
    };
    auto r = render(*p1, 400);
    std::printf("peak after a chord: %.3f\n", r.first);
    CHECK(r.second, "output is finite");
    CHECK(r.first > 0.05 && r.first < 1.0, "the default sound is audible and below 0 dBFS");
    // change a parameter, save, load into a fresh instance
    juce::AudioProcessorParameter* prm = nullptr; for (auto* q : p1->getParameters()) if (q->getName(100).contains("[fc]")) { prm = q; break; }
    CHECK(prm != nullptr, "found a knob parameter to test with");
    if (!prm) return 1;
    const float before = prm->getValue(); prm->setValueNotifyingHost(before > 0.5f ? 0.25f : 0.75f);
    juce::MemoryBlock state; p1->getStateInformation(state);
    CHECK(state.getSize() > 100, "state saved");
    auto p2 = make(); p2->setPlayConfigDetails(0, 2, 48000.0, 256); p2->prepareToPlay(48000.0, 256);
    p2->setStateInformation(state.getData(), (int) state.getSize());
    int diff = 0; for (int i = 0; i < np; ++i) if (std::fabs(p1->getParameters()[i]->getValue() - p2->getParameters()[i]->getValue()) > 0.009f) { if (++diff <= 5) std::printf("  differs: #%d %s  %.4f vs %.4f\n", i, p1->getParameters()[i]->getName(40).toRawUTF8(), p1->getParameters()[i]->getValue(), p2->getParameters()[i]->getValue()); }
    CHECK(diff == 0, "state round trip restores every parameter");
    p1->releaseResources(); p2->releaseResources(); p1.reset(); p2.reset();
    std::printf(fails ? "%d checks failed\n" : "all checks passed\n", fails);
    return fails ? 1 : 0;
}
