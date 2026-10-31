// The PLAYER bar of mockup v3.8 (#plr): a small looping MIDI player above the keyboard strip, shown while the PLAYER switch (CONTROLLER panel) is on.
// It controls the processor's PlayerHost (the audio thread plays the notes through the current sound); this file is only the user interface.
// Coordinates inside the bar: x is a window x (kPlayer.bar.x is its left edge), y is relative to the top of the bar.
#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace jp {

class PlBtn; class PlSel; class PlNum; class PlLock; class PlPos; class PlMenu;

class PlayerBar : public juce::Component {
public:
    PlayerBar(JPurrr8000AudioProcessor& p, Ctx& ctx, DisplayEnv& env);
    ~PlayerBar() override;
    void place(float barY);                            // the bar's top in the window
    void paint(juce::Graphics&) override;
    void refresh();                                    // 30 Hz: follow the host (piece, tempo, lock, transpose, playing), animate the position bar
    void lookChanged();
    const layout::Tip* tipAt(juce::Point<float> rootPoint);   // tip of the element under a point in editor (design) coordinates, or nullptr
    juce::Component& menuComponent();                  // the piece list: a child of the editor root (it opens upwards, outside the bar)
    bool menuOpen() const;
    void closeMenu();
    void openMenu();
    bool menuOwns(juce::Component* c) const;           // c is the list or one of its parts, or the piece box
    void stopAll();                                    // the bar is hidden: stop the piece, close the list

    std::function<void(juce::Component&, const juce::String&)> showBubble;   // value bubble under a component
    std::function<void()> hideBubble, hideTip;

    // test hooks (snapshot and checks)
    void demo(int libraryIndex, float beat);           // show the playing state with the playhead at beat, without sound and without a timer (the mockup's __JPplayer.demo)
    void stopDemo();
    bool loadTestFile(const juce::File& f);            // loads a MIDI file like the LOAD button (no dialog)
    juce::File exportToFile(const juce::File& dir);    // writes the current piece like EXPORT MIDI, returns the file
    static juce::String midiFileName(const juce::String& pieceName);   // plMidiName(): "<name>.mid" with only letters, digits, # and - kept
    bool playingShown() const;                         // the PLAY button shows STOP (playing, or the demo state)
    JPurrr8000AudioProcessor& proc;
    Ctx& x;
    DisplayEnv& env;
    Controller& ctl;
    const Look& L() const { return x.L(); }

private:
    friend class PlBtn; friend class PlSel; friend class PlNum; friend class PlLock; friend class PlPos; friend class PlMenu;
    std::unique_ptr<PlBtn> play, prev, next, load, exp;
    std::unique_ptr<PlSel> sel;
    std::unique_ptr<PlNum> bpm, tr;
    std::unique_ptr<PlLock> lock;
    std::unique_ptr<PlPos> pos;
    std::unique_ptr<PlMenu> menu;
    layout::Tip tipBuf[16]; float barY = 0;
    bool demoOn = false; float demoBeat = 0;
    juce::String lastSig;
    void togglePlay();
    void doLoad();
    void doExport();
    void say(const juce::String& t) { ctl.say(t); }
    juce::String posText() const;                      // "3 / 8"
    float posBeats() const;                            // where the playhead is
    void ensurePiece();
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace jp
