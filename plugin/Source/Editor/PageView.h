// One window page (SOUND, CHAIN, MASTER) built from the mockup layout: section frames, labels, controls, displays.
#pragma once
#include "Widgets.h"
#include <map>

namespace jp {

class PageView : public juce::Component {
public:
    PageView(Ctx& ctx, int pageIndex, DisplayEnv& env);
    ~PageView() override;
    void paint(juce::Graphics&) override;
    void refresh();                                   // 30 Hz: poll controls, labels
    void tickDisplays(double nowSeconds);
    void lookChanged();
    const layout::Tip* tipAt(juce::Point<float> p) const;     // innermost tip region at a design point
    const layout::Page& page;
    int index;
    const layout::Text* findText(const char* id) const;

private:
    Ctx& x; DisplayEnv& env;
    juce::OwnedArray<juce::Component> comps;
    juce::Array<Ctrl*> ctrls;
    juce::Array<Display*> displays;
    Display* o1Display = nullptr; Display* mbDisplay = nullptr;
    std::vector<char> skipText;
    std::map<juce::String, juce::String> dyn;
    void drawText(juce::Graphics& g, const layout::Text& t, const juce::String* override);
};

// The editor's own frame: wood cheeks, panel texture, header, footer.
juce::Image panelTexture(const Look& L);

} // namespace jp
