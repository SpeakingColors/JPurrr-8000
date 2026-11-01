// The two small floating panels of mockup v3.10 / v3.11: EXT IN (top bar) and the MELODY GENERATOR (player bar).
// Everything is laid out from the mockup dump (layout::kPnExt, kPnGen: positions relative to the panel's corner).
#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace jp {

class PnItem;

class FloatPanel : public juce::Component, private juce::TextEditor::Listener {
public:
    enum Kind { kExt, kGen };
    FloatPanel(Kind k, JPurrr8000AudioProcessor& p, Ctx& ctx);
    ~FloatPanel() override;
    Kind kind() const { return kd; }
    bool isOpen() const { return isVisible(); }
    void open(juce::Point<int> topLeft);                  // Root coordinates (design pixels)
    void close();
    void refresh();                                       // 30 Hz while open: chips, number boxes, meter, info lines
    void paint(juce::Graphics&) override;
    bool hitTest(int x, int y) override { return inner().contains(x, y); }
    bool keyPressed(const juce::KeyPress&) override;
    void lookChanged() { repaint(); }
    juce::Rectangle<int> inner() const { return { kShadow, kShadow, innerW, innerH }; }
    const layout::PnGeo& geo() const { return *g; }

    std::function<void(juce::Component&, const juce::String&)> showTip;       // info bubble under a component
    std::function<void()> hideTip;
    std::function<void()> onClosed;
    std::function<void(juce::Component&, const juce::String&)> showBubble;    // value bubble while dragging a number box
    std::function<void()> hideBubble;
    std::function<void()> onGenerated;                    // GEN: a piece was written (the player bar repaints)

    // test hooks
    void generate(bool roll);                             // NEW MELODY (roll) or rewrite with the same seed
    juce::String infoLine() const;

    static constexpr int kShadow = 36;
    JPurrr8000AudioProcessor& proc;
    Ctx& x;

private:
    friend class PnItem;
    Kind kd; const layout::PnGeo* g; int innerW, innerH;
    juce::OwnedArray<PnItem> items;
    float meterL = -90.f, meterR = -90.f;
    juce::String lastSig;
    Controller& ctl() const { return x.c; }
    bool chipOn(const layout::PnBox& b) const;
    void chipClick(const layout::PnBox& b);
    int numGet(const layout::PnBox& b) const;
    void numSet(const layout::PnBox& b, int v);
    void numRange(const layout::PnBox& b, int& lo, int& hi, int& big) const;
    int numDef(const layout::PnBox& b) const;
    juce::String numText(const layout::PnBox& b, int v) const;
    juce::String dynText(const layout::PnLine& l) const;
    void buttonClick(const layout::PnBox& b);
    void paintMeter(juce::Graphics& g, juce::Rectangle<float> r);
};

} // namespace jp
