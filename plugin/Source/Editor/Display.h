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
};

// What a display may touch. Values are the integers of jpurrr::kParams (same as the mockup's P[id].v).
struct DisplayEnv {
    std::function<int(int)> get;                         // param index -> current value
    std::function<void(int, int)> set;                   // param index, value (changes from a drag; editor does the host gesture)
    std::function<void(int)> beginGesture, endGesture;   // for draggable displays
    jpurrr::Live* live = nullptr;                        // may be null (offline render)
    Palette pal;
    double nowSeconds = 0;                               // editor clock, for time based animation
    juce::Font mono(float px) const;                     // JetBrains Mono at px (mockup font sizes are CSS px at scale 1)
    juce::Font cond(float px, bool bold = false) const;  // Barlow Condensed
};

class Display : public juce::Component {
public:
    explicit Display(DisplayEnv& e) : env(e) {}
    // Called by the editor timer (about 30 Hz) while the page is visible. Default: repaint.
    virtual void tick() { repaint(); }
protected:
    DisplayEnv& env;
    int v(int idx) const { return env.get(idx); }
};

// Factory by mockup canvas id ("cv_o1", "cv_qdm", ...). Returns nullptr for unknown ids.
// Implemented in Editor/Displays/Registry.cpp. w, h are the canvas CSS size from the mockup (data-w, data-h).
std::unique_ptr<Display> makeDisplay(const juce::String& canvasId, DisplayEnv& env);

} // namespace jp
