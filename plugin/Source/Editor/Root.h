// The editor content at design size (2920 x page height), scaled by the editor with a transform.
#pragma once
#include "PageView.h"
#include "Dialogs.h"

namespace jp {

class PnameW;
class MenuPanel;
class Bubble;
class KeyboardStrip;

class Root : public juce::Component, private juce::Timer {
public:
    explicit Root(JPurrr8000AudioProcessor& p);
    ~Root() override;
    void paint(juce::Graphics&) override;
    void tick(double nowSeconds);                      // 30 Hz from the editor
    void showPage(int p);
    int pageIndex() const { return page; }
    int designHeight() const;                          // page height, with the keyboard strip while PREVIEW is on
    bool stripShown() const;
    void layoutStrip();                                // place the keyboard strip under the page, show or hide it with PREVIEW
    void syncHeight();                                 // follow a PREVIEW change: resize the content and tell the editor
    std::function<void()> onPageChanged;               // the editor resizes itself
    std::function<void()> onDefaultSize;               // double click on a wood cheek: back to 100%
    void applyLook(const juce::String& id);
    bool applyState(const juce::String& name);          // test hook (snapshot tool): sound chain master guide ... look_navy reset

    Controller& controller() { return ctl; }
    Ctx& context() { return ctx; }
    const Look& look() const { return lookByIndex(ctx.lookIdx); }
    DisplayEnv& displayEnv() { return env; }
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isDown) override;
    void showValueBubble(juce::Component& c, int idx);
    void showTextBubble(juce::Component& c, const juce::String& text);   // the value bubble with a given text (keyboard wheels)
    KeyboardStrip* keyStrip() { return strip.get(); }
    void hideValueBubble();
    void hideTip();
    void showMenu(bool show);
    float scale = 1.0f;                                // current editor scale, for the size readout

private:
    JPurrr8000AudioProcessor& proc;
    Controller ctl;
    Ctx ctx;
    DisplayEnv env;
    int page = 0, lastFx = 1;
    juce::OwnedArray<PageView> pages;
    juce::OwnedArray<HBtnW> header;
    std::unique_ptr<PnameW> pname;
    std::unique_ptr<MenuPanel> menu;
    std::unique_ptr<Bubble> tipBubble, valBubble;
    std::unique_ptr<KeyboardStrip> strip;
    std::unique_ptr<DialogLayer> dialogs;
    juce::Image texture;
    juce::String lastInfo, lastMsg, lastTitle;
    const layout::Tip* hoverTip = nullptr; juce::Point<float> hoverPos; juce::uint32 hoverSince = 0;
    mutable layout::Tip wheelTips[2];                  // tips of the keyboard wheels (position follows the page)
    const layout::Tip* tipDone = nullptr;              // tip dismissed by a press: not shown again until the pointer reaches another element
    void timerCallback() override;
    void updateTip();
    const layout::Tip* tipAtPoint(juce::Point<float> p, juce::Rectangle<float>& region) const;
    juce::String tipText(const layout::Tip&) const;
    float ftrDy() const { return (float) designHeight() - (float) layout::kChromeH; }
};

} // namespace jp
