#include "PlayerHost.h"
#include <thread>
#include <chrono>

PlayerHost::PlayerHost() {
    for (auto& s : slots) s.ev.reset(new jpurrr::PlayEvent[(size_t) jpurrr::kMaxLoadedEvents]);
    for (auto& s : gens) s.ev.reset(new jpurrr::PlayEvent[136]);
}

int PlayerHost::genCount() const { int k = 0; for (auto& s : gens) if (s.name.isNotEmpty()) ++k; return k; }
int PlayerHost::genSlotAt(int k) const {
    int order[kGenMax], n = 0;
    for (int i = 0; i < kGenMax; ++i) if (gens[i].name.isNotEmpty()) order[n++] = i;
    for (int a = 0; a < n; ++a) for (int b = a + 1; b < n; ++b) if (gens[order[a]].seq > gens[order[b]].seq) std::swap(order[a], order[b]);
    return k >= 0 && k < n ? order[k] : -1;
}
void PlayerHost::selectGen(int slot) {
    if (slot < 0 || slot >= kGenMax || gens[slot].name.isEmpty()) return;
    if (!lock.load()) setBpm(gens[slot].bpm);
    selLib.store(-1); selRaw.store(-10 - slot); gen.fetch_add(1);
}
int PlayerHost::addGenerated(const jpurrr::MelodyGenResult& g) {
    if (!g.ok) return -1;
    for (int i = 0; i < 100 && ack.load() != gen.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const juce::String name = juce::String::fromUTF8(g.name);
    const int cur = selRaw.load() <= -10 ? -10 - selRaw.load() : -1;                 // the slot that is playing now
    if (cur >= 0 && gens[cur].name == name) {                                        // the same settings again: the notes are identical, keep the slot
        if (!lock.load()) setBpm(g.bpm);
        genMeta[cur] = std::make_unique<jpurrr::MelodyGenResult>(g);
        selLib.store(-1); selRaw.store(-10 - cur); gen.fetch_add(1); return cur;
    }
    int s = -1;
    for (int i = 0; i < kGenMax; ++i) if (gens[i].name == name && i != cur) { s = i; break; }   // the same settings again: replaced in place
    if (s < 0) for (int i = 0; i < kGenMax; ++i) if (gens[i].name.isEmpty() && i != cur) { s = i; break; }
    if (s < 0) { int oldest = 1 << 30; for (int i = 0; i < kGenMax; ++i) if (i != cur && gens[i].seq < oldest) { oldest = gens[i].seq; s = i; } }
    if (s < 0) return -1;
    Slot& sl = gens[s];
    sl.n = g.n;
    for (int i = 0; i < g.n; ++i) sl.ev[(size_t) i] = g.ev[(size_t) i];
    sl.beats = g.beats; sl.bpm = g.bpm; sl.name = name; sl.seq = ++seqNow;
    genMeta[s] = std::make_unique<jpurrr::MelodyGenResult>(g);
    if (!lock.load()) setBpm(g.bpm);
    selLib.store(-1); selRaw.store(-10 - s); gen.fetch_add(1);
    return s;
}

void PlayerHost::select(int libraryIndex) {
    const int n = jpurrr::libraryItemCount();
    if (libraryIndex < 0 || libraryIndex >= n) { selLib.store(-1); selRaw.store(-1); gen.fetch_add(1); return; }
    if (!lock.load()) setBpm(jpurrr::libraryItem(libraryIndex).bpm);
    selLib.store(libraryIndex); selRaw.store(libraryIndex); gen.fetch_add(1);
}

void PlayerHost::step(int dir) {
    // the list is the library, then the generated melodies, then the files of your own (as the mockup's list); it wraps around
    const int n = jpurrr::libraryItemCount(), ng = genCount(), total = n + ng + userCount(); if (total <= 0) return;
    int cur = selLib.load();
    const int sRaw = selRaw.load();
    if (cur < 0 && sRaw <= -10) { for (int k = 0; k < ng; ++k) if (genSlotAt(k) == -10 - sRaw) cur = n + k; }
    else if (cur < 0 && sRaw <= -2) { for (int k = 0; k < userCount(); ++k) if (userSlotAt(k) == -2 - sRaw) cur = n + ng + k; }
    const int i = cur < 0 ? (dir > 0 ? 0 : total - 1) : (cur + dir + total) % total;
    if (i < n) select(i); else if (i < n + ng) selectGen(genSlotAt(i - n)); else selectUser(userSlotAt(i - n - ng));
}

juce::String PlayerHost::pieceName() const {
    const int s = selRaw.load();
    if (s >= 0) return jpurrr::libraryItem(s).name;
    if (s <= -10) return gens[-10 - s].name;
    if (s <= -2) return slots[-2 - s].name;
    return {};
}
juce::String PlayerHost::groupName() const {
    const int s = selRaw.load();
    if (s <= -10) return "Uplifting trance melody generator";
    if (s <= -2) return "Your MIDI file";
    for (int g = 0; g < jpurrr::libraryGroupCount(); ++g) { const auto& gr = jpurrr::libraryGroup(g); if (s >= gr.first && s < gr.first + gr.count) return gr.name; }
    return {};
}
double PlayerHost::pieceBpm() const {
    const int s = selRaw.load();
    return s >= 0 ? (double) jpurrr::libraryItem(s).bpm : s <= -10 ? gens[-10 - s].bpm : s <= -2 ? slots[-2 - s].bpm : bpmA.load();
}
jpurrr::PlayView PlayerHost::view() const {
    jpurrr::PlayView v; const int s = selRaw.load();
    if (s >= 0) { const auto& it = jpurrr::libraryItem(s); v.ev = it.ev; v.n = it.n; v.beats = it.beats; }
    else if (s <= -10) { const Slot& sl = gens[-10 - s]; v.ev = sl.ev.get(); v.n = sl.n; v.beats = sl.beats; }
    else if (s <= -2) { const Slot& sl = slots[-2 - s]; v.ev = sl.ev.get(); v.n = sl.n; v.beats = sl.beats; }
    return v;
}
int PlayerHost::userCount() const { int k = 0; for (auto& s : slots) if (s.name.isNotEmpty()) ++k; return k; }
int PlayerHost::userSlotAt(int k) const {
    int order[2], n = 0;
    for (int i = 0; i < 2; ++i) if (slots[i].name.isNotEmpty()) order[n++] = i;
    if (n == 2 && slots[order[0]].seq > slots[order[1]].seq) std::swap(order[0], order[1]);
    return k >= 0 && k < n ? order[k] : -1;
}
void PlayerHost::selectUser(int slot) {
    if (slot < 0 || slot > 1 || slots[slot].name.isEmpty()) return;
    if (!lock.load()) setBpm(slots[slot].bpm);
    selLib.store(-1); selRaw.store(-2 - slot); gen.fetch_add(1);
}

bool PlayerHost::loadFile(const juce::File& f, juce::String& error) {
    juce::MemoryBlock mb;
    if (!f.existsAsFile() || !f.loadFileAsData(mb)) { error = "That file could not be read."; return false; }
    if (mb.getSize() > 4u * 1024u * 1024u) { error = "That file is too big for the player (over 4 MB)."; return false; }
    jpurrr::LoadedPiece pc; std::string err;
    if (!jpurrr::parseMidiFile((const std::uint8_t*) mb.getData(), mb.getSize(), pc, &err)) { error = juce::String(err) + "."; return false; }
    // wait (a moment) until the audio thread has taken the last change, so the slot we write is not the one it plays
    for (int i = 0; i < 100 && ack.load() != gen.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const juce::String name = "MIDI: " + f.getFileNameWithoutExtension().substring(0, 40) + " (" + juce::String(juce::roundToInt(pc.bpm)) + ")" + (pc.truncated ? " (cut)" : "");
    const int cur = selRaw.load() <= -2 ? -2 - selRaw.load() : -1;               // the slot that is playing now
    int s;
    if (cur >= 0) s = 1 - cur;                                                   // never write the one in use
    else if (slots[0].name.isEmpty()) s = 0; else if (slots[1].name.isEmpty()) s = 1;
    else s = slots[0].seq < slots[1].seq ? 0 : 1;                                // both used and none playing: the older one goes
    Slot& sl = slots[s];
    sl.n = (int) pc.ev.size();
    for (int i = 0; i < sl.n; ++i) sl.ev[(size_t) i] = pc.ev[(size_t) i];
    sl.beats = pc.beats; sl.bpm = pc.bpm; sl.name = name; sl.seq = ++seqNow;
    if (slots[1 - s].name == name) slots[1 - s].name = {};                       // the same file again: one entry (the old copy is only hidden, its notes stay safe)
    if (!lock.load()) setBpm(pc.bpm);
    selLib.store(-1); selRaw.store(-2 - s); gen.fetch_add(1);
    return true;
}

void PlayerHost::reset() { player.stop(); wasPlaying = false; seenGen = 0; ack.store(gen.load()); }

int PlayerHost::process(int n, double sr, jpurrr::Player::Out* out, int cap) {
    const int g = gen.load();
    if (g != seenGen) {
        seenGen = g;
        const int s = selRaw.load();
        jpurrr::PlayView v;
        if (s >= 0) { const jpurrr::LibItem& it = jpurrr::libraryItem(s); v.ev = it.ev; v.n = it.n; v.beats = it.beats; }
        else if (s <= -10) { const Slot& sl = gens[-10 - s]; v.ev = sl.ev.get(); v.n = sl.n; v.beats = sl.beats; }
        else if (s <= -2) { const Slot& sl = slots[-2 - s]; v.ev = sl.ev.get(); v.n = sl.n; v.beats = sl.beats; }
        player.setPiece(v);
        loopA.store((float) (v.beats > 4.0 ? v.beats : 4.0));
        ack.store(g);
    }
    const bool want = playing.load();
    if (want != wasPlaying) { wasPlaying = want; if (want) player.play(); else player.stop(); }
    player.setBpm(bpmA.load()); player.setTranspose(trA.load());
    const int k = player.process(n, sr, out, cap);
    posA.store((float) player.position());
    return k;
}
