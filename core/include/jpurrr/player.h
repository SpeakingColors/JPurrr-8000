// Built-in MIDI player: loops a short piece (a library demo or a MIDI file of your own) through the current sound, to test the synth.
// Pure C++17, no JUCE. The audio thread calls Player::process(); no allocation there. A piece is a list of notes [beat, length, note, velocity].
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace jpurrr {

struct PlayEvent { float beat; float len; std::uint8_t note; std::uint8_t vel; };      // len in beats, note 60 = C4, vel 1..127

// A piece as the player sees it. The notes belong to someone else (the library tables or a loaded file) and must outlive the player's use of them.
struct PlayView { const PlayEvent* ev = nullptr; int n = 0; double beats = 16.0; };     // beats: loop length, a multiple of 4

struct LibItem { const char* id; const char* name; int bpm; float beats; const PlayEvent* ev; int n; };
struct LibGroup { const char* name; int first; int count; };                          // items first .. first+count-1 of the flat list

// Generated from data/player_library.json by tools/build_player_library.py (core/src/player_library_gen.cpp).
int libraryItemCount();
const LibItem& libraryItem(int i);
int libraryGroupCount();
const LibGroup& libraryGroup(int g);
int libraryFind(const char* id);                       // index of the item with this id, or -1

// A piece read from a standard MIDI file.
struct LoadedPiece {
    std::string name;
    double bpm = 120.0;
    double beats = 4.0;
    std::vector<PlayEvent> ev;                         // sorted by beat
    bool truncated = false;                            // more notes than kMaxLoadedEvents: the rest were dropped
    PlayView view() const { PlayView v; v.ev = ev.data(); v.n = (int) ev.size(); v.beats = beats; return v; }
};
constexpr int kMaxLoadedEvents = 8000;
// Reads format 0 or 1 files with ticks per quarter note. Notes on channel 10 (drums) are skipped. The first tempo is used. The start moves back to
// a bar line (multiple of 4 beats), the loop length is the end rounded up to 4 beats. Returns false with a message for anything else.
bool parseMidiFile(const std::uint8_t* data, std::size_t size, LoadedPiece& out, std::string* error = nullptr);

class Player {
public:
    struct Out { int offset; bool on; std::uint8_t note; std::uint8_t vel; };      // offset: sample within the block
    static constexpr int kMaxSounding = 192;
    static constexpr int kMaxOut = 384;                                            // largest number of events process() can return in one block

    void setPiece(const PlayView& v);                  // the next process() starts the new piece from its beginning (sounding notes end first)
    void setBpm(double bpm) { bpm_ = bpm < 20.0 ? 20.0 : bpm > 300.0 ? 300.0 : bpm; }
    void setTranspose(int semitones) { transpose_ = semitones < -48 ? -48 : semitones > 48 ? 48 : semitones; }
    double bpm() const { return bpm_; }
    int transpose() const { return transpose_; }
    void play() { if (!playing_) { playing_ = true; restart_ = true; } }
    void stop() { if (playing_) { playing_ = false; stopping_ = true; } }
    bool playing() const { return playing_; }
    double position() const { return pos_ >= loop_ ? std::fmod(pos_, loop_) : pos_; }  // beats into the loop

    // Advances n samples. Fills out with the note events (sorted by offset, note offs first at the same offset). Returns how many.
    int process(int n, double sampleRate, Out* out, int cap);
    // Note offs for everything that is sounding, now. Use after stop() or before the synth is reset.
    int flush(Out* out, int cap);

private:
    struct Sounding { std::uint8_t note; double offAt; int minOff; };
    void startOver();
    PlayView v_{};
    PlayView pending_{}; bool hasPending_ = false;
    double bpm_ = 120.0; int transpose_ = 0;
    bool playing_ = false, restart_ = false, stopping_ = false;
    double loop_ = 16.0; long lap_ = 0; int idx_ = 0;
    // time: beats = baseBeats_ + (count_ - baseCount_) * perSample_, so the result does not depend on the block size
    long long count_ = 0, baseCount_ = 0; double baseBeats_ = 0.0, perSample_ = 0.0, pos_ = 0.0, lastSr_ = 0.0, lastBpm_ = 0.0;
    double beatsAt(long long c) const { return baseBeats_ + (double) (c - baseCount_) * perSample_; }
    Sounding snd_[kMaxSounding]; int ns_ = 0;
};

}  // namespace jpurrr
