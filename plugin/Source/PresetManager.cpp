#include "PresetManager.h"
#include "jpurrr/patch.h"
#include "Editor/Layout.h"
using namespace jpurrr;

namespace {
bool isNoSave(int i) { return (kParams[i].flags & kNoSave) != 0; }
juce::String sanitizeFile(const juce::String& s) {
    auto r = s.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    return r.isEmpty() ? juce::String("preset") : r.substring(0, 40);
}
}

PresetManager::PresetManager() {
    const int n = factoryCount();
    for (int i = 0; i < n; ++i) {
        const Patch& p = factoryPatch(i);
        PresetEntry e; e.name = p.name; e.type = p.type; e.factory = true; e.bank = "A"; e.slot = i + 1;
        for (auto& g : p.genres) e.genres.add(g);
        for (int k = 0; k < kNumParams; ++k) e.v[(size_t) k] = p.v[k];
        list.push_back(e);
    }
    nFactory = n;
    for (int k = 0; k < jp::layout::kNumStartSlots; ++k) { const int s = jp::layout::kStartSlots[k] - 1; if (s >= 0 && s < n) list[(size_t) s].start = true; }   // the START HERE list of the mockup
    cur.name = list.empty() ? "Init" : list[0].name; cur.type = list.empty() ? "Lead" : list[0].type;
    scanUser();
}

juce::File PresetManager::presetsDir() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Studio 87").getChildFile("JPurrr-8000").getChildFile("presets");
}

std::array<int, kNumParams> PresetManager::defaults() const {
    std::array<int, kNumParams> a {}; for (int i = 0; i < kNumParams; ++i) a[(size_t) i] = kParams[i].def; return a;
}

void PresetManager::sortUser() {
    std::stable_sort(list.begin() + nFactory, list.end(), [](const PresetEntry& a, const PresetEntry& b) {
        return a.bank == b.bank ? a.slot < b.slot : a.bank < b.bank; });
}

void PresetManager::scanUser() {
    list.resize((size_t) nFactory);
    auto dir = presetsDir();
    if (!dir.isDirectory()) return;
    for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.json")) {
        PresetEntry e; juce::String err;
        if (!fromJson(f.loadFileAsString(), e, err)) continue;
        e.factory = false; e.uid = f.getFileNameWithoutExtension();
        if (e.bank.isEmpty() || e.bank[0] < 'E' || e.bank[0] > 'G') e.bank = "F";
        if (e.slot < 1 || e.slot > 128 || find(e.bank, e.slot) >= 0) { e.slot = nextFreeSlot(e.bank); if (e.slot == 0) continue; }
        list.push_back(e);
    }
    sortUser();
}

int PresetManager::find(const juce::String& bank, int slot) const {
    for (int i = 0; i < size(); ++i) if (list[(size_t) i].bank == bank && list[(size_t) i].slot == slot) return i;
    return -1;
}
int PresetManager::indexOfCurrent() const {
    for (int i = 0; i < size(); ++i) { auto& p = list[(size_t) i]; if (p.bank == cur.bank && p.slot == cur.slot && p.name == cur.name) return i; }
    return find(cur.bank, cur.slot);
}
int PresetManager::nextFreeSlot(const juce::String& bank) const {
    for (int s = 1; s <= 128; ++s) if (find(bank, s) < 0) return s;
    return 0;
}
int PresetManager::findUserByName(const juce::String& name, const juce::String& bank) const {
    for (int i = nFactory; i < size(); ++i) if (list[(size_t) i].name == name && list[(size_t) i].bank == bank) return i;
    return -1;
}

int PresetManager::saveUser(const PresetEntry& in, const juce::String& overwriteUid) {
    PresetEntry e = in; e.factory = false;
    int idx = -1;
    if (overwriteUid.isNotEmpty()) for (int i = nFactory; i < size(); ++i) if (list[(size_t) i].uid == overwriteUid) idx = i;
    auto dir = presetsDir(); dir.createDirectory();
    if (idx >= 0) { e.bank = list[(size_t) idx].bank; e.slot = list[(size_t) idx].slot; e.uid = overwriteUid; list[(size_t) idx] = e; }
    else {
        if (e.slot < 1 || find(e.bank, e.slot) >= 0) e.slot = nextFreeSlot(e.bank);
        if (e.slot == 0) return -1;
        auto base = sanitizeFile(e.bank + juce::String(e.slot).paddedLeft('0', 3) + " " + e.name);
        juce::String uid = base; int k = 2;
        while (dir.getChildFile(uid + ".json").exists()) uid = base + " " + juce::String(k++);
        e.uid = uid; list.push_back(e);
    }
    dir.getChildFile(e.uid + ".json").replaceWithText(toJson(e, true));
    sortUser();
    return find(e.bank, e.slot);
}

bool PresetManager::renameUser(int index, const juce::String& newName) {
    if (index < nFactory || index >= size()) return false;
    auto& e = list[(size_t) index]; e.name = newName;
    presetsDir().getChildFile(e.uid + ".json").replaceWithText(toJson(e, true));
    return true;
}
bool PresetManager::deleteUser(int index) {
    if (index < nFactory || index >= size()) return false;
    presetsDir().getChildFile(list[(size_t) index].uid + ".json").deleteFile();
    list.erase(list.begin() + index);
    return true;
}

void PresetManager::pushHistory(const PresetEntry& e) {
    const juce::String h = e.bank + juce::String(e.slot) + " " + e.name;
    history.removeString(h); history.insert(0, h);
    while (history.size() > 12) history.remove(history.size() - 1);
}

juce::String PresetManager::toJson(const PresetEntry& e, bool withPlacement) {
    auto* o = new juce::DynamicObject();
    o->setProperty("app", "JPurrr-8000"); o->setProperty("format", 1);
    o->setProperty("name", e.name); o->setProperty("type", e.type);
    if (withPlacement) {
        juce::Array<juce::var> g, t; for (auto& s : e.genres) g.add(s); for (auto& s : e.tags) t.add(s);
        o->setProperty("genres", g); o->setProperty("tags", t); o->setProperty("bank", e.bank); o->setProperty("slot", e.slot);
    }
    auto* v = new juce::DynamicObject();
    for (int i = 0; i < kNumParams; ++i) if (!isNoSave(i) && e.v[(size_t) i] != kParams[i].def) v->setProperty(kParams[i].id, e.v[(size_t) i]);
    o->setProperty("v", juce::var(v));
    return juce::JSON::toString(juce::var(o), false);
}

bool PresetManager::fromJson(const juce::String& text, PresetEntry& out, juce::String& err, int* skipped) {
    juce::var j;
    if (juce::JSON::parse(text, j).failed() || !j.isObject()) { err = "This is not a JPurrr-8000 preset."; return false; }
    const auto app = j.getProperty("app", {}).toString();
    if ((app != "JPurrr-8000" && app != "JPurr-8000") || !j.getProperty("name", {}).isString() || !j.getProperty("v", {}).isObject()) {
        err = "This is not a JPurrr-8000 preset."; return false;
    }
    PresetEntry e;
    for (int i = 0; i < kNumParams; ++i) e.v[(size_t) i] = kParams[i].def;
    e.name = j["name"].toString().substring(0, 40);
    const auto ty = j.getProperty("type", {}).toString();
    bool known = false; for (int i = 0; i < 10; ++i) known |= ty == jp::layout::kTypes[i];
    e.type = known ? ty : juce::String("Lead");
    if (auto* ga = j.getProperty("genres", {}).getArray()) for (auto& g : *ga) e.genres.add(g.toString());
    if (auto* ta = j.getProperty("tags", {}).getArray()) for (auto& g : *ta) e.tags.add(g.toString());
    e.bank = j.getProperty("bank", "F").toString().substring(0, 1).toUpperCase();
    e.slot = (int) j.getProperty("slot", 0);
    int bad = 0;
    if (auto* vo = j["v"].getDynamicObject()) {
        for (auto& nv : vo->getProperties()) {
            const int idx = paramIndexById(nv.name.toString().toRawUTF8());
            if (idx < 0 || isNoSave(idx)) continue;
            if (!(nv.value.isInt() || nv.value.isInt64() || nv.value.isDouble())) { ++bad; continue; }
            const double d = (double) nv.value;
            if (!std::isfinite(d) || d < kParams[idx].min || d > kParams[idx].max) { ++bad; continue; }
            e.v[(size_t) idx] = (int) std::lround(d);
        }
    }
    if (skipped) *skipped = bad;
    out = e; return true;
}
