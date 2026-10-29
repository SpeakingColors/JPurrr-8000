// Headless snapshot tool: builds the real editor, selects each page or dialog and writes PNGs at design scale 1
// (2920 px wide). Compare them with tools/snap_mockup.js output. Usage: jpurrr_snapshot <outdir> [state...]
#include "PluginEditor.h"
#include "Editor/Fonts.h"
#include <juce_events/juce_events.h>
#include "jpurrr/params_gen.h"
#include <cstdlib>

static void writePng(const juce::Image& img, const juce::File& f) {
    f.deleteFile(); juce::FileOutputStream o(f); juce::PNGImageFormat png; png.writeImageToStream(img, o);
}


// Self test (no GUI): state round trip, user preset files, JSON export/import. Uses a temporary HOME.
static int selftest() {
    using namespace jpurrr;
    int fails = 0;
    auto check = [&](bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; };
    auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("jpurrr_selftest_home");
    tmp.deleteRecursively(); tmp.createDirectory();
    setenv("HOME", tmp.getFullPathName().toRawUTF8(), 1); setenv("XDG_CONFIG_HOME", tmp.getFullPathName().toRawUTF8(), 1);
    juce::ScopedJuceInitialiser_GUI init;
    std::vector<int> ids;                                          // a few host-visible, saved parameters
    for (int i = 0; i < kNumParams && ids.size() < 4; ++i) if (!(kParams[i].flags & kNoSave) && kParams[i].max > kParams[i].min + 1) ids.push_back(i);
    {
        JPurrr8000AudioProcessor a; a.prepareToPlay(44100.0, 512);
        for (int k = 0; k < (int) ids.size(); ++k) a.setInt(ids[(size_t) k], kParams[ids[(size_t) k]].max - k);
        a.presets.cur.name = "Round Trip"; a.presets.cur.bank = "E"; a.presets.cur.slot = 7; a.presets.cur.factory = false; a.presets.cur.dirty = true;
        a.uiWidth = 1777;
        juce::MemoryBlock mb; a.getStateInformation(mb);
        check(mb.getSize() > 0, "state written");
        JPurrr8000AudioProcessor b; b.prepareToPlay(44100.0, 512);
        b.setStateInformation(mb.getData(), (int) mb.getSize());
        bool same = true; for (int k = 0; k < (int) ids.size(); ++k) same &= b.getInt(ids[(size_t) k]) == kParams[ids[(size_t) k]].max - k;
        check(same, "state: parameter values restored");
        check(b.presets.cur.name == "Round Trip" && b.presets.cur.bank == "E" && b.presets.cur.slot == 7 && b.presets.cur.dirty, "state: preset name, bank, slot, dirty restored");
        check(b.uiWidth == 1777, "state: editor width restored");
        const char junk[] = "not a state"; JPurrr8000AudioProcessor c; c.setStateInformation(junk, (int) sizeof(junk));
        check(true, "state: garbage input does not crash");
    }
    PresetManager pm; pm.scanUser();
    const int n0 = pm.size();
    PresetEntry e; e.name = "Self Test"; e.type = "Pad"; e.bank = "F"; e.genres.add("Trance"); e.tags.add("warm");
    for (int i = 0; i < kNumParams; ++i) e.v[(size_t) i] = kParams[i].def;
    e.v[(size_t) ids[0]] = kParams[ids[0]].max;
    int idx = pm.saveUser(e, {});
    check(idx >= 0 && pm.size() == n0 + 1, "saveUser adds a sound");
    check(pm.findUserByName("Self Test", "F") >= 0, "findUserByName");
    { PresetManager p2; p2.scanUser(); check(p2.size() == n0 + 1 && p2.findUserByName("Self Test", "F") >= 0, "file survives a rescan"); }
    idx = pm.findUserByName("Self Test", "F");
    check(pm.renameUser(idx, "Renamed") && pm.findUserByName("Renamed", "F") >= 0, "rename");
    idx = pm.findUserByName("Renamed", "F");
    const auto text = PresetManager::toJson(pm.at(idx), false);
    PresetEntry back; juce::String err; int skipped = -1;
    check(PresetManager::fromJson(text, back, err, &skipped) && skipped == 0 && back.name == "Renamed" && back.v[(size_t) ids[0]] == kParams[ids[0]].max
          && back.type == "Pad", "export then import round trip (export format, no placement)");
    PresetEntry back2; check(PresetManager::fromJson(PresetManager::toJson(pm.at(idx), true), back2, err) && back2.genres.contains("Trance") && back2.tags.contains("warm") && back2.bank == "F", "file format keeps genres, tags, bank");
    check(!PresetManager::fromJson("{", back, err), "invalid JSON rejected");
    check(!PresetManager::fromJson("{\"app\":\"Other\",\"name\":\"x\",\"v\":{}}", back, err), "foreign app rejected");
    const juce::String idStr = juce::String(kParams[ids[0]].id);
    check(PresetManager::fromJson("{\"app\":\"JPurrr-8000\",\"name\":\"x\",\"v\":{\"" + idStr + "\":99999999,\"nope\":1}}", back, err, &skipped) && skipped == 1 && back.v[(size_t) ids[0]] == kParams[ids[0]].def,
          "out of range value skipped, unknown id ignored");
    check(pm.deleteUser(idx) && pm.size() == n0, "delete");
    check(!pm.deleteUser(0), "factory sound cannot be deleted");
    tmp.deleteRecursively();
    std::printf(fails ? "SELFTEST FAILED (%d)\n" : "SELFTEST OK\n", fails);
    return fails ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && juce::String(argv[1]) == "--selftest") return selftest();
    juce::ScopedJuceInitialiser_GUI init;
    juce::File out(argc > 1 ? argv[1] : "snap-plugin"); out.createDirectory();
    juce::StringArray states;
    for (int i = 2; i < argc; ++i) states.add(argv[i]);
    if (states.isEmpty()) states = { "sound", "chain", "master", "sound_nokbd", "chain_nokbd", "master_nokbd", "guide", "settings", "about", "save", "browser", "export", "import", "page1", "page2", "page4", "page6", "page7", "page8", "menu", "look_navy", "look_soda", "look_purr" };

    JPurrr8000AudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* editor = dynamic_cast<JPurrrEditor*>(ed.get());
    editor->setScaleWidth(2920);
    editor->content().displayEnv().animPhase = 0.0;        // freeze the oscillator scroll
    for (auto& s : states) {
        editor->content().applyState(s);
        editor->setScaleWidth(2920);
        editor->content().tick(0.0);
        auto img = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        writePng(img, out.getChildFile(s + ".png"));
        std::printf("%s %dx%d\n", s.toRawUTF8(), img.getWidth(), img.getHeight());
        editor->content().applyState("reset");
    }
    ed.reset();
    jp::fonts::clearCache();
    return 0;
}
