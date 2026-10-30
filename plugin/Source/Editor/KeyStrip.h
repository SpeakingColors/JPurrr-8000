// The live keyboard strip at the bottom of the window (mockup #kbd): BEND wheel, MOD wheel and 61 keys C2..C7.
// Notes go to the processor (lock-free queue), the wheels move the BEND and MOD lever values, the same values the lever pad uses.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "Widgets.h"
#include "jpurrr/keyboard.h"

namespace jp {

class WheelW;
class KeysW;
class SplitW;
class InfoW;

class KeyboardStrip : public juce::Component {
public:
    KeyboardStrip(JPurrr8000AudioProcessor& p, Ctx& ctx);
    ~KeyboardStrip() override;
    void paint(juce::Graphics&) override;
    void place(float stripY);                          // position under the current page
    void refresh();                                    // 30 Hz: follow the lever values (pad, MIDI CC 1), release notes that must not stick
    void lookChanged();
    // v3.5: while the PARTS band is shown the keys get the LOWER / UPPER tints and the split marker (KEY MODE DUAL and SPLIT)
    void setPartsMode(bool on);
    void keysRepaint();
    juce::Rectangle<float> infoRect() const;           // the KEYS / VEL readout over the keys (mockup v3.7 #kinfo) in strip coordinates
    juce::Rectangle<float> splitHandle() const;        // the split handle in strip coordinates (window x, y from the top of the strip); empty when not shown
    jp::KeyTint tint;
    // test hook (snapshot tool): handles jump to their values, pretend a wheel is dragged or has keyboard focus (-1 none, 0 BEND, 1 MOD)
    void snapshotState(int dragWheel, int focusWheel);
    void animateForTest(int ms);                       // test hook (headless check): run the wheel animations for ms without a message loop

    // notes (message thread)
    void noteOn(int note, int vel = jpurrr::kKeyVelocity);
    void noteOff(int note);
    void releaseAll();
    bool anyDown() const { return held.count() > 0; }
    int computerBase() const { return ck.base(); }          // the note A plays (48 = C3 at the start)
    int computerVelocity() const { return ck.velocity(); }
    void setComputerKeys(int base, int vel);           // test hook (snapshot tool): the octave and velocity of the computer keys
    bool isLit(int note) const { return held.isDown(note); }

    // computer keys, called by the editor root
    bool keyDown(const juce::KeyPress& k);             // true when the key is one of the computer keys (notes, Z / X octave, C / V velocity)
    void keyStateChanged();                            // a key went up: release notes whose key is no longer down

    std::function<void(juce::Component&, const juce::String&)> showBubble;   // value bubble under a component
    std::function<void()> hideBubble, hideTip;
    std::function<bool()> modalOpen;                   // a dialog is open: the computer keys do not play
    const Look& L() const { return x.L(); }

private:
    friend class WheelW; friend class KeysW; friend class SplitW; friend class InfoW;
    JPurrr8000AudioProcessor& proc; Ctx& x; Controller& ctl;
    jpurrr::HeldNotes held;
    bool lit[128] = {};
    jpurrr::ComputerKeys ck;                           // the computer keys that are down, the octave and the velocity
    std::unique_ptr<KeysW> keys;
    std::unique_ptr<SplitW> split;
    std::unique_ptr<InfoW> info;
    std::unique_ptr<WheelW> wheels[2];
    void releaseComputerKeys();
};

} // namespace jp
