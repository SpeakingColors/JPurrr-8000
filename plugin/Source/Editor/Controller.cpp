#include "Controller.h"
#include "Layout.h"
#include <map>
using namespace jpurrr;
using namespace jp;
namespace L = jp::layout;

// ------------------------------------------------------------------ settings
Settings& Settings::get() { static Settings s; static bool loaded = false; if (!loaded) { loaded = true; s.load(); } return s; }
static juce::File settingsFile() { return PresetManager::presetsDir().getParentDirectory().getChildFile("settings.json"); }
void Settings::load() {
    juce::var j; if (juce::JSON::parse(settingsFile().loadFileAsString(), j).failed() || !j.isObject()) return;
    bubbles = (bool) j.getProperty("bubbles", true); power = (bool) j.getProperty("power", true);
    look = j.getProperty("look", "jp").toString();
    if (look != "jp" && look != "navy" && look != "soda" && look != "purr") look = "jp";
}
void Settings::save() const {
    auto* o = new juce::DynamicObject(); o->setProperty("bubbles", bubbles); o->setProperty("power", power); o->setProperty("look", look);
    auto f = settingsFile(); f.getParentDirectory().createDirectory(); f.replaceWithText(juce::JSON::toString(juce::var(o)));
}

// ------------------------------------------------------------------ helpers
namespace {
juce::String sg(int v) { return v > 0 ? "+" + juce::String(v) : juce::String(v); }
juce::String collapse(const char* s) { return juce::String(s).replaceCharacters("\t\n", "  ").trim(); }
const char* const ABC = "ABCDEFG";
}

int Controller::idx(const char* id) {
    static std::map<juce::String, int> cache;
    auto it = cache.find(id);
    if (it != cache.end()) return it->second;
    const int i = paramIndexById(id); jassert(i >= 0);
    cache[id] = i; return i;
}

Controller::Controller(JPurrr8000AudioProcessor& p) : proc(p), pm(p.presets) {}

int Controller::get(int i) const {
    if (i == P_m2d) return proc.getInt(P_m2dp + juce::jlimit(0, 2, proc.getInt(P_m2s)));
    return proc.getInt(i);
}
bool Controller::isSaved(int i) const { return !(kParams[i].flags & kNoSave); }

void Controller::begin(int i) { proc.beginGesture(i == P_m2d ? P_m2dp + juce::jlimit(0, 2, proc.getInt(P_m2s)) : i); }
void Controller::end(int i) { proc.endGesture(i == P_m2d ? P_m2dp + juce::jlimit(0, 2, proc.getInt(P_m2s)) : i); }

juce::String Controller::label(int i) const { return collapse(kParams[i].label); }

juce::String Controller::fmt(int i) const {
    static std::vector<const L::FmtTable*> tab;
    if (tab.empty()) {
        tab.assign(kNumParams, nullptr);
        for (int k = 0; k < L::kNumFmt; ++k) { const int pi = paramIndexById(L::kFmt[k].id); if (pi >= 0) tab[(size_t) pi] = &L::kFmt[k]; }
    }
    const int v = get(i); const ParamDef& d = kParams[i];
    if (auto* t = tab[(size_t) i]) { const int k = v - t->min; if (k >= 0 && k < t->n) return juce::String::fromUTF8(t->s[k]); }
    if (d.optCount > 0 && v >= d.min && v - d.min < d.optCount) return juce::String::fromUTF8(kOptionText[d.optStart + (v - d.min)]);
    if (d.type == PType::Switch) return v ? "ON" : "OFF";
    return d.min < 0 ? sg(v) : juce::String(v);
}

// mockup deps(id) + the side effects of setP
void Controller::deps(int i) {
    if (i == P_o1w) {
        if (get(P_o1w) == 3) {                                    // FEEDBACK OSC is mono only
            if (!(get(P_mono) || get(P_leg))) proc.setInt(P_mono, 1);
            proc.setInt(P_uni, 0);
        }
    }
    if (i == P_uni && get(P_uni) && get(P_o1w) == 3) { proc.setInt(P_uni, 0); say("Unison cannot be used with FEEDBACK OSC"); }
    if ((i == P_mono || i == P_leg) && get(i)) proc.setInt(i == P_mono ? P_leg : P_mono, 0);   // exclusive
    if (i == P_m2s) { /* m2d is a proxy of m2dp/m2df/m2da: nothing to copy */ }
    if (i == P_mono || i == P_leg || i == P_uni || i == P_vc) { lcd2Override.clear(); lcd2Until = 0; }
}

void Controller::quiet(int i, int v) {
    const ParamDef& d = kParams[i];
    v = juce::jlimit(d.min, d.max, v);
    if (i == P_m2d) { proc.setInt(P_m2dp + juce::jlimit(0, 2, proc.getInt(P_m2s)), v); return; }
    proc.setInt(i, v);
    deps(i);
}

void Controller::user(int i, int v) {
    const ParamDef& d = kParams[i];
    v = juce::jlimit(d.min, d.max, v);
    if (i == P_gpt && v > 0 && i < kNumParams) {
        // choosing a built in gate pattern sets the 16 steps (mockup GPATS)
        static const char* const pats[8] = { "xxx.xx.xxx.xx.x.", "x.xx.x.xx.xx.x.x", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "x..x..x..x..x...", "x..x..x.x..x..x.", "xxx.xxx.xxx.xxx.", "x.x.xxxxx.x.xxxx" };
        int m = 0; for (int k = 0; k < 16; ++k) if (pats[v - 1][k] == 'x') m |= 1 << k;
        proc.setInt(P_gpat, m);
    }
    quiet(i, v);
    if (d.type == PType::Switch && (d.flags & kMomentary)) return;
    touchLcd(i);
    if (isSaved(i) || i == P_m2d) markDirty();
    if (touched) touched(i);
}

void Controller::fromDisplay(int i, int v) {
    quiet(i, v);
    if (i == P_rib) ribbonApply();
    if (isSaved(i)) markDirty();
}

void Controller::touchLcd(int i) {
    lcd2Override = label(i).substring(0, 16).paddedRight(' ', 17) + fmt(i);
    lcd2Until = juce::Time::getMillisecondCounter() + 1800;
}
juce::String Controller::showL2() const {
    return "VOICES " + juce::String(get(P_vc)) + "  " + (get(P_mono) ? "MONO" : get(P_leg) ? "LEGATO" : get(P_uni) ? "UNISON" : "POLY");
}
juce::String Controller::lcd2() const {
    if (lcd2Until != 0 && (juce::int32) (juce::Time::getMillisecondCounter() - lcd2Until) < 0) return lcd2Override;
    return showL2();
}
Controller::Nav Controller::nav() const {
    const int s = pm.cur.slot - 1; const int bi = juce::jmax(0, juce::String(ABC).indexOfChar(pm.cur.bank[0]));
    return { bi * 2 + (s >= 64 ? 1 : 0), (s % 64) / 8 + 1, s % 8 + 1 };
}
juce::String Controller::lcd1() const {
    auto n = nav();
    return ((editOn ? juce::String("EDIT  ") : (perform ? juce::String("PERFORM ") : juce::String("PATCH "))) + pm.cur.bank + juce::String(n.g % 2 + 1) + ":" + juce::String(n.b) + "-" + juce::String(n.n) + " " + pm.cur.name).substring(0, 30);
}
juce::String Controller::title() const { return pm.cur.name + (pm.cur.dirty ? " *" : ""); }
juce::String Controller::info() const {
    return pm.cur.bank + (pm.cur.slot ? juce::String(pm.cur.slot).paddedLeft('0', 3) : juce::String()) + (pm.cur.factory ? juce::String::fromUTF8(" \xc2\xb7 factory") : juce::String::fromUTF8(" \xc2\xb7 mine"));
}
void Controller::say(const juce::String& t) { msgText = t; msgUntil = juce::Time::getMillisecondCounter() + 3200; }
juce::String Controller::message() const {
    if (msgUntil != 0 && (juce::int32) (juce::Time::getMillisecondCounter() - msgUntil) < 0) return msgText;
    return {};
}

bool Controller::lit(const juce::String& id) const {
    const auto n = nav();
    if (id.startsWith("bb")) return !editOn && id.substring(2).getIntValue() == n.b;
    if (id.startsWith("nb")) return id.substring(2).getIntValue() == n.n;
    if (id == "rba") return assigning == kRib;
    if (id == "va") return assigning == kVel;
    if (id == "edit") return editOn;
    if (id == "pp") return perform;
    if (id == "shift") return shiftOn;
    return false;
}
bool Controller::blink(const juce::String& id) const { return editOn && id.startsWith("bb"); }

void Controller::controlTouched(int i) {
    if (assigning == kRib) { ribT = i; ribBase = get(i); say("Ribbon and lever now move " + label(i)); }
    else if (assigning == kVel) { if (velSet.count(i)) velSet.erase(i); else velSet.insert(i); say("Velocity morph " + juce::String(velSet.count(i) ? "on" : "off") + " for " + label(i) + " (works in the plugin)"); }
    else return;
    assigning = kNone;
}
void Controller::ribbonBegin() { ribBase = ribT >= 0 ? get(ribT) : 0; }
void Controller::ribbonEnd() {
    if (!get(P_rhold)) { proc.setInt(P_rib, 0); if (ribT >= 0) quiet(ribT, ribBase); }
}
void Controller::ribbonApply() {
    if (ribT < 0) return;
    const ParamDef& t = kParams[ribT]; const double n = get(P_rib) / 127.0;
    const double v = get(P_rrel) ? ribBase + (n - .5) * (t.max - t.min) : t.min + n * (t.max - t.min);
    quiet(ribT, (int) std::lround(v));
}

void Controller::markDirty() { if (!pm.cur.dirty) pm.cur.dirty = true; }

void Controller::afterLoad() {
    deps(P_o1w);
    lcd2Override.clear(); lcd2Until = 0;
}

void Controller::load(int li) {
    if (li < 0 || li >= pm.size()) return;
    const PresetEntry& e = pm.at(li);
    proc.setAll(e.v.data());
    afterLoad();
    pm.cur.name = e.name; pm.cur.bank = e.bank; pm.cur.slot = e.slot; pm.cur.factory = e.factory; pm.cur.type = e.type; pm.cur.dirty = false;
    pm.pushHistory(e);
    proc.presetName = e.name;
}
void Controller::step(int d) {
    const int n = pm.size(); if (n == 0) return;
    int i = pm.indexOfCurrent(); if (i < 0) i = 0;
    load((i + d + n) % n);
}
void Controller::navTo(int g, int b, int n) {
    const auto cn = nav();
    int G = g < 0 ? cn.g : g, B = b < 0 ? cn.b : b, N = n < 0 ? cn.n : n;
    G = (G + 14) % 14;
    const juce::String bank = juce::String::charToString(ABC[G >> 1]);
    const int slot = (G % 2) * 64 + (B - 1) * 8 + N;
    const int li = pm.find(bank, slot);
    if (li >= 0) load(li);
    else { say("Empty slot " + bank + juce::String(G % 2 + 1) + ":" + juce::String(B) + "-" + juce::String(N)); lcd2Override = "(empty)"; lcd2Until = juce::Time::getMillisecondCounter() + 1500; }
}

PresetEntry Controller::snapshot(const juce::String& name) const {
    PresetEntry e; e.name = name; e.type = pm.cur.type;
    for (int i = 0; i < kNumParams; ++i) e.v[(size_t) i] = kParams[i].def;
    for (int i = 0; i < kNumParams; ++i) if (isSaved(i)) e.v[(size_t) i] = proc.getInt(i);
    return e;
}

bool Controller::saveAs(const juce::String& name, const juce::String& type, const juce::StringArray& genres, const juce::StringArray& tags, const juce::String& bank, bool overwrite) {
    PresetEntry e = snapshot(name); e.type = type; e.genres = genres; e.tags = tags; e.bank = bank;
    juce::String uid;
    if (overwrite) { const int ex = pm.findUserByName(name, bank); if (ex >= 0) uid = pm.at(ex).uid; }
    const int li = pm.saveUser(e, uid);
    if (li < 0) { say("Bank " + bank + " is full"); return false; }
    load(li);
    return true;
}
void Controller::renameCurrent(const juce::String& v) {
    if (pm.cur.factory) {
        PresetEntry e = snapshot(v); e.type = pm.cur.type; e.bank = "E";
        const int li = pm.saveUser(e, {});
        if (li >= 0) { load(li); say("Saved as your own copy in bank E"); } else say("Bank E is full");
    } else {
        const int li = pm.find(pm.cur.bank, pm.cur.slot);
        if (li >= 0 && pm.renameUser(li, v)) { pm.cur.name = v; proc.presetName = v; say("Renamed"); }
    }
}
bool Controller::deleteCurrent() {
    const int li = pm.find(pm.cur.bank, pm.cur.slot);
    if (pm.cur.factory || li < 0) return false;
    pm.deleteUser(li); load(0); say("Deleted"); return true;
}

juce::String Controller::exportText(const PresetEntry& e) { return PresetManager::toJson(e, false); }

bool Controller::importText(const juce::String& text, juce::String& err) {
    PresetEntry e; int bad = 0;
    if (!PresetManager::fromJson(text, e, err, &bad)) return false;
    e.bank = "F"; e.slot = 0;
    const int li = pm.saveUser(e, {});
    if (li < 0) { err = "Bank F is full"; return false; }
    load(li); if (closeDialog) closeDialog(); say("Imported" + juce::String(bad ? " (" + juce::String(bad) + " values skipped)" : ""));
    return true;
}

// Plumbing for the SysEx helper: replace the bodies with core/include/jpurrr/sysex.h and preset_io.h calls.
bool Controller::importFile(const juce::File& f, juce::String& err) {
    if (!f.existsAsFile()) { err = "File not found."; return false; }
    if (f.hasFileExtension("syx")) { err = "SysEx import comes with a later update."; return false; }   // TODO(sysex): jpurrr::sysex parse
    return importText(f.loadFileAsString(), err);
}
bool Controller::exportPatch(const juce::File& f, juce::String& err) {
    if (f.hasFileExtension("syx")) { err = "SysEx export comes with a later update."; return false; }    // TODO(sysex): jpurrr::sysex build
    auto e = snapshot(pm.cur.name);
    if (!f.replaceWithText(exportText(e))) { err = "Could not write the file."; return false; }
    return true;
}

// ------------------------------------------------------------------ front panel buttons (mockup ACT)
void Controller::action(const juce::String& id) {
    if (id == "pp") { perform = !perform; }
    else if (id == "shift") { shiftOn = !shiftOn; }
    else if (id == "exit") { if (closeDialog) closeDialog(); editOn = false; shiftOn = false; assigning = kNone; }
    else if (id == "write") { if (openSave) openSave(); }
    else if (id == "edit") { editOn = !editOn; if (!editOn && closeDialog) closeDialog(); }
    else if (id == "dec") step(-1);
    else if (id == "inc") step(1);
    else if (id == "gdn") navTo(nav().g - 1, -1, -1);
    else if (id == "gup") navTo(nav().g + 1, -1, -1);
    else if (id == "osm") user(P_os, get(P_os) - 1);
    else if (id == "osp") user(P_os, get(P_os) + 1);
    else if (id == "ksm") user(P_ksh, get(P_ksh) - 1);
    else if (id == "ksp") user(P_ksh, get(P_ksh) + 1);
    else if (id == "qap") {
        const int t = juce::jlimit(0, 5, get(P_qtg));
        const auto& q = L::kQvals[t];
        for (int k = 0; k < q.n; ++k) { const juce::String key = q.ids[k]; if (key == "qon" || key == "qtg") continue; user(idx(q.ids[k]), q.vals[k]); }
        say("Chain set for " + juce::String::fromUTF8(kOptionText[kParams[P_qtg].optStart + t]));
    }
    else if (id == "rba") { assigning = kRib; say("Move a knob or slider to give it to the ribbon"); }
    else if (id == "va") { assigning = kVel; say("Move a knob or slider to set its velocity morph"); }
    else if (id == "arec") say("RPS recording comes with the plugin");
    else if (id == "arom") say("Built in RPS patterns come with the plugin");
    else if (id.startsWith("bb")) { const int b = id.substring(2).getIntValue(); if (editOn) { if (openPage) openPage(b); } else navTo(-1, b, -1); }
    else if (id.startsWith("nb")) navTo(-1, -1, id.substring(2).getIntValue());
}
