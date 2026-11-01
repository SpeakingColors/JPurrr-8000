#include "Panels.h"
#include "../PlayerHost.h"
#include <jpurrr/melody_gen.h>

using namespace jp::layout;
using jpurrr::P_o2ext;

namespace jp {

namespace {
// inset 0 0 6px rgba(0,0,0,.6) of the number boxes and the signal canvas
void insetShadow(juce::Graphics& g, juce::Rectangle<float> box) {
    const float a[6] = { 0.26f, 0.19f, 0.12f, 0.07f, 0.04f, 0.02f };
    for (int d = 0; d < 6; ++d) { g.setColour(juce::Colours::black.withAlpha(a[d])); g.drawRect(box.reduced((float) d), 1.0f); }
}
juce::Colour darkText() { return juce::Colour(0xff10141a); }
juce::Path dicePath() {                                   // the GEN die (svg 14 x 14): rounded square outline 1.4 px and three pips
    juce::Path p; p.addRoundedRectangle(1.5f, 1.5f, 11.0f, 11.0f, 2.2f);
    juce::Path o; juce::PathStrokeType(1.4f).createStrokedPath(o, p);
    for (auto c : { juce::Point<float>(4.6f, 4.6f), juce::Point<float>(9.4f, 9.4f), juce::Point<float>(7.f, 7.f) }) o.addEllipse(c.x - 1.1f, c.y - 1.1f, 2.2f, 2.2f);
    return o;
}
class NumEdit : public juce::TextEditor {
public:
    std::function<bool(const juce::KeyPress&)> onKey;
    bool keyPressed(const juce::KeyPress& k) override { if (onKey && onKey(k)) return true; return juce::TextEditor::keyPressed(k); }
};
}

// ============================================================================================== one control of a panel
class PnItem : public juce::Component, private juce::Timer, private juce::TextEditor::Listener {
public:
    PnItem(FloatPanel& p, const PnBox& bx) : panel(p), b(bx) {
        setMouseCursor(b.kind == 'n' ? juce::MouseCursor::UpDownResizeCursor : b.kind == 'v' ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
        setComponentID(b.id);
        const int x0 = juce::roundToInt(b.r.x), y0 = juce::roundToInt(b.r.y);
        setBounds(FloatPanel::kShadow + x0, FloatPanel::kShadow + y0, juce::roundToInt(b.r.x + b.r.w) - x0, juce::roundToInt(b.r.y + b.r.h) - y0);
    }
    const PnBox& b;
    void paint(juce::Graphics& g) override {
        const Look& L = panel.x.L();
        g.addTransform(juce::AffineTransform::translation(-(float) (getX() - FloatPanel::kShadow), -(float) (getY() - FloatPanel::kShadow)));   // panel coordinates
        const juce::Rectangle<float> r(std::round(b.r.x), std::round(b.r.y), std::round(b.r.x + b.r.w) - std::round(b.r.x), std::round(b.r.y + b.r.h) - std::round(b.r.y));
        if (b.kind == 'c') {
            const bool on = panel.chipOn(b);
            auto bg = on ? L[acc] : L[sel]; if (hover) bg = bg.brighter(0.25f);
            g.setColour(bg); g.fillRoundedRectangle(r, 4.0f);
            g.setColour(on ? L[acc] : L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
            g.setColour(on ? darkText() : L[lbl]); g.setFont(fontOf(b.f)); drawTextAt(g, b.text, b.line.x, b.line.base, 'l');
        } else if (b.kind == 'n') {
            g.setColour(L[lcdbg]); g.fillRoundedRectangle(r, 3.0f); insetShadow(g, r);
            g.setColour(hover || editor != nullptr ? L[acc] : L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
            if (editor == nullptr) { g.setColour(L[lcdfg]); g.setFont(fontOf(b.f)); drawTextAt(g, panel.numText(b, panel.numGet(b)), r.getCentreX(), b.base, 'c'); }
        } else if (b.kind == 'b') {
            const bool pressedLook = down && hover;
            auto bg = L[btnbg]; if (hover && !pressedLook) bg = bg.brighter(0.25f);
            g.setColour(bg); g.fillRoundedRectangle(r, 4.0f);
            g.setColour(L[hi]); g.fillRect(juce::Rectangle<float>(r.getX() + 1, r.getY() + 1, r.getWidth() - 2, 1.0f));
            g.setColour(L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
            g.setColour(L[lbl]); g.fillPath(dicePath(), juce::AffineTransform::translation(b.icon.x, b.icon.y));
            g.setFont(fontOf(b.f)); drawTextAt(g, b.text, b.line.x, b.line.base, 'l');
        } else if (b.kind == 'v') {
            panel.paintMeter(g, r);
        }
    }
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); if (b.tip[0] && Settings::get().bubbles) startTimer(1000); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; stopTimer(); repaint(); }
    void mouseDown(const juce::MouseEvent& e) override {
        stopTimer(); if (panel.hideTip) panel.hideTip();
        if (e.mods.isPopupMenu() || b.kind == 'v') return;
        down = true; dragging = false; startY = e.getScreenY(); v0 = b.kind == 'n' ? panel.numGet(b) : 0; repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!down || b.kind != 'n') return;
        const double sc = juce::Component::getApproximateScaleFactorForComponent(this);
        const double dy = (double) (startY - e.getScreenY()) / sc;
        if (!dragging && std::abs(dy) > 3.0) { dragging = true; endEdit(false); }
        if (dragging) { panel.numSet(b, (int) std::lround((double) v0 + dy / (e.mods.isShiftDown() ? 8.0 : 2.0))); bubble(); repaint(); }
    }
    void mouseUp(const juce::MouseEvent& e) override {
        const bool wasDrag = dragging, was = down; down = false; dragging = false; repaint();
        if (!was) return;
        if (wasDrag) { if (panel.hideBubble) panel.hideBubble(); return; }
        if (!getLocalBounds().contains(e.getPosition()) || e.mods.isPopupMenu()) return;
        if (b.kind == 'c') panel.chipClick(b);
        else if (b.kind == 'b') panel.buttonClick(b);
        else if (b.kind == 'n' && editor == nullptr) startEdit();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override { if (b.kind == 'n') { endEdit(false); panel.numSet(b, panel.numDef(b)); repaint(); } }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
        if (b.kind != 'n' || w.deltaY == 0) return;
        int lo, hi, big; panel.numRange(b, lo, hi, big);
        const int dir = (w.deltaY > 0) != w.isReversed ? 1 : -1;
        panel.numSet(b, panel.numGet(b) + dir * (e.mods.isShiftDown() ? big : 1));
        if (editor) editor->setText(juce::String(panel.numGet(b)), false);
        bubble(); repaint();
        juce::Timer::callAfterDelay(700, [sp = juce::Component::SafePointer<PnItem>(this)] { if (sp && !sp->dragging && sp->panel.hideBubble) sp->panel.hideBubble(); });
    }
    void resized() override { if (editor) editor->setBounds(getLocalBounds().reduced(5, 1)); }
    void stopEdit() { endEdit(false); }
private:
    FloatPanel& panel; bool hover = false, down = false, dragging = false; int startY = 0, v0 = 0;
    std::unique_ptr<NumEdit> editor;
    void timerCallback() override { stopTimer(); if (hover && !down && panel.showTip) panel.showTip(*this, juce::String::fromUTF8(b.tip)); }
    void bubble() { if (panel.showBubble) panel.showBubble(*this, juce::String(b.id) == "pgSeed" ? "SEED  " + juce::String(panel.numGet(b)) : "INPUT LEVEL  " + panel.numText(b, panel.numGet(b))); }
    void startEdit() {
        if (editor) return;
        const Look& L = panel.x.L();
        editor = std::make_unique<NumEdit>();
        editor->setFont(fontOf(b.f)); editor->setMultiLine(false); editor->setBorder({}); editor->setIndents(0, 0);
        editor->setJustification(juce::Justification::centred); editor->setInputRestrictions(8, juce::String(b.id) == "pgSeed" ? "0123456789" : "0123456789+-");
        editor->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        editor->setColour(juce::TextEditor::textColourId, L[lcdfg]); editor->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        editor->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        editor->setColour(juce::TextEditor::highlightColourId, L[acc].withAlpha(0.5f)); editor->setColour(juce::CaretComponent::caretColourId, L[lcdfg]);
        editor->setText(juce::String(panel.numGet(b)), false);
        editor->setBounds(getLocalBounds().reduced(5, 1)); editor->addListener(this);
        editor->onKey = [this](const juce::KeyPress& k) {
            const int c = k.getKeyCode(); int lo, hi, big; panel.numRange(b, lo, hi, big);
            const bool up = c == juce::KeyPress::upKey || c == juce::KeyPress::pageUpKey, dn = c == juce::KeyPress::downKey || c == juce::KeyPress::pageDownKey;
            if (!up && !dn) return false;
            const bool page = c == juce::KeyPress::pageUpKey || c == juce::KeyPress::pageDownKey;
            const int st = (page || k.getModifiers().isShiftDown()) ? big : 1;
            panel.numSet(b, panel.numGet(b) + (up ? st : -st)); editor->setText(juce::String(panel.numGet(b)), false); editor->selectAll(); repaint(); return true;
        };
        addAndMakeVisible(*editor); editor->grabKeyboardFocus(); editor->selectAll(); repaint();
    }
    void endEdit(bool commit) {
        if (!editor) return;
        const auto t = editor->getText(); auto e = std::move(editor); editor.reset(); e->removeListener(this); removeChildComponent(e.get());
        if (commit) { const juce::String d = t.retainCharacters("0123456789+-"); if (d.containsAnyOf("0123456789")) panel.numSet(b, d.getIntValue()); }
        repaint();
    }
    void textEditorReturnKeyPressed(juce::TextEditor&) override { endEdit(true); }
    void textEditorEscapeKeyPressed(juce::TextEditor&) override { endEdit(false); }
    void textEditorFocusLost(juce::TextEditor&) override { juce::MessageManager::callAsync([sp = juce::Component::SafePointer<PnItem>(this)] { if (sp) sp->endEdit(true); }); }
};

// ============================================================================================== the panel
FloatPanel::FloatPanel(Kind k, JPurrr8000AudioProcessor& p, Ctx& ctx) : proc(p), x(ctx), kd(k), g(k == kExt ? &kPnExt : &kPnGen) {
    innerW = juce::roundToInt(g->r.w); innerH = juce::roundToInt(g->r.h);
    setSize(innerW + 2 * kShadow, innerH + 2 * kShadow);
    setWantsKeyboardFocus(true); setMouseClickGrabsKeyboardFocus(false);
    for (auto& b : g->boxes) { auto* it = new PnItem(*this, b); items.add(it); addAndMakeVisible(it); }
    setVisible(false);
}
FloatPanel::~FloatPanel() = default;

void FloatPanel::open(juce::Point<int> tl) {
    setTopLeftPosition(tl.x - kShadow, tl.y - kShadow);
    setVisible(true); toFront(false); refresh(); repaint();
}
void FloatPanel::close() {
    if (!isVisible()) return;
    for (auto* it : items) it->stopEdit();
    setVisible(false); if (hideTip) hideTip(); if (hideBubble) hideBubble(); if (onClosed) onClosed();
}
bool FloatPanel::keyPressed(const juce::KeyPress& k) { if (k == juce::KeyPress::escapeKey) { close(); return true; } return false; }

bool FloatPanel::chipOn(const PnBox& b) const {
    const juce::String id = b.id; auto& S = Settings::get();
    if (id == "exSrc") return (ctl().get(P_o2ext) != 0) == (b.v == 1);
    if (id == "exIn") return S.extIn == b.v;
    if (id == "pgKey") return S.genKey == b.v;
    if (id == "pgMode") return S.genMode == b.v;
    if (id == "pgStyle") return S.genStyle == b.v;
    return false;
}
void FloatPanel::chipClick(const PnBox& b) {
    const juce::String id = b.id; auto& S = Settings::get();
    if (id == "exSrc") { ctl().begin(P_o2ext); ctl().user(P_o2ext, b.v); ctl().end(P_o2ext); }
    else if (id == "exIn") { S.extIn = b.v; S.save(); proc.setExtInput(S.extIn, S.extGain); }
    else if (id == "pgKey") { S.genKey = b.v; generate(false); }
    else if (id == "pgMode") { S.genMode = b.v; generate(false); }
    else if (id == "pgStyle") { S.genStyle = b.v; generate(false); }
    repaint();
}
void FloatPanel::numRange(const PnBox& b, int& lo, int& hi, int& big) const {
    if (juce::String(b.id) == "pgSeed") { lo = 1; hi = 16777215; big = 1000; } else { lo = -24; hi = 24; big = 6; }
}
int FloatPanel::numGet(const PnBox& b) const {
    auto& S = Settings::get();
    if (juce::String(b.id) == "pgSeed") return S.genSeed > 0 ? S.genSeed : 1;
    return S.extGain;
}
int FloatPanel::numDef(const PnBox& b) const { return juce::String(b.id) == "pgSeed" ? numGet(b) : 0; }
juce::String FloatPanel::numText(const PnBox& b, int v) const {
    if (juce::String(b.id) == "pgSeed") return juce::String(v);
    return (v > 0 ? "+" : "") + juce::String(v) + " dB";
}
void FloatPanel::numSet(const PnBox& b, int v) {
    int lo, hi, big; numRange(b, lo, hi, big); v = juce::jlimit(lo, hi, v); auto& S = Settings::get();
    if (juce::String(b.id) == "pgSeed") { if (v == S.genSeed) return; S.genSeed = v; generate(false); }
    else { S.extGain = v; S.save(); proc.setExtInput(S.extIn, S.extGain); }
    repaint();
}
void FloatPanel::buttonClick(const PnBox& b) { if (juce::String(b.id) == "pgNew") generate(true); }

void FloatPanel::generate(bool roll) {
    auto& S = Settings::get();
    if (roll || S.genSeed <= 0) S.genSeed = 1 + juce::Random::getSystemRandom().nextInt(16777215);
    const int key = S.genKey >= 0 ? S.genKey : (int) (((long long) S.genSeed * 7 + 3) % 12);
    const jpurrr::MelodyGenResult r = jpurrr::melodyGenerate(S.genSeed, key, S.genMode, S.genStyle);
    if (!r.ok) { ctl().say("The melody generator could not write that one"); return; }
    proc.midiPlayer.addGenerated(r);
    if (roll && !proc.midiPlayer.isPlaying()) proc.midiPlayer.setPlaying(true);
    S.save(); if (onGenerated) onGenerated();
    repaint();
}

juce::String FloatPanel::infoLine() const {
    auto& h = proc.midiPlayer;
    for (int s = 0; s < PlayerHost::kGenMax; ++s) {
        if (!h.genSelected(s)) continue;
        const auto* m = h.genInfo(s); if (m == nullptr) return {};
        juce::StringArray w; w.addTokens(juce::String(m->chords), " ", ""); w.removeEmptyStrings();
        juce::String cs; for (int i = 0; i < juce::jmin(4, w.size()); ++i) cs << (i ? " " : "") << w[i];
        return cs + " ... , " + juce::String(jpurrr::melodyStyleName(m->style)).toUpperCase() + ", " + juce::String(m->n) + " notes, " + juce::String(juce::roundToInt(m->bpm)) + " BPM";
    }
    return {};
}
juce::String FloatPanel::dynText(const PnLine& l) const {
    const juce::String id = l.id;
    if (id == "pgInfo") return infoLine();
    if (id == "exInN") return Settings::get().extIn ? juce::String("Needs a host that feeds audio to an instrument.") : juce::String::fromUTF8("Pick the track in your DAW\xe2\x80\x99s sidechain menu.");
    if (id == "exSt") return (meterL > -60.f || meterR > -60.f) ? "SIGNAL" : "NO SIGNAL";
    return juce::String::fromUTF8(l.t);
}

void FloatPanel::refresh() {
    if (!isVisible()) return;
    auto dB = [](float a) { return a > 1.0e-4f ? juce::jmax(-90.0f, 20.0f * std::log10(a)) : -90.0f; };
    const float l = dB(proc.extMeterL.load(std::memory_order_relaxed)), r = dB(proc.extMeterR.load(std::memory_order_relaxed));
    const juce::String sig = juce::String(juce::roundToInt(l)) + "/" + juce::String(juce::roundToInt(r)) + "/" + infoLine() + "/" + juce::String(Settings::get().extIn) + juce::String(ctl().get(P_o2ext));
    meterL = l; meterR = r;
    if (sig != lastSig) { lastSig = sig; repaint(); }
}

void FloatPanel::paintMeter(juce::Graphics& gr, juce::Rectangle<float> r) {
    const Look& L = x.L(); const Palette pal = paletteOf(L);
    gr.setColour(pal.bg); gr.fillRoundedRectangle(r, 2.0f); insetShadow(gr, r);
    gr.setColour(pal.edge); gr.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);
    const float ox = r.getX() + 1.0f, oy = r.getY() + 1.0f, w = 430.0f, h = 44.0f, x0 = 26.0f, x1 = w - 6.0f;
    auto X = [&](float db) { return x0 + (db + 60.0f) / 60.0f * (x1 - x0); };
    const juce::Font mono = fontOf(kMono, 15.5f, 400);
    const float lv[2] = { meterL, meterR };
    for (int i = 0; i < 2; ++i) {
        gr.setColour(pal.txt); gr.setFont(mono); drawTextAt(gr, i ? "R" : "L", ox + 4.0f, oy + 10.0f + (float) i * 14.0f + 8.0f, 'l');
        const juce::Rectangle<float> bar(ox + x0, oy + 5.0f + (float) i * 14.0f, x1 - x0, 11.0f);
        gr.setColour(pal.bg); gr.fillRect(bar);
        if (lv[i] > -60.0f) {
            const float xe = ox + X(juce::jmin(0.0f, lv[i]));
            gr.setColour(lv[i] > -6.0f ? pal.acc : pal.line); gr.fillRect(juce::Rectangle<float>(bar.getX(), bar.getY(), xe - bar.getX(), bar.getHeight()));
        }
        gr.setColour(pal.grid); gr.drawRect(juce::Rectangle<float>(bar.getX() + 0.5f, bar.getY() + 0.5f, bar.getWidth() - 1.0f, 10.0f), 1.0f);
    }
    for (int db : { -60, -48, -36, -24, -12, 0 }) {
        const float tx = std::round(ox + X((float) db)) + 0.5f;
        gr.setColour(pal.grid); gr.drawLine(tx, oy + 5.0f, tx, oy + 34.0f, 1.0f);
        gr.setColour(pal.txt); gr.setFont(mono);
        drawTextAt(gr, db == 0 ? juce::String("0 dB") : juce::String(db), ox + juce::jmin(w - 14.0f, juce::jmax(x0 + 8.0f, X((float) db))), oy + h - 3.0f, 'c');
    }
}

void FloatPanel::paint(juce::Graphics& gr) {
    const Look& L = x.L(); const auto in = inner().toFloat();
    juce::DropShadow(juce::Colours::black.withAlpha(0.6f), 30, { 0, 10 }).drawForRectangle(gr, in.toNearestInt());
    gr.setColour(L[hdr]); gr.fillRoundedRectangle(in, 6.0f);
    gr.setColour(L[acc]); gr.drawRoundedRectangle(in.reduced(0.5f), 6.0f, 1.0f);
    gr.addTransform(juce::AffineTransform::translation((float) kShadow, (float) kShadow));
    gr.setColour(L[edge]); gr.fillRect(juce::Rectangle<float>(g->sep.x, g->sep.y, g->sep.w, g->sep.h));
    for (auto& l : g->lines) {
        gr.setColour(L.c[l.tok]); gr.setFont(fontOf(l.f));
        if (l.id[0]) {
            gr.saveState(); gr.reduceClipRegion(juce::Rectangle<float>(l.el.x, l.el.y, l.el.w, l.el.h).toNearestInt());
            drawTextAt(gr, dynText(l), l.x, l.base, 'l'); gr.restoreState();
        } else drawTextAt(gr, l.t, l.x, l.base, 'l');
    }
}

} // namespace jp
