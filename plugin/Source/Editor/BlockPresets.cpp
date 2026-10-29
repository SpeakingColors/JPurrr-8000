#include "BlockPresets.h"
#include "Widgets.h"
#include "../BlockPresetFile.h"
using namespace jp;
using namespace jpurrr;

namespace {
BlockPresetStore& store() { return BlockPresetFile::get().store(); }
int curVal(Controller& c, const std::string& id) { return c.get(Controller::idx(id.c_str())); }
bool factoryMatches(Controller& c, int mp, int p) {
    const layout::MpDef& d = layout::kMp[mp];
    for (int k = 0; k < d.nIds; ++k) if (c.get(Controller::idx(d.ids[k])) != d.list[p].vals[k]) return false;
    return true;
}
bool userMatches(Controller& c, int mp, const BlockPresetEntry& e) {
    return blockPresetMatches(e, mpIds(mp), [&](const std::string& id) { return curVal(c, id); });
}
std::string key(int mp) { return layout::kMp[mp].key; }
juce::String title(int mp) { return juce::String::fromUTF8(layout::kMp[mp].title); }
MpDone persist(Controller& c) {
    if (BlockPresetFile::get().save()) return MpDone::Ok;
    c.say("Could not write the preset file");
    return MpDone::Failed;
}
}

int jp::mpIndexOf(const char* k) { for (int m = 0; m < layout::kNumMp; ++m) if (juce::String(layout::kMp[m].key) == k) return m; return -1; }
std::vector<std::string> jp::mpIds(int mp) { std::vector<std::string> v; for (int k = 0; k < layout::kMp[mp].nIds; ++k) v.push_back(layout::kMp[mp].ids[k]); return v; }
std::vector<std::string> jp::mpSaveIds(int mp) { auto v = mpIds(mp); if (key(mp) == "gate") v.push_back("gpat"); return v; }
std::vector<std::string> jp::mpFactoryNames(int mp) { std::vector<std::string> v; for (int k = 0; k < layout::kMp[mp].nList; ++k) v.push_back(layout::kMp[mp].list[k].name); return v; }
const std::vector<BlockPresetEntry>& jp::mpUserList(int mp) { return store().list(key(mp)); }

MpChoice jp::mpCurrent(Controller& c, int mp) {
    const layout::MpDef& d = layout::kMp[mp];
    const auto& users = mpUserList(mp);
    const juce::String pick = c.mpPick.count(mp) ? c.mpPick[mp] : juce::String();
    if (pick.startsWith("f:")) { const int p = pick.substring(2).getIntValue(); if (p >= 0 && p < d.nList && factoryMatches(c, mp, p)) return { p, -1 }; }
    if (pick.startsWith("u:")) { const int u = mpUserIndex(mp, pick.substring(2)); if (u >= 0 && userMatches(c, mp, users[(size_t) u])) return { -1, u }; }
    for (int p = 0; p < d.nList; ++p) if (factoryMatches(c, mp, p)) return { p, -1 };
    for (size_t u = 0; u < users.size(); ++u) if (userMatches(c, mp, users[u])) return { -1, (int) u };
    return {};
}

void jp::mpApplyUser(Controller& c, int mp, int ui) {
    const auto& users = mpUserList(mp);
    if (ui < 0 || ui >= (int) users.size()) return;
    const BlockPresetEntry e = users[(size_t) ui];
    const auto ids = mpIds(mp); const auto vals = blockPresetValues(e, ids);
    int gpt = -1;
    for (size_t k = 0; k < ids.size(); ++k) { if (ids[k] == "gpt") { gpt = (int) k; continue; } c.user(Controller::idx(ids[k].c_str()), vals[k]); }
    if (gpt >= 0) c.user(Controller::idx("gpt"), vals[(size_t) gpt]);
    if (key(mp) == "gate" && gpt >= 0 && vals[(size_t) gpt] == 0) c.quiet(Controller::idx("gpat"), blockPresetValues(e, { "gpat" })[0]);   // own 16 step pattern
    c.mpPick[mp] = "u:" + juce::String(e.name);
    c.say(title(mp) + ": " + juce::String::fromUTF8(e.name.c_str()));
}

bool jp::mpFactoryNameTaken(int mp, const juce::String& name) { return blockNameTaken(mpFactoryNames(mp), name.toStdString()); }
juce::String jp::mpCleanName(const juce::String& typed) {
    const auto s = BlockPresetStore::cleanName(typed.toStdString());
    return s.empty() ? juce::String("Untitled") : juce::String::fromUTF8(s.c_str());
}
int jp::mpUserIndex(int mp, const juce::String& name) { return store().find(key(mp), name.toStdString()); }

MpDone jp::mpStore(Controller& c, int mp, const juce::String& name, bool overwrite) {
    BlockPresetFile::get().refreshIfChanged();
    std::vector<std::pair<std::string, int>> vals;
    for (const auto& id : mpSaveIds(mp)) vals.push_back({ id, curVal(c, id) });
    const auto r = store().save(key(mp), name.toStdString(), vals, overwrite);
    if (r == BlockPresetStore::Result::Full) { c.say("Your list for this block is full"); return MpDone::Full; }
    if (r != BlockPresetStore::Result::Ok) return MpDone::Failed;
    c.mpPick[mp] = "u:" + juce::String(BlockPresetStore::cleanName(name.toStdString()));
    return persist(c);
}

MpDone jp::mpStoreAsNew(Controller& c, int mp, const juce::String& base, juce::String& savedAs) {
    BlockPresetFile::get().refreshIfChanged();
    const std::string n = store().freeName(key(mp), base.toStdString(), mpFactoryNames(mp));
    savedAs = juce::String::fromUTF8(n.c_str());
    return mpStore(c, mp, savedAs, false);
}

bool jp::mpRename(Controller& c, int mp, const juce::String& oldName, const juce::String& newName, juce::String& hint) {
    BlockPresetFile::get().refreshIfChanged();
    const juce::String nn = juce::String::fromUTF8(BlockPresetStore::cleanName(newName.toStdString()).c_str());
    if (mpFactoryNameTaken(mp, nn)) { hint = "Factory presets cannot be changed. Pick another name."; return false; }
    const auto r = store().rename(key(mp), oldName.toStdString(), nn.toStdString());
    if (r == BlockPresetStore::Result::Exists) { hint = "That name is already used. Pick another one."; return false; }
    if (r != BlockPresetStore::Result::Ok) return false;
    c.mpPick[mp] = "u:" + nn;
    return persist(c) == MpDone::Ok;
}

bool jp::mpDelete(Controller& c, int mp, const juce::String& name) {
    BlockPresetFile::get().refreshIfChanged();
    if (store().remove(key(mp), name.toStdString()) != BlockPresetStore::Result::Ok) return false;
    c.mpPick.erase(mp);
    return persist(c) == MpDone::Ok;
}

juce::String jp::mpOwnSelected(Controller& c, int mp) {
    const MpChoice ch = mpCurrent(c, mp);
    if (ch.user >= 0) return juce::String::fromUTF8(mpUserList(mp)[(size_t) ch.user].name.c_str());
    c.say(ch.custom() ? "Pick one of your own presets first" : "Factory presets cannot be changed");
    return {};
}
