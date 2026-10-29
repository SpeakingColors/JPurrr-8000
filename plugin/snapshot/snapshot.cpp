// Headless snapshot tool: builds the real editor, selects each page or dialog and writes PNGs at design scale 1
// (2920 px wide). Compare them with tools/snap_mockup.js output. Usage: jpurrr_snapshot <outdir> [state...]
#include "PluginEditor.h"
#include "Editor/Fonts.h"
#include "BlockPresetFile.h"
#include "Editor/BlockPresets.h"
#include "Editor/Widgets.h"
#include <juce_events/juce_events.h>
#include "jpurrr/params_gen.h"
#include <cstdlib>

// The popup of a block preset select drawn with the plugin's own menu look (a real popup needs a native window, which a headless run has not).
static void overlayPopup(juce::Image& img, JPurrrEditor& editor, int mp) {
    using namespace jp;
    const auto& look = lookByIndex(0);
    auto& lf = popupLnF(look);
    struct Item { juce::String text; bool header, ticked, active; };
    std::vector<Item> items;
    auto& c = editor.content().controller();
    const MpChoice ch = mpCurrent(c, mp);
    for (int k = 0; k < layout::kMp[mp].nList; ++k) items.push_back({ juce::String::fromUTF8(layout::kMp[mp].list[k].name), false, k == ch.factory, true });
    if (!mpUserList(mp).empty()) items.push_back({ "YOUR PRESETS", true, false, false });
    for (size_t u = 0; u < mpUserList(mp).size(); ++u) items.push_back({ juce::String::fromUTF8(mpUserList(mp)[u].name.c_str()), false, (int) u == ch.user, true });
    items.push_back({ "Custom", false, ch.custom(), false });
    juce::Rectangle<float> sel;
    for (auto& s2 : layout::kPages[layout::kChain].selects) if (juce::String(s2.mp) == layout::kMp[mp].key) sel = { s2.r.x, s2.r.y, s2.r.w, s2.r.h };
    int w = (int) sel.getWidth(), h = 0;
    for (auto& it : items) { int iw = 0, ih = 0; if (it.header) lf.getIdealPopupMenuSectionHeaderSizeWithOptions(it.text, 26, iw, ih, juce::PopupMenu::Options()); else lf.getIdealPopupMenuItemSize(it.text, false, 26, iw, ih); w = std::max(w, iw); h += ih; }
    const int b = lf.getPopupMenuBorderSize();
    juce::Graphics g(img);
    g.setOrigin((int) sel.getX() + 262 * 0 + (int) (layout::kCheekL.w * 0), (int) sel.getBottom());
    lf.drawPopupMenuBackground(g, w + 2 * b, h + 2 * b);
    int y = b;
    for (auto& it : items) {
        int iw = 0, ih = 0; if (it.header) lf.getIdealPopupMenuSectionHeaderSizeWithOptions(it.text, 26, iw, ih, juce::PopupMenu::Options()); else lf.getIdealPopupMenuItemSize(it.text, false, 26, iw, ih);
        juce::Rectangle<int> area(b, y, w, ih);
        if (it.header) lf.drawPopupMenuSectionHeader(g, area, it.text);
        else lf.drawPopupMenuItem(g, area, false, it.active, false, it.ticked, false, it.text, {}, nullptr, nullptr);
        y += ih;
    }
}

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
    // block presets file: missing file, round trip, unreadable file kept as .bad, atomic write leaves no temp file
    {
        auto f = tmp.getChildFile("bp").getChildFile("block_presets.json");
        { BlockPresetFile a(f); check(a.store().empty() && !a.loadedFromBadFile(), "block presets: missing file gives an empty list");
          check(a.store().save("drive", "Warm", { { "qdv", 30 } }, false) == BlockPresetStore::Result::Ok && a.save() && f.existsAsFile(), "block presets: save writes the file");
          check(tmp.getChildFile("bp").findChildFiles(juce::File::findFiles, false, "*").size() == 1, "block presets: no temp file left behind"); }
        { BlockPresetFile b(f); check(b.store().find("drive", "warm") == 0 && b.store().list("drive")[0].vals[0].second == 30, "block presets: reloads after a restart"); }
        f.replaceWithText("{ this is not json");
        { BlockPresetFile c(f); check(c.store().empty() && c.loadedFromBadFile() && f.withFileExtension("json.bad").existsAsFile() && !f.existsAsFile(), "block presets: corrupt file kept as .bad, empty list, no crash");
          check(c.store().save("low", "X", { { "qhp", 1 } }, false) == BlockPresetStore::Result::Ok && c.save(), "block presets: can save again after a bad file"); }
        f.replaceWithText("");
        { BlockPresetFile d(f); check(d.store().empty(), "block presets: empty file does not crash"); }
        f.replaceWithText("{\"app\":\"JPurrr-8000\",\"format\":1,\"blocks\":{\"gate\":[{\"n\":\"Late\",\"v\":{\"gpt\":2}}]}}");
        { BlockPresetFile e2(f); check(e2.store().find("gate", "late") == 0, "block presets: a hand written file loads");
          BlockPresetFile other(f); other.store().save("gate", "Other", { { "gpt", 3 } }, false); juce::Thread::sleep(1100); other.save();
          e2.refreshIfChanged(); check(e2.store().find("gate", "other") == 1, "block presets: a change by another window is picked up"); }
    }
    // block presets through the controller: save, match, pick, rename, delete, trance gate pattern
    {
        JPurrr8000AudioProcessor proc; proc.prepareToPlay(44100.0, 512);
        jp::Controller ctl(proc);
        const int mp = jp::mpIndexOf("drive"), gate = jp::mpIndexOf("gate");
        ctl.user(jp::Controller::idx("qdv"), 33); ctl.user(jp::Controller::idx("qdx"), 44);
        check(jp::mpCurrent(ctl, mp).custom(), "block presets: odd knob values show Custom");
        check(jp::mpStore(ctl, mp, "Mine", false) == jp::MpDone::Ok && jp::mpCurrent(ctl, mp).user == 0, "block presets: saved preset is shown by name");
        check(jp::mpStore(ctl, mp, "mine", false) == jp::MpDone::Failed, "block presets: same name is not overwritten without asking");
        ctl.user(jp::Controller::idx("qdv"), 50);
        check(jp::mpCurrent(ctl, mp).custom(), "block presets: moving a knob gives Custom again");
        jp::mpApplyUser(ctl, mp, 0);
        check(ctl.get(jp::Controller::idx("qdv")) == 33 && ctl.get(jp::Controller::idx("qdx")) == 44 && jp::mpCurrent(ctl, mp).user == 0, "block presets: applying sets the knobs");
        juce::String why;
        check(!jp::mpRename(ctl, mp, "Mine", "Default", why) && why.isNotEmpty(), "block presets: factory name refused on rename");
        check(jp::mpRename(ctl, mp, "Mine", "Yours", why) && jp::mpCurrent(ctl, mp).user == 0 && jp::mpUserList(mp)[0].name == "Yours", "block presets: rename");
        juce::String as; check(jp::mpStoreAsNew(ctl, mp, "Yours", as) == jp::MpDone::Ok && as == "Yours 2", "block presets: save as new gives Name 2");
        check(jp::mpFactoryNameTaken(mp, "warm PUSH"), "block presets: factory names found without case");
        check(jp::mpDelete(ctl, mp, "Yours 2") && jp::mpUserList(mp).size() == 1, "block presets: delete");
        { BlockPresetFile again(BlockPresetFile::defaultFile()); check(again.store().find("drive", "yours") == 0 && again.store().list("drive").size() == 1, "block presets: changes are on disk"); }
        // trance gate with an own pattern
        ctl.user(jp::Controller::idx("gpt"), 0); ctl.fromDisplay(jp::Controller::idx("gpat"), 0x1234);
        check(jp::mpStore(ctl, gate, "Mine gate", false) == jp::MpDone::Ok && jp::mpCurrent(ctl, gate).user == 0, "block presets: gate preset saved");
        ctl.fromDisplay(jp::Controller::idx("gpat"), 0x4321);
        check(jp::mpCurrent(ctl, gate).custom(), "block presets: another pattern is Custom");
        jp::mpApplyUser(ctl, gate, 0);
        check(ctl.get(jp::Controller::idx("gpat")) == 0x1234 && ctl.get(jp::Controller::idx("gpt")) == 0, "block presets: gate pattern comes back");
    }
    tmp.deleteRecursively();
    std::printf(fails ? "SELFTEST FAILED (%d)\n" : "SELFTEST OK\n", fails);
    return fails ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && juce::String(argv[1]) == "--selftest") return selftest();
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray wanted; for (int i = 2; i < argc; ++i) wanted.add(argv[i]);
    bool bp = false; for (auto& w : wanted) bp |= w.startsWith("bp_");
    if (bp) {                                                           // block preset states: own empty user data folder, two presets of the drive stack
        auto home = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("jpurrr_snap_home");
        home.deleteRecursively(); home.createDirectory();
        setenv("HOME", home.getFullPathName().toRawUTF8(), 1); setenv("XDG_CONFIG_HOME", home.getFullPathName().toRawUTF8(), 1);
        auto& st = BlockPresetFile::get().store();
        st.save("drive", "Warm Drive", { { "qdv", 30 }, { "qdx", 40 }, { "qdf", 10 }, { "qdm", 60 } }, false);
        st.save("drive", "Crunchy", { { "qdv", 55 }, { "qdx", 60 }, { "qdf", 20 }, { "qdm", 80 } }, false);
    }
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
        if (s == "bp_popup") overlayPopup(img, *editor, jp::mpIndexOf("drive"));
        writePng(img, out.getChildFile(s + ".png"));
        std::printf("%s %dx%d\n", s.toRawUTF8(), img.getWidth(), img.getHeight());
        editor->content().applyState("reset");
    }
    ed.reset();
    jp::fonts::clearCache();
    return 0;
}
