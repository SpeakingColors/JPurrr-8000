#include "PlayerHost.h"
#include <thread>
#include <chrono>

PlayerHost::PlayerHost() {
    for (auto& s : slots) s.ev.reset(new jpurrr::PlayEvent[(size_t) jpurrr::kMaxLoadedEvents]);
}

void PlayerHost::select(int libraryIndex) {
    const int n = jpurrr::libraryItemCount();
    if (libraryIndex < 0 || libraryIndex >= n) { selLib.store(-1); selRaw.store(-1); gen.fetch_add(1); return; }
    if (!lock.load()) setBpm(jpurrr::libraryItem(libraryIndex).bpm);
    selLib.store(libraryIndex); selRaw.store(libraryIndex); gen.fetch_add(1);
}

void PlayerHost::step(int dir) {
    const int n = jpurrr::libraryItemCount(); if (n <= 0) return;
    int i = selLib.load();
    i = i < 0 ? (dir > 0 ? 0 : n - 1) : (i + dir + n) % n;
    select(i);
}

juce::String PlayerHost::pieceName() const {
    const int s = selRaw.load();
    if (s >= 0) return jpurrr::libraryItem(s).name;
    if (s <= -2 && userName.isNotEmpty()) return userName;
    return {};
}

bool PlayerHost::loadFile(const juce::File& f, juce::String& error) {
    juce::MemoryBlock mb;
    if (!f.existsAsFile() || !f.loadFileAsData(mb)) { error = "That file could not be read."; return false; }
    if (mb.getSize() > 4u * 1024u * 1024u) { error = "That file is too big for the player (over 4 MB)."; return false; }
    jpurrr::LoadedPiece pc; std::string err;
    if (!jpurrr::parseMidiFile((const std::uint8_t*) mb.getData(), mb.getSize(), pc, &err)) { error = juce::String(err) + "."; return false; }
    // wait (a moment) until the audio thread has taken the last change, so the slot we write is not the one it plays
    for (int i = 0; i < 100 && ack.load() != gen.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const int s = 1 - lastSlot;
    Slot& sl = slots[s];
    sl.n = (int) pc.ev.size();
    for (int i = 0; i < sl.n; ++i) sl.ev[(size_t) i] = pc.ev[(size_t) i];
    sl.beats = pc.beats;
    lastSlot = s;
    userName = f.getFileNameWithoutExtension().substring(0, 40) + " (" + juce::String(juce::roundToInt(pc.bpm)) + ")" + (pc.truncated ? " (cut)" : "");
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
