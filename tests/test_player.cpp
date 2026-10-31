// Tests for the built-in MIDI player (core/src/player.cpp) and its library (data/player_library.json).
#include "test_util.h"
#include "jpurrr/player.h"
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <vector>
using namespace jpurrr;

namespace {
struct Ev { long at; bool on; int note; };
// runs the player for 'seconds' in blocks of 'block' samples and returns every event with its absolute sample time
std::vector<Ev> run(Player& p, double seconds, int block, double sr = 48000.0) {
    std::vector<Ev> all; std::vector<Player::Out> out(Player::kMaxOut);
    long t = 0; const long total = (long) (seconds * sr);
    while (t < total) {
        const int n = (int) std::min<long>(block, total - t);
        const int k = p.process(n, sr, out.data(), (int) out.size());
        for (int i = 0; i < k; ++i) all.push_back({ t + out[(size_t) i].offset, out[(size_t) i].on, out[(size_t) i].note });
        t += n;
    }
    return all;
}
}  // namespace

TEST(player_timing_and_loop) {
    // two notes in a 4 beat loop at 120 BPM: a beat is 0.5 s = 24000 samples
    static const PlayEvent ev[] = { { 0.f, 0.5f, 60, 100 }, { 2.f, 1.f, 64, 90 } };
    Player p; PlayView v; v.ev = ev; v.n = 2; v.beats = 4.0; p.setPiece(v); p.setBpm(120); p.play();
    const auto e = run(p, 4.1, 64);                                 // two laps and a bit
    std::vector<Ev> ons; for (const auto& x : e) if (x.on) ons.push_back(x);
    CHECK(ons.size() == 5);                                         // 60 64 | 60 64 | 60
    if (ons.size() >= 5) {
        CHECK(std::labs(ons[0].at - 0) <= 1 && ons[0].note == 60);
        CHECK(std::labs(ons[1].at - 48000) <= 1 && ons[1].note == 64);
        CHECK(std::labs(ons[2].at - 96000) <= 1 && ons[2].note == 60);   // the loop is 4 beats = 2 s
        CHECK(std::labs(ons[3].at - 144000) <= 1);
    }
    // every note off comes after its on and nothing is left sounding after a stop
    p.stop(); std::vector<Player::Out> out(Player::kMaxOut); const int k = p.process(64, 48000.0, out.data(), (int) out.size());
    for (int i = 0; i < k; ++i) CHECK(!out[(size_t) i].on);
}

TEST(player_block_size_does_not_change_the_result) {
    static const PlayEvent ev[] = { { 0.f, 0.2f, 60, 100 }, { 0.25f, 0.2f, 62, 100 }, { 0.5f, 0.2f, 64, 100 }, { 1.75f, 0.2f, 65, 100 }, { 3.9f, 0.05f, 67, 100 } };
    std::vector<Ev> ref;
    for (int block : { 1, 7, 64, 480, 4096 }) {
        Player p; PlayView v; v.ev = ev; v.n = 5; v.beats = 4.0; p.setPiece(v); p.setBpm(137); p.play();
        const auto e = run(p, 9.0, block);
        if (ref.empty()) ref = e;
        else {
            CHECK(e.size() == ref.size());
            for (size_t i = 0; i < e.size() && i < ref.size(); ++i) { CHECK(e[i].at == ref[i].at && e[i].on == ref[i].on && e[i].note == ref[i].note); }
        }
    }
    CHECK(ref.size() > 20);
}

TEST(player_transpose_bpm_and_retrigger) {
    static const PlayEvent ev[] = { { 0.f, 4.f, 60, 100 }, { 1.f, 1.f, 60, 100 } };      // the second note hits the first one again
    Player p; PlayView v; v.ev = ev; v.n = 2; v.beats = 4.0; p.setPiece(v); p.setBpm(60); p.setTranspose(12); p.play();
    const auto e = run(p, 1.5, 64);
    int on72 = 0, off72 = 0; for (const auto& x : e) { if (x.note == 72) (x.on ? on72 : off72)++; CHECK(x.note == 72); }
    CHECK(on72 == 2 && off72 == 1);                                   // the first 72 ended when the second began
    p.setTranspose(-100); p.setBpm(1000);
    CHECK(p.transpose() == -48 && p.bpm() == 300.0);
    p.setBpm(1); CHECK(p.bpm() == 20.0);
    // notes never leave 0..127
    Player q; PlayView w; static const PlayEvent hi[] = { { 0.f, 1.f, 120, 100 } }; w.ev = hi; w.n = 1; q.setPiece(w); q.setTranspose(48); q.play();
    for (const auto& x : run(q, 0.2, 64)) CHECK(x.note >= 0 && x.note <= 127);
}

TEST(player_new_piece_ends_sounding_notes) {
    static const PlayEvent a[] = { { 0.f, 8.f, 50, 100 } }, b[] = { { 0.f, 1.f, 70, 100 } };
    Player p; PlayView va; va.ev = a; va.n = 1; va.beats = 8; p.setPiece(va); p.setBpm(120); p.play();
    std::vector<Player::Out> out(Player::kMaxOut);
    p.process(480, 48000.0, out.data(), (int) out.size());
    PlayView vb; vb.ev = b; vb.n = 1; vb.beats = 4; p.setPiece(vb);
    const int k = p.process(480, 48000.0, out.data(), (int) out.size());
    bool off50 = false, on70 = false; for (int i = 0; i < k; ++i) { if (!out[(size_t) i].on && out[(size_t) i].note == 50) off50 = true; if (out[(size_t) i].on && out[(size_t) i].note == 70) on70 = true; }
    CHECK(off50 && on70);
    p.stop(); const int k2 = p.process(64, 48000.0, out.data(), (int) out.size());
    bool off70 = false; for (int i = 0; i < k2; ++i) if (!out[(size_t) i].on && out[(size_t) i].note == 70) off70 = true;
    CHECK(off70);
    CHECK(p.process(64, 48000.0, out.data(), (int) out.size()) == 0);        // stopped: silence
}

TEST(player_reads_a_midi_file) {
    // header: format 1, 2 tracks, 480 ticks per quarter. Tempo 138. Notes on channel 1 (E5 at beat 0, A4 at beat 1) and a drum note on channel 10.
    std::vector<std::uint8_t> f = { 'M','T','h','d',0,0,0,6,0,1,0,2,0x01,0xE0,
        'M','T','r','k',0,0,0,11, 0,0xFF,0x51,3,0x06,0xA2,0x5E, 0,0xFF,0x2F,0,
        'M','T','r','k',0,0,0,0 };
    std::vector<std::uint8_t> tr = { 0,0x90,76,100, 0x81,0x70,0x80,76,0,           // delta 0 on, delta 240 off (0x81 0x70 = 240)
                                     0x81,0x70,0x90,69,90, 0x81,0x70,0x80,69,0,   // delta 480 on, 240 off
                                     0,0x99,36,127, 60,0x89,36,0,                  // drums: skipped
                                     0,0xFF,0x2F,0 };
    f.resize(f.size() - 4); f.insert(f.end(), { 0,0,0,(std::uint8_t) tr.size() }); f.insert(f.end(), tr.begin(), tr.end());
    LoadedPiece pc; std::string err;
    CHECK(parseMidiFile(f.data(), f.size(), pc, &err));
    CHECK(std::fabs(pc.bpm - 138.0) < 0.01);
    CHECK(pc.ev.size() == 2);
    if (pc.ev.size() == 2) {
        CHECK(pc.ev[0].note == 76 && pc.ev[0].vel == 100 && std::fabs(pc.ev[0].beat - 0.f) < 1e-4 && std::fabs(pc.ev[0].len - 0.5f) < 1e-4);
        CHECK(pc.ev[1].note == 69 && std::fabs(pc.ev[1].beat - 1.f) < 1e-4);
    }
    CHECK(pc.beats == 4.0);
    // garbage never crashes and says no
    LoadedPiece bad; CHECK(!parseMidiFile((const std::uint8_t*) "hello world, not midi", 21, bad, &err) && !err.empty());
    for (size_t cut = 0; cut < f.size(); ++cut) { LoadedPiece x; parseMidiFile(f.data(), cut, x, nullptr); }     // every truncation is survived (ASan/UBSan check)
}

TEST(player_library_is_sound) {
    const int n = libraryItemCount(); CHECK(n >= 100);
    std::set<std::string> ids; int covered = 0;
    for (int g = 0; g < libraryGroupCount(); ++g) {
        const LibGroup& gr = libraryGroup(g); CHECK(gr.count > 0 && gr.name[0]);
        covered += gr.count; CHECK(gr.first + gr.count <= n);
    }
    CHECK(covered == n);
    for (int i = 0; i < n; ++i) {
        const LibItem& it = libraryItem(i);
        CHECK(ids.insert(it.id).second);                              // unique ids
        CHECK(libraryFind(it.id) == i);
        CHECK(it.n > 0 && it.bpm >= 60 && it.bpm <= 200 && it.beats >= 4.f && std::fmod(it.beats, 4.f) == 0.f);
        float prev = -1.f;
        for (int k = 0; k < it.n; ++k) {
            const PlayEvent& e = it.ev[k];
            CHECK(e.note <= 127 && e.vel >= 1 && e.vel <= 127 && e.len > 0.f && e.beat >= 0.f && e.beat < it.beats);
            CHECK(e.beat >= prev); prev = e.beat;                      // sorted
        }
    }
    CHECK(libraryFind("nonexistent") == -1);
    // every piece plays without overflowing the event buffer, at a small and a large block size
    for (int i = 0; i < n; ++i) {
        const LibItem& it = libraryItem(i); Player p; PlayView v; v.ev = it.ev; v.n = it.n; v.beats = it.beats;
        p.setPiece(v); p.setBpm(it.bpm); p.play();
        const auto e = run(p, 2.0 * it.beats * 60.0 / it.bpm, 480);
        int on = 0; for (const auto& x : e) on += x.on; CHECK(on >= it.n);
    }
}
