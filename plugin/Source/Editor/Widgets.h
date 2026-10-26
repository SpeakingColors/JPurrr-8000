// The mockup's controls as JUCE components: knob, slider (fader), switch, option group, dropdown, LCD.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Controller.h"
#include "Layout.h"
#include "Theme.h"

namespace jp {

struct Ctx {
    Controller& c;
    int lookIdx = 0;
    const Look& L() const { return lookByIndex(lookIdx); }
    std::function<void(juce::Component&, int)> showVal;     // value bubble under a control
    std::function<void()> hideVal, hideTip;
    std::function<void()> userAction;                       // any user edit (power saving timers etc.)
};

class Ctrl : public juce::Component {
public:
    explicit Ctrl(Ctx& c) : x(c) {}
    virtual void refresh() {}                               // poll the processor (30 Hz)
    virtual void lookChanged() { repaint(); }
protected:
    Ctx& x;
};

class KnobW : public Ctrl {
public:
    KnobW(Ctx& c, int paramIndex);
    void paint(juce::Graphics&) override;
    void refresh() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }
    int idx;
private:
    int cached = -99999; double dragStartV = 0; int dragStartY = 0; bool dragging = false;
};

class SliderW : public Ctrl {
public:
    SliderW(Ctx& c, int paramIndex, int h, float tk);
    void paint(juce::Graphics&) override;
    void refresh() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }
    int idx;
private:
    int hh; float tk; int cached = -99999; double dragStartV = 0; int dragStartY = 0; bool dragging = false;
};

class SwitchW : public Ctrl {
public:
    SwitchW(Ctx& c, int paramIndex, juce::String actionId, bool momentary);   // paramIndex -1: pure button (BROWSE)
    void paint(juce::Graphics&) override;
    void refresh() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; down = false; repaint(); }
    std::function<void()> onPress;                       // BROWSE
    int idx; juce::String id; bool mom;
private:
    bool hover = false, down = false, lit = false, blink = false; juce::uint32 flashUntil = 0; int cached = -1;
};

class OptionsW : public Ctrl {
public:
    OptionsW(Ctx& c, int paramIndex, const layout::Opts& o, juce::Array<const layout::Text*> labels);
    void paint(juce::Graphics&) override;
    void refresh() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override { hot = -1; repaint(); }
    int idx;
private:
    const layout::Opts& o; juce::Array<const layout::Text*> labels; int cached = -1, hot = -1;
    int hit(juce::Point<int> p) const;
};

// A <select>: a flat box with a chevron; the popup lists the options.
class SelectW : public Ctrl {
public:
    SelectW(Ctx& c, const layout::Select& s, int paramIndex, int mpIndex);
    void paint(juce::Graphics&) override;
    void refresh() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    bool keyPressed(const juce::KeyPress&) override;
    int idx, mp;
    // Shared with the edit pages: draw a select box at r with text.
    static void drawBox(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, const Look& L, const juce::Font& f, bool hover);
private:
    const layout::Select& s; juce::String text; bool hover = false;
    juce::String currentText() const;
};

class LcdW : public Ctrl {
public:
    LcdW(Ctx& c, const layout::Lcd& l, const layout::Text* t1, const layout::Text* t2);
    void paint(juce::Graphics&) override;
    void refresh() override;
private:
    const layout::Lcd& l; const layout::Text *t1, *t2; juce::String s1, s2;
};

// Block preset menus (mockup MPD): which entry of kMp matches the current values, or -1 (Custom).
int mpMatch(Controller& c, int mpIndex);
void mpApply(Controller& c, int mpIndex, int presetIndex);

// Popup menu look
void styleMenu(juce::LookAndFeel_V4& lf, const Look& L);

} // namespace jp
