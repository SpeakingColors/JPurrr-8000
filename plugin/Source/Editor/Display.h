// Shared interface between the editor framework and the display canvases (cv_* in the mockup).
// Every canvas of the mockup becomes one Display subclass in Editor/Displays/*.cpp.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include "jpurrr/params_gen.h"
#include "jpurrr/live.h"

namespace jp {

// Same names as the mockup's CC object (cacheColors): bg, line, fill, grid, acc, txt.
// The editor fills it from the active look (the mockup's data-look themes).
struct Palette {
    juce::Colour bg { 0xff080d22 }, line { 0xff7fd0ff }, fill { 0x307fd0ff }, grid { 0xff1c2748 }, acc { 0xfff2a43a }, txt { 0xff9fb0dc };
    juce::Colour edge { 0xff0a0f2c };                    // the mockup's --edge, the 1 px canvas border
};

// What a display may touch. Values are the integers of jpurrr::kParams (same as the mockup's P[id].v).
struct DisplayEnv {
    std::function<int(int)> get;                         // param index -> current value
    std::function<void(int, int)> set;                   // param index, value (changes from a drag; editor does the host gesture)
    std::function<void(int)> beginGesture, endGesture;   // for draggable displays
    jpurrr::Live* live = nullptr;                        // may be null (offline render)
    Palette pal;
    double nowSeconds = 0;                               // editor clock, for time based animation
    // Additions by the display helper (all optional, defaults keep the old behaviour):
    double sampleRate = 44100;                           // audio sample rate, for the spectrum bin mapping
    double animPhase = -1;                               // test hook: >= 0 freezes the oscillator scroll phase and clock
    std::function<int()> ribbonTarget;                   // param index the ribbon moves (mockup ribT), -1 or empty: none
    juce::Font mono(float px) const;                     // JetBrains Mono at px (mockup font sizes are CSS px at scale 1)
    juce::Font cond(float px, bool bold = false) const;  // Barlow Condensed
};

class Display : public juce::Component {
public:
    explicit Display(DisplayEnv& e);                     // registers the instance for jp::noteTouch (Displays/Registry.cpp)
    ~Display() override;
    // Called by the editor timer (about 30 Hz) while the page is visible. Default: repaint.
    virtual void tick() { repaint(); }
    // A control was touched by the user (mockup: setP(id, v, true)). The oscillator displays show their
    // parameter view for about 1.7 s after CTRL 1 / CTRL 2 moved. Default: ignored.
    virtual void touched(int /*paramIndex*/) {}
    // Text the mockup writes into a DOM label next to the canvas (o1info under cv_o1, mbr1..4 under cv_qdm).
    // The editor places a label with it. slot 0 for single label displays. Default: empty.
    virtual juce::String infoText(int /*slot*/ = 0) const { return {}; }
protected:
    DisplayEnv& env;
    int v(int idx) const { return env.get(idx); }
};

// The editor calls this whenever the user changes a parameter (knob drag, wheel, typed value; not for preset loads
// and not for changes made by a display itself). Forwards to touched() of every live display.
void noteTouch(int paramIndex);

// Factory by mockup canvas id ("cv_o1", "cv_qdm", ...). Returns nullptr for unknown ids.
// Implemented in Editor/Displays/Registry.cpp. w, h are the canvas CSS size from the mockup (data-w, data-h).
std::unique_ptr<Display> makeDisplay(const juce::String& canvasId, DisplayEnv& env);

} // namespace jp
