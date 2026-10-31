// Headless check of the sound engine options (gear menu): settings.json round trip and fallback, the processor hands the options to the synth.
// Not built by default:  cmake --build <dir> --target jpurrr_enginecheck && <dir>/plugin/jpurrr_enginecheck
// It uses a temporary HOME, so the real settings.json is never touched.
#include "PluginProcessor.h"
#include "Editor/Controller.h"
#include "jpurrr/patch.h"
#include <juce_events/juce_events.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
using namespace jpurrr;

static int fails = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; }

static juce::File settingsPath() { return PresetManager::presetsDir().getParentDirectory().getChildFile("settings.json"); }
static void writeSettings(const char* text) { auto f = settingsPath(); f.getParentDirectory().createDirectory(); f.replaceWithText(text); }
static bool same(const EngineOptions& o, int s, int a, int os) { return o.style == s && o.antiAlias == a && o.oversampling == os; }

static std::vector<float> render(JPurrr8000AudioProcessor& p, int blocks) {
    juce::AudioBuffer<float> buf(2, 256); juce::MidiBuffer midi; std::vector<float> out;
    for (int b = 0; b < blocks; ++b) { buf.clear(); midi.clear(); p.processBlock(buf, midi); out.insert(out.end(), buf.getReadPointer(0), buf.getReadPointer(0) + 256); }
    return out;
}

int main() {
    auto home = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("jpurrr_enginecheck_home");
    home.deleteRecursively(); home.createDirectory(); setenv("HOME", home.getFullPathName().toRawUTF8(), 1);
    setenv("XDG_CONFIG_HOME", home.getChildFile(".config").getFullPathName().toRawUTF8(), 1);
    juce::ScopedJuceInitialiser_GUI init;
    check(settingsPath().getFullPathName().startsWith(home.getFullPathName()), "settings.json lives in the temporary HOME");

    // --- pack / unpack: every combination survives, the default word is the old behaviour
    bool all = true;
    for (int s = 0; s < 2; ++s) for (int a = 0; a < 2; ++a) for (int os : { 0, 1, 2, 4 }) all = all && same(JPurrr8000AudioProcessor::unpackEngine(JPurrr8000AudioProcessor::packEngine(s, a, os)), s, a, os);
    check(all, "pack/unpack round trip for all 16 combinations");
    check(JPurrr8000AudioProcessor::packEngine(0, 0, 0) == 0, "Classic / Standard / Auto packs to 0");

    // --- settings: fallback to defaults
    jp::Settings d; check(d.engineStyle == 0 && d.engineAa == 0 && d.engineOs == 0, "defaults are Classic / Standard / Auto");
    { jp::Settings s; s.load(); check(s.engineStyle == 0 && s.engineAa == 0 && s.engineOs == 0, "no settings.json: defaults"); }
    writeSettings("{\"engineStyle\": 7, \"engineAa\": -1, \"engineOs\": 3}");
    { jp::Settings s; s.load(); check(s.engineStyle == 0 && s.engineAa == 0 && s.engineOs == 0, "bad values (7, -1, 3) fall back to defaults"); }
    writeSettings("{\"engineStyle\": \"x\", \"engineOs\": 8}");
    { jp::Settings s; s.load(); check(s.engineStyle == 0 && s.engineAa == 0 && s.engineOs == 0, "wrong type and 8x fall back to defaults"); }
    writeSettings("{\"bubbles\": true, \"look\": \"navy\"}");
    { jp::Settings s; s.load(); check(s.engineOs == 0 && s.look == "navy", "old settings.json without engine keys: defaults, rest kept"); }

    // --- settings: round trip through save()
    for (int os : { 0, 1, 2, 4 }) {
        jp::Settings w; w.engineStyle = 1; w.engineAa = 1; w.engineOs = os; w.look = "soda"; w.save();
        jp::Settings r; r.load();
        check(r.engineStyle == 1 && r.engineAa == 1 && r.engineOs == os && r.look == "soda", os == 0 ? "round trip Clean / High / Auto" : os == 1 ? "round trip Clean / High / 1x" : os == 2 ? "round trip Clean / High / 2x" : "round trip Clean / High / 4x");
    }

    // --- the processor reads settings.json when the first instance is made
    writeSettings("{\"engineStyle\": 1, \"engineAa\": 1, \"engineOs\": 4}");
    JPurrr8000AudioProcessor proc; proc.setPlayConfigDetails(0, 2, 48000, 256); proc.prepareToPlay(48000, 256);
    check(same(proc.appliedEngineOptions(), 1, 1, 4), "first instance starts with the saved options (Clean / High / 4x)");

    // --- Done button: setEngineWord, the audio thread applies it at the start of the next block
    proc.setAll(factoryPatch(0).v);
    JPurrr8000AudioProcessor::setEngineWord(0, 0, 0);
    check(same(proc.appliedEngineOptions(), 1, 1, 4), "nothing changes before the next block");
    render(proc, 1);
    check(same(proc.appliedEngineOptions(), 0, 0, 0), "after processBlock: Classic / Standard / Auto");
    JPurrr8000AudioProcessor::setEngineWord(1, 0, 2); render(proc, 1);
    check(same(proc.appliedEngineOptions(), 1, 0, 2), "after processBlock: Clean / Standard / 2x");
    JPurrr8000AudioProcessor::setEngineWord(1, 1, 1); render(proc, 1);
    check(same(proc.appliedEngineOptions(), 1, 1, 1), "after processBlock: Clean / High / 1x");

    // --- plugin wide: a second instance follows the same word
    JPurrr8000AudioProcessor second; second.setPlayConfigDetails(0, 2, 48000, 256); second.prepareToPlay(48000, 256);
    check(same(second.appliedEngineOptions(), 1, 1, 1), "a second instance starts with the current word");
    JPurrr8000AudioProcessor::setEngineWord(0, 1, 4); render(proc, 1); render(second, 1);
    check(same(proc.appliedEngineOptions(), 0, 1, 4) && same(second.appliedEngineOptions(), 0, 1, 4), "both instances follow a change");

    // --- the options reach the sound: Super Saw patch, Classic and Clean must differ, and both stay finite and audible
    auto note = [&](int style) {
        JPurrr8000AudioProcessor p; p.setPlayConfigDetails(0, 2, 44100, 256); p.prepareToPlay(44100, 256); p.setAll(factoryPatch(0).v);
        JPurrr8000AudioProcessor::setEngineWord(style, 0, 1); p.setInt(P_pch, 5); p.stripNoteOn(96); render(p, 1); return render(p, 40);
    };
    auto a = note(0), b = note(1); double diff = 0, peakA = 0, peakB = 0; bool finite = true;
    for (size_t i = 0; i < a.size(); ++i) { diff += std::fabs(a[i] - b[i]); peakA = std::max(peakA, (double) std::fabs(a[i])); peakB = std::max(peakB, (double) std::fabs(b[i])); finite = finite && std::isfinite(a[i]) && std::isfinite(b[i]); }
    check(finite && peakA > 0.01 && peakB > 0.01, "both styles make finite, audible sound");
    check(diff > 1.0, "Classic and Clean really sound different (the options reach the synth)");

    home.deleteRecursively();
    std::printf(fails ? "\n%d FAILED\n" : "\nall ok\n", fails);
    return fails ? 1 : 0;
}
