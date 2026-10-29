// The PARTS band of mockup v3.5 (LOWER card, KEYS map, UPPER card), shown between the SOUND page and the keyboard strip while EDIT PARTS is on.
// Coordinates inside the band: x is a window x, y is relative to the top of the band (layout::kParts.top in the window).
#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace jp {

class PartCardW;

class PartsBand : public juce::Component {
public:
    PartsBand(JPurrr8000AudioProcessor& p, Ctx& ctx);
    ~PartsBand() override;
    void place();                                       // under the SOUND page
    void paint(juce::Graphics&) override;
    void refresh();                                     // 20 Hz: follow the parts, KEY MODE, PANEL SELECT and the split point
    void lookChanged();
    const layout::Tip* tipAt(juce::Point<float> rootPoint);   // tip of the element under a point in editor (design) coordinates
    std::function<void(int)> openBrowser;               // BROWSE and a click on the name: the preset browser for that part
    std::function<void()> hideTip;
    bool offCard(int part) const;                       // LOWER in SINGLE
private:
    friend class PartCardW;
    JPurrr8000AudioProcessor& proc; Ctx& x; Controller& ctl;
    juce::OwnedArray<PartCardW> cards;                  // [0] = UPPER, [1] = LOWER
    juce::OwnedArray<HBtnW> btns;                       // pt_prev_l, pt_next_l, pt_browse_l, pt_prev_u, ...
    juce::String sig;
    layout::Tip tipBuf[8]; std::string tipText[8];
    void clickCard(int part);
};

} // namespace jp
