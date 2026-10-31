#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include "jpurrr/player.h"

// The built-in MIDI player seen from the plugin: the message thread (editor) sets what to play, the audio thread asks for the note events of each block.
// A piece is a library piece (tables compiled into the core) or one MIDI file the user loaded (copied into one of two fixed buffers, so the audio
// thread never allocates or waits).
class PlayerHost {
public:
    PlayerHost();
    // ---- message thread ----
    void select(int libraryIndex);                                   // -1 = no piece. Sets the tempo to the piece's own unless the lock is on
    int selected() const { return selLib.load(); }                   // library index, or -1 (also while a file of your own is loaded)
    bool loadFile(const juce::File& f, juce::String& error);         // a MIDI file of your own; the name is shown by pieceName()
    bool fileLoaded() const { return userName.isNotEmpty() && selRaw.load() <= -2; }
    juce::String pieceName() const;
    void setPlaying(bool on) { playing.store(on); }
    bool isPlaying() const { return playing.load(); }
    void setBpm(double bpm) { bpmA.store(juce::jlimit(20.0, 300.0, bpm)); }
    double bpm() const { return bpmA.load(); }
    void setTranspose(int st) { trA.store(juce::jlimit(-48, 48, st)); }
    int transpose() const { return trA.load(); }
    void setBpmLock(bool on) { lock.store(on); }
    bool bpmLock() const { return lock.load(); }
    float position() const { return posA.load(); }                  // beats into the loop, for the display
    float loopBeats() const { return loopA.load(); }
    // the piece after this one or before it, wrapping inside the library (also from a file of your own)
    void step(int dir);
    // ---- audio thread ----
    int process(int n, double sampleRate, jpurrr::Player::Out* out, int cap);
    int flush(jpurrr::Player::Out* out, int cap) { return player.flush(out, cap); }
    void reset();

private:
    struct Slot { std::unique_ptr<jpurrr::PlayEvent[]> ev; int n = 0; double beats = 4.0; };
    Slot slots[2];
    int lastSlot = 1;                                                // message thread: the slot that was written last
    juce::String userName;                                           // message thread
    std::atomic<int> selRaw { -1 };                                  // >= 0 library index, -1 none, -2 / -3 = user slot 0 / 1
    std::atomic<int> selLib { -1 };
    std::atomic<int> gen { 0 }, ack { 0 };
    std::atomic<bool> playing { false }, lock { false };
    std::atomic<double> bpmA { 138.0 };
    std::atomic<int> trA { 0 };
    std::atomic<float> posA { 0.f }, loopA { 16.f };
    // audio thread
    jpurrr::Player player;
    int seenGen = 0; bool wasPlaying = false;
};
