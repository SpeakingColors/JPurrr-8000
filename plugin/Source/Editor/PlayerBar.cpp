#include "PlayerBar.h"
#include "Displays/Common.h"
using namespace jp;
using namespace jp::layout;

namespace {
juce::Rectangle<float> R(const Rect& r) { return { r.x, r.y, r.w, r.h }; }
// browsers snap both edges of a box to device pixels
juce::Rectangle<float> snapR(const Rect& r) {
    const float x0 = std::round(r.x), y0 = std::round(r.y), x1 = std::round(r.x + r.w), y1 = std::round(r.y + r.h);
    return { x0, y0, x1 - x0, y1 - y0 };
}
juce::Rectangle<int> localBounds(const Rect& r) {          // bar-relative component bounds for a rectangle in (window x, bar y)
    const int x0 = juce::roundToInt(r.x - kPlayer.bar.x), y0 = juce::roundToInt(r.y);
    return { x0, y0, juce::roundToInt(r.x - kPlayer.bar.x + r.w) - x0, juce::roundToInt(r.y + r.h) - y0 };
}
void toBarFrame(juce::Graphics& g, const juce::Component& c) { g.addTransform(juce::AffineTransform::translation(-(kPlayer.bar.x + (float) c.getX()), -(float) c.getY())); }
float baseIn(float top, float h, const juce::Font& f) { return top + (h - (f.getAscent() + f.getDescent())) / 2.0f + f.getAscent(); }
juce::Colour darkText() { return juce::Colour(0xff10141a); }
const juce::Path& iconPath(int which) {                     // 0 play, 1 stop, 2 previous, 3 next (viewBox 0 0 14 14)
    static const juce::Path p[4] = { juce::Drawable::parseSVGPath("M2.5 1.5v11l9.5-5.5z"), juce::Drawable::parseSVGPath("M2.5 2.5h9v9h-9z"),
                                     juce::Drawable::parseSVGPath("M2 2h2v10H2zM12 2v10L5 7z"), juce::Drawable::parseSVGPath("M10 2h2v10h-2zM2 2v10l7-5z") };
    return p[which];
}
juce::String ellipsize(const juce::Font& f, juce::String s, float maxW) {
    if (textWidth(f, s) <= maxW) return s;
    const auto dots = juce::String::fromUTF8("\xe2\x80\xa6");
    while (s.length() > 1 && textWidth(f, s + dots) > maxW) s = s.dropLastCharacters(1);
    return s + dots;
}
// inset 0 0 6px rgba(0,0,0,.6) of the number boxes and the position canvas (layers as the canvas displays draw it)
void insetShadow(juce::Graphics& g, juce::Rectangle<float> box) {
    const float a[6] = { 0.26f, 0.19f, 0.12f, 0.07f, 0.04f, 0.02f };
    for (int d = 0; d < 6; ++d) { g.setColour(juce::Colours::black.withAlpha(a[d])); g.drawRect(box.reduced((float) d), 1.0f); }
}
}

namespace jp {

// ============================================================================================== buttons (.hb: PLAY, previous, next, LOAD MIDI FILE, EXPORT MIDI)
class PlBtn : public juce::Component {
public:
    enum Kind { kPlay, kPrev, kNext, kText };
    PlBtn(PlayerBar& b, Kind k, const Rect& r, juce::String id, juce::String text = {}) : bar(b), kind(k), rect(r), txt(std::move(text)) {
        setComponentID(id); setMouseCursor(juce::MouseCursor::PointingHandCursor); setWantsKeyboardFocus(true); setMouseClickGrabsKeyboardFocus(false);
        setBounds(localBounds(r));
    }
    std::function<void()> onClick;
    std::function<void()> onDragOut;                     // EXPORT: the pointer left with the button down
    void paint(juce::Graphics& g) override {
        toBarFrame(g, *this);
        const Look& L = bar.L(); const auto r = snapR(rect);
        const bool on = kind == kPlay && bar.playingShown();
        auto bg = on ? L[acc] : L[btnbg];
        if (hover && !on) bg = bg.brighter(0.25f);
        g.setColour(bg); g.fillRoundedRectangle(r, 4.0f);
        g.setColour(L[hi]); g.fillRect(juce::Rectangle<float>(r.getX() + 1, r.getY() + 1, r.getWidth() - 2, 1.0f));
        g.setColour(on ? L[acc] : L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
        if (ring) { g.setColour(L[acc]); g.drawRoundedRectangle(r.expanded(2.0f), 6.0f, 2.0f); }       // :focus-visible, outline 2 px, offset 1 px
        const juce::Colour fg = on ? darkText() : L[lbl];
        g.setColour(fg);
        auto icon = [&](int which, const Rect& ir) { g.fillPath(iconPath(which), juce::AffineTransform::translation(ir.x, ir.y)); };
        if (kind == kPlay) {
            icon(on ? 1 : 0, on ? kPlayer.stopIcon : kPlayer.playIcon);
            const PlLine& l = on ? kPlayer.stopL : kPlayer.playL;
            g.setFont(fontOf(kPlayer.play)); drawTextAt(g, on ? "STOP" : "PLAY", l.x, l.base, 'l');
        } else if (kind == kPrev) icon(2, kPlayer.prevIcon);
        else if (kind == kNext) icon(3, kPlayer.nextIcon);
        else {
            const PlLine& l = txt == "LOAD MIDI FILE" ? kPlayer.loadL : kPlayer.expL;
            g.setFont(fontOf(kPlayer.play)); drawTextAt(g, txt, l.x, l.base, 'l');
        }
    }
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) return; bar.hideTip(); pressed = true; dragged = false; }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!pressed || dragged || !onDragOut) return;
        const double sc = juce::Component::getApproximateScaleFactorForComponent(this);
        if (e.getDistanceFromDragStart() / sc > 6) { dragged = true; onDragOut(); }
    }
    void mouseUp(const juce::MouseEvent& e) override {
        const bool was = pressed && !dragged; pressed = false;
        if (was && getLocalBounds().contains(e.getPosition()) && !e.mods.isPopupMenu() && onClick) onClick();
    }
    bool keyPressed(const juce::KeyPress& k) override {
        if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) { if (onClick) onClick(); return true; }
        return false;
    }
    void focusGained(FocusChangeType t) override { ring = (t == focusChangedByTabKey); repaint(); }
    void focusLost(FocusChangeType) override { ring = false; repaint(); }
private:
    PlayerBar& bar; Kind kind; Rect rect; juce::String txt; bool hover = false, ring = false, pressed = false, dragged = false;
};

// ============================================================================================== the piece box (#plSel)
class PlSel : public juce::Component {
public:
    explicit PlSel(PlayerBar& b) : bar(b) { setComponentID("plSel"); setMouseCursor(juce::MouseCursor::PointingHandCursor); setWantsKeyboardFocus(true); setMouseClickGrabsKeyboardFocus(false); setBounds(localBounds(kPlayer.sel)); }
    void paint(juce::Graphics& g) override {
        toBarFrame(g, *this);
        const Look& L = bar.L(); const auto r = snapR(kPlayer.sel); const auto& host = bar.proc.midiPlayer;
        g.setColour(L[lcdbg]); g.fillRoundedRectangle(r, 3.0f);
        g.setColour(hover || bar.menuOpen() ? L[acc] : L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
        if (ring) { g.setColour(L[acc]); g.drawRoundedRectangle(r.expanded(2.0f), 5.0f, 2.0f); }
        // group (right, up to 260 px), then the name takes the rest; both with an ellipsis when too long
        const auto fg = fontOf(kPlayer.group), fn = fontOf(kPlayer.name);
        const juce::String grp = host.groupName().toUpperCase();
        const float gw = std::min(260.0f, textWidth(fg, grp)), gx = kPlayer.chev.x - 12.0f - gw;
        const float nx = kPlayer.nameR.x, nw = gx - 12.0f - nx;
        g.setColour(L[lcdfg]); g.setFont(fn); drawTextAt(g, ellipsize(fn, host.pieceName(), nw), nx, kPlayer.nameBase, 'l');
        g.setColour(L[lbl2]); g.setFont(fg); drawTextAt(g, ellipsize(fg, grp, gw), gx, kPlayer.grpBase, 'l');
        g.setColour(L[lbl]); g.fillPath(chev(), juce::AffineTransform::translation(kPlayer.chev.x, kPlayer.chev.y));
    }
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) bar.hideTip(); }
    void mouseUp(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu() || !getLocalBounds().contains(e.getPosition())) return;
        if (bar.menuOpen()) bar.closeMenu(); else bar.openMenu();
    }
    bool keyPressed(const juce::KeyPress& k) override {
        if (k == juce::KeyPress::downKey || k == juce::KeyPress::upKey || k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) { if (!bar.menuOpen()) bar.openMenu(); return true; }
        return false;
    }
    void focusGained(FocusChangeType t) override { ring = (t == focusChangedByTabKey); repaint(); }
    void focusLost(FocusChangeType) override { ring = false; repaint(); }
private:
    PlayerBar& bar; bool hover = false, ring = false;
    static const juce::Path& chev() { static const juce::Path p = juce::Drawable::parseSVGPath("M1 4h10L6 10z"); return p; }
};

// ============================================================================================== number boxes (#plBpm, #plTr)
class NumEdit : public juce::TextEditor {
public:
    std::function<bool(const juce::KeyPress&)> onKey;
    bool keyPressed(const juce::KeyPress& k) override { if (onKey && onKey(k)) return true; return juce::TextEditor::keyPressed(k); }
};

class PlNum : public juce::Component, private juce::TextEditor::Listener {
public:
    PlNum(PlayerBar& b, bool isBpm) : bar(b), bpm(isBpm) {
        setComponentID(isBpm ? "plBpm" : "plTr"); setMouseCursor(juce::MouseCursor::UpDownResizeCursor); setWantsKeyboardFocus(true); setMouseClickGrabsKeyboardFocus(false);
        setBounds(localBounds(isBpm ? kPlayer.bpmR : kPlayer.trR));
    }
    const Rect& rect() const { return bpm ? kPlayer.bpmR : kPlayer.trR; }
    float base() const { return bpm ? kPlayer.bpmBase : kPlayer.trBase; }
    int lo() const { return bpm ? 20 : -24; } int hi() const { return bpm ? 300 : 24; } int big() const { return bpm ? 5 : 12; }
    juce::String name() const { return bpm ? "BPM" : "TRANSPOSE"; }
    int get() const { auto& h = bar.proc.midiPlayer; return bpm ? juce::roundToInt(h.bpm()) : h.transpose(); }
    void set(int v) { auto& h = bar.proc.midiPlayer; v = juce::jlimit(lo(), hi(), v); if (bpm) h.setBpm((double) v); else h.setTranspose(v); }
    int def() const { return bpm ? juce::roundToInt(bar.proc.midiPlayer.pieceBpm()) : 0; }
    juce::String fmt(int v) const { return bpm ? juce::String(v) : (v > 0 ? "+" : "") + juce::String(v); }
    bool editing() const { return editor != nullptr; }

    void paint(juce::Graphics& g) override {
        toBarFrame(g, *this);
        const Look& L = bar.L(); const auto r = snapR(rect());
        g.setColour(L[lcdbg]); g.fillRoundedRectangle(r, 3.0f);
        insetShadow(g, r);
        g.setColour(hover || editing() ? L[acc] : L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
        if (editing() || ring) { g.setColour(L[acc]); g.drawRoundedRectangle(r.expanded(2.0f), 5.0f, 2.0f); }
        if (!editing()) { g.setColour(L[lcdfg]); g.setFont(fontOf(kPlayer.num)); drawTextAt(g, fmt(get()), r.getCentreX(), base(), 'c'); }
    }
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) return;
        bar.hideTip(); down = true; dragging = false; startY = e.getScreenY(); v0 = get();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!down) return;
        const double sc = juce::Component::getApproximateScaleFactorForComponent(this);
        const double dy = (double) (startY - e.getScreenY()) / sc;
        if (!dragging && std::abs(dy) > 3.0) { dragging = true; endEdit(true); }
        if (dragging) { set((int) std::lround((double) v0 + dy / (e.mods.isShiftDown() ? 8.0 : 2.0))); bubble(); repaint(); }
    }
    void mouseUp(const juce::MouseEvent& e) override {
        const bool wasDrag = dragging; down = false; dragging = false;
        if (wasDrag) { if (bar.hideBubble) bar.hideBubble(); return; }
        if (!editing() && !e.mods.isPopupMenu() && getLocalBounds().contains(e.getPosition())) startEdit();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override { set(def()); endEdit(false); repaint(); }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
        if (w.deltaY == 0) return;
        const int dir = (w.deltaY > 0) != w.isReversed ? 1 : -1;
        set(get() + dir * (e.mods.isShiftDown() ? big() : 1));
        if (editing()) editor->setText(juce::String(get()), false);
        bubble(); repaint();
        juce::Timer::callAfterDelay(700, [sp = juce::Component::SafePointer<PlNum>(this)] { if (sp && !sp->dragging && sp->bar.hideBubble) sp->bar.hideBubble(); });
    }
    void focusGained(FocusChangeType t) override { if (t == focusChangedByTabKey) startEdit(); }       // a text box: tabbing into it selects its text
    void resized() override { if (editor) editor->setBounds(getLocalBounds().reduced(5, 1)); }
    void stopEdit() { endEdit(false); }
private:
    PlayerBar& bar; bool bpm; bool hover = false, ring = false, down = false, dragging = false; int startY = 0, v0 = 0;
    std::unique_ptr<NumEdit> editor;
    void bubble() { if (bar.showBubble) bar.showBubble(*this, name() + "  " + fmt(get())); }
    void startEdit() {
        if (editor) return;
        editor = std::make_unique<NumEdit>();
        editor->setFont(fontOf(kPlayer.num)); editor->setMultiLine(false); editor->setBorder({}); editor->setIndents(0, 0);
        editor->setJustification(juce::Justification::centred); editor->setInputRestrictions(5, "0123456789+-");
        editor->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        editor->setColour(juce::TextEditor::textColourId, bar.L()[lcdfg]); editor->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        editor->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        editor->setColour(juce::TextEditor::highlightColourId, bar.L()[acc].withAlpha(0.5f));
        editor->setColour(juce::CaretComponent::caretColourId, bar.L()[lcdfg]);
        editor->setText(juce::String(get()), false);                     // the raw number while editing (no "+")
        editor->setBounds(getLocalBounds().reduced(5, 1)); editor->addListener(this);
        editor->onKey = [this](const juce::KeyPress& k) {
            const int c = k.getKeyCode();
            const bool up = c == juce::KeyPress::upKey || c == juce::KeyPress::pageUpKey, dn = c == juce::KeyPress::downKey || c == juce::KeyPress::pageDownKey;
            if (!up && !dn) return false;
            const bool page = c == juce::KeyPress::pageUpKey || c == juce::KeyPress::pageDownKey;
            const int st = (page || k.getModifiers().isShiftDown()) ? big() : 1;
            set(get() + (up ? st : -st)); editor->setText(juce::String(get()), false); editor->selectAll(); repaint(); return true;
        };
        addAndMakeVisible(*editor); editor->grabKeyboardFocus(); editor->selectAll(); repaint();
    }
    void endEdit(bool commit) {
        if (!editor) return;
        const auto t = editor->getText(); auto e = std::move(editor); editor.reset(); e->removeListener(this); removeChildComponent(e.get());
        if (commit) { const juce::String d = t.retainCharacters("0123456789+-"); if (d.containsAnyOf("0123456789")) set(d.getIntValue()); }
        repaint();
    }
    void textEditorReturnKeyPressed(juce::TextEditor&) override { endEdit(true); }
    void textEditorEscapeKeyPressed(juce::TextEditor&) override { endEdit(false); }
    void textEditorFocusLost(juce::TextEditor&) override { juce::MessageManager::callAsync([sp = juce::Component::SafePointer<PlNum>(this)] { if (sp) sp->endEdit(true); }); }
};

// ============================================================================================== LOCK (#plLockG)
class PlLock : public juce::Component {
public:
    explicit PlLock(PlayerBar& b) : bar(b) { setComponentID("plLock"); setMouseCursor(juce::MouseCursor::PointingHandCursor); setWantsKeyboardFocus(true); setMouseClickGrabsKeyboardFocus(false); setBounds(localBounds(kPlayer.lockG)); }
    void paint(juce::Graphics& g) override {
        toBarFrame(g, *this);
        const Look& L = bar.L(); const auto r = snapR(kPlayer.lockR); const bool on = bar.proc.midiPlayer.bpmLock();
        auto bg = L[btnbg]; if (hover) bg = bg.brighter(0.2f);
        g.setColour(bg); g.fillRoundedRectangle(r, 3.0f);
        g.setColour(L[hi]); g.fillRect(juce::Rectangle<float>(r.getX() + 1, r.getY() + 1, r.getWidth() - 2, 1.0f));
        g.setColour(L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
        if (ring) { g.setColour(L[acc]); g.drawRoundedRectangle(r.expanded(2.0f), 5.0f, 2.0f); }
        drawLed(g, R(kPlayer.led), on, L);
    }
    bool hitTest(int px, int py) override { (void) px; (void) py; return true; }
    void mouseMove(const juce::MouseEvent& e) override { const bool h = R(kPlayer.lockR).contains((float) e.x + (float) getX() + kPlayer.bar.x, (float) e.y + (float) getY()); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) bar.hideTip(); }
    void mouseUp(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu() && getLocalBounds().contains(e.getPosition())) toggle(); }
    bool keyPressed(const juce::KeyPress& k) override { if (k == juce::KeyPress::spaceKey || k == juce::KeyPress::returnKey) { toggle(); return true; } return false; }
    void focusGained(FocusChangeType t) override { ring = (t == focusChangedByTabKey); repaint(); }
    void focusLost(FocusChangeType) override { ring = false; repaint(); }
private:
    PlayerBar& bar; bool hover = false, ring = false;
    void toggle() { auto& h = bar.proc.midiPlayer; h.setBpmLock(!h.bpmLock()); repaint(); }
};

// ============================================================================================== position bar (#plPos, a canvas 590 x 26)
class PlPos : public disp::CanvasDisplay {
public:
    PlPos(PlayerBar& b, DisplayEnv& e) : disp::CanvasDisplay(e, kPlayer.posW, kPlayer.posH), bar(b) { setComponentID("plPos"); setInterceptsMouseClicks(false, false); }
    void tick() override { repaint(); }
protected:
    void draw(disp::Ctx& x, float w, float h) override {
        const auto v = bar.proc.midiPlayer.view(); if (v.n <= 0 && v.ev == nullptr) return;
        const double bt = v.beats > 0 ? v.beats : 4.0; const bool pl = bar.playingShown(); const float pos = bar.posBeats();
        auto X = [&](double b) { return (float) (b / bt * w); };
        x.fillStyle(env.pal.txt);
        for (int b = 0; b < (int) bt; ++b) { const bool br = b % 4 == 0; x.globalAlpha = br ? 0.55f : 0.35f; x.fillRect(std::round(X(b)), br ? 0.0f : h - 6, br ? 1.5f : 1.0f, br ? h : 6.0f); }
        x.globalAlpha = 1.0f;
        if (pl) { x.fillStyle(env.pal.fill); x.fillRect(0, 0, X(pos), h); }
        int mn = 127, mx = 0; for (int i = 0; i < v.n; ++i) { mn = std::min(mn, (int) v.ev[i].note); mx = std::max(mx, (int) v.ev[i].note); }
        const float rg = (float) std::max(1, mx - mn);
        for (int i = 0; i < v.n; ++i) {
            const auto& e = v.ev[i]; const bool on = pl && pos >= e.beat && pos < e.beat + e.len;
            const float y = 3 + (1 - (float) (e.note - mn) / rg) * (h - 10);
            x.fillStyle(on ? env.pal.acc : env.pal.line); x.globalAlpha = on ? 1.0f : 0.75f;
            x.fillRect(X(e.beat), y, std::max(2.0f, X(e.len) - 1), 3.0f);
        }
        x.globalAlpha = 1.0f;
        x.fillStyle(pl ? env.pal.acc : env.pal.txt); x.fillRect(std::min(w - 2, X(pos)), 0, 2.0f, h);
    }
private:
    PlayerBar& bar;
};

// ============================================================================================== the piece list (#plMenu)
class PlMenu : public juce::Component {
public:
    explicit PlMenu(PlayerBar& b) : bar(b) { setComponentID("plMenu"); setVisible(false); setPaintingIsUnclipped(true); setWantsKeyboardFocus(true); setMouseClickGrabsKeyboardFocus(true); }
    bool isOpen() const { return isVisible(); }
    struct Row { bool header; juce::String text, small; int idx; float y, h; };

    void open() {
        build();
        const auto& M = kPlayer.menu;
        const float selTop = bar.barY + kPlayer.sel.y;
        const float left = juce::jmax(M.left, juce::jmin(kPlayer.sel.x, (float) kDesignW - 34.0f - M.w - 4.0f));
        const float maxH = juce::jmax(M.minH, juce::jmin(M.maxH, selTop - M.minTop));
        const float h = juce::jmin(maxH, contentH + 2 * M.border);
        setBounds(juce::roundToInt(left), juce::roundToInt(selTop - h - M.above), juce::roundToInt(M.w), juce::roundToInt(h));
        viewH = (float) getHeight() - 2 * M.border;
        act = cur;
        scroll = 0;
        for (auto& r : rows) if (!r.header && r.idx == cur) { scroll = r.y - viewH / 2 + r.h / 2; break; }
        clampScroll();
        setVisible(true); toFront(false); grabKeyboardFocus(); repaint();
    }
    void close(bool refocus) { if (!isVisible()) return; setVisible(false); if (refocus) if (auto* s = bar.sel.get()) s->grabKeyboardFocus(); bar.repaint(); }

    void paint(juce::Graphics& g) override {
        const Look& L = bar.L(); const auto& M = kPlayer.menu; auto b = getLocalBounds().toFloat();
        juce::DropShadow(juce::Colours::black.withAlpha(0.6f), 30, { 0, 10 }).drawForRectangle(g, getLocalBounds());
        g.setColour(L[hdr]); g.fillRoundedRectangle(b, M.radius);
        g.saveState(); g.reduceClipRegion(getLocalBounds().reduced(juce::roundToInt(M.border)));
        const auto fr = fontOf(M.rowFont), fs = fontOf(M.rowSmall), fg = fontOf(M.groupFont), fgs = fontOf(M.groupSmall);
        const float inW = (float) getWidth() - 2 * M.border;
        auto ry = [&](const Row& r) { return M.border + r.y - scroll; };
        for (auto& r : rows) {                                                        // rows first, the group headers sit on top of them
            if (r.header) continue; const float y = ry(r); if (y + r.h < 0 || y > (float) getHeight()) continue;
            const bool isCur = r.idx == cur, isAct = r.idx == act;
            if (isCur) { g.setColour(L[btnbg]); g.fillRect(M.border, y, inW, r.h); }
            else if (isAct) { g.setColour(L[hi]); g.fillRect(M.border, y, inW, r.h); }
            g.setColour(L[edge]); g.fillRect(M.border, y + r.h - 1, inW, 1.0f);
            const float smW = textWidth(fs, r.small), base = std::round(baseIn(y, r.h - 1, fr)), baseS = std::round(baseIn(y, r.h - 1, fs)) + 1.0f;
            g.setColour(L[lbl2]); g.setFont(fs); drawTextAt(g, r.small, M.border + inW - M.rowPadR, baseS, 'r');
            g.setColour(isCur ? L[acc] : L[lbl]); g.setFont(fr);
            drawTextAt(g, ellipsize(fr, r.text, inW - M.rowPadL - M.rowPadR - M.rowGap - smW), M.border + M.rowPadL, base, 'l');
        }
        for (auto& r : rows) {
            if (!r.header) continue;
            const float y = M.border + juce::jmax(r.y - scroll, 0.0f);                // position: sticky; top: 0 (later headers cover earlier ones)
            g.setColour(L[pnl3]); g.fillRect(M.border, y, inW, r.h);
            g.setColour(L[edge]); g.fillRect(M.border, y + r.h - 1, inW, 1.0f);
            const float base = std::round(baseIn(y, r.h - 1, fg));
            g.setColour(L[lbl2]); g.setFont(fgs); drawTextAt(g, r.small, M.border + inW - M.groupPad, std::round(baseIn(y, r.h - 1, fgs)), 'r');
            g.setColour(L[ttl]); g.setFont(fg); drawTextAt(g, r.text.toUpperCase(), M.border + M.groupPad, base, 'l');
        }
        if (contentH > viewH && (hoverIn || dragBar)) {                               // a thin overlay scroll bar while the pointer is in the list
            const float th = std::max(24.0f, viewH * viewH / contentH), ty = M.border + (viewH - th) * (scroll / (contentH - viewH));
            g.setColour(L[lbl2].withAlpha(0.55f)); g.fillRoundedRectangle((float) getWidth() - M.border - 8.0f, ty, 6.0f, th, 3.0f);
        }
        g.restoreState();
        g.setColour(L[acc]); g.drawRoundedRectangle(b.reduced(0.5f), M.radius, M.border);
    }
    void mouseMove(const juce::MouseEvent& e) override { hoverIn = true; setAct(rowAt(e.position)); }
    void mouseEnter(const juce::MouseEvent&) override { hoverIn = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hoverIn = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override {
        bar.hideTip();
        if (contentH > viewH && e.position.x >= (float) getWidth() - 14.0f) { dragBar = true; dragY0 = e.position.y; dragS0 = scroll; return; }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!dragBar) return;
        const float th = std::max(24.0f, viewH * viewH / contentH);
        scroll = dragS0 + (e.position.y - dragY0) * (contentH - viewH) / std::max(1.0f, viewH - th); clampScroll(); repaint();
    }
    void mouseUp(const juce::MouseEvent& e) override {
        if (dragBar) { dragBar = false; repaint(); return; }
        const int i = rowAt(e.position); if (i >= 0 && getLocalBounds().contains(e.getPosition())) choose(i);
    }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { scroll -= w.deltaY * 480.0f * (w.isReversed ? -1.0f : 1.0f); clampScroll(); repaint(); }
    bool keyPressed(const juce::KeyPress& k) override {
        const int c = k.getKeyCode();
        if (c == juce::KeyPress::downKey) move(1); else if (c == juce::KeyPress::upKey) move(-1);
        else if (c == juce::KeyPress::pageDownKey) move(10); else if (c == juce::KeyPress::pageUpKey) move(-10);
        else if (c == juce::KeyPress::homeKey) move(-999); else if (c == juce::KeyPress::endKey) move(999);
        else if (c == juce::KeyPress::returnKey || c == juce::KeyPress::spaceKey) { if (act >= 0) pick(act); close(true); }
        else if (c == juce::KeyPress::escapeKey || c == juce::KeyPress::tabKey) close(true);
        else return false;
        return true;
    }
private:
    PlayerBar& bar; std::vector<Row> rows; float contentH = 0, viewH = 0, scroll = 0; int cur = -1, act = -1; bool hoverIn = false, dragBar = false; float dragY0 = 0, dragS0 = 0;
    void build() {
        rows.clear(); const auto& M = kPlayer.menu; float y = 0; auto& host = bar.proc.midiPlayer;
        const int n = jpurrr::libraryItemCount();
        auto header = [&](const juce::String& t, int count) { rows.push_back({ true, t, juce::String(count), -1, y, M.groupH }); y += M.groupH; };
        auto item = [&](const juce::String& t, int bpm, int idx) { rows.push_back({ false, t, juce::String(bpm), idx, y, M.rowH }); y += M.rowH; };
        for (int g = 0; g < jpurrr::libraryGroupCount(); ++g) {
            const auto& gr = jpurrr::libraryGroup(g); header(gr.name, gr.count);
            for (int k = 0; k < gr.count; ++k) { const auto& it = jpurrr::libraryItem(gr.first + k); item(it.name, it.bpm, gr.first + k); }
        }
        if (host.userCount() > 0) {
            header("Your MIDI files", host.userCount());
            for (int k = 0; k < host.userCount(); ++k) { const int s = host.userSlotAt(k); item(host.userName(s), juce::roundToInt(host.userBpm(s)), n + k); }
        }
        contentH = y;
        cur = host.selected() >= 0 ? host.selected() : -1;
        if (cur < 0 && host.fileLoaded()) for (int k = 0; k < host.userCount(); ++k) if (host.userSelected(host.userSlotAt(k))) cur = n + k;
    }
    void clampScroll() { scroll = juce::jlimit(0.0f, std::max(0.0f, contentH - viewH), scroll); }
    int rowAt(juce::Point<float> p) const {                                           // a row index, or -1 (also under a sticky header)
        const auto& M = kPlayer.menu; const float vy = p.y - M.border; if (vy < 0 || vy >= viewH) return -1;
        for (auto& r : rows) if (r.header) { const float y = juce::jmax(r.y - scroll, 0.0f); if (vy >= y && vy < y + r.h) return -1; }
        const float cy = vy + scroll;
        for (auto& r : rows) if (!r.header && cy >= r.y && cy < r.y + r.h) return r.idx;
        return -1;
    }
    void setAct(int i) { if (i >= 0 && i != act) { act = i; repaint(); } }
    void move(int d) {
        std::vector<const Row*> items; for (auto& r : rows) if (!r.header) items.push_back(&r);
        if (items.empty()) return;
        int k = -1; for (int i = 0; i < (int) items.size(); ++i) if (items[(size_t) i]->idx == act) k = i;
        k = juce::jlimit(0, (int) items.size() - 1, (k < 0 ? 0 : k) + d);
        act = items[(size_t) k]->idx; const Row& r = *items[(size_t) k];
        if (r.y < scroll) scroll = r.y; else if (r.y + r.h > scroll + viewH) scroll = r.y + r.h - viewH;               // scrollIntoView({block: 'nearest'})
        clampScroll(); repaint();
    }
    void pick(int idx) {
        auto& host = bar.proc.midiPlayer; const int n = jpurrr::libraryItemCount();
        if (idx < n) host.select(idx); else host.selectUser(host.userSlotAt(idx - n));
        bar.repaint();
    }
    void choose(int idx) { pick(idx); close(true); }
};

} // namespace jp

// ============================================================================================== PlayerBar
PlayerBar::PlayerBar(JPurrr8000AudioProcessor& p, Ctx& ctx, DisplayEnv& e) : proc(p), x(ctx), env(e), ctl(ctx.c) {
    setInterceptsMouseClicks(false, true);
    auto add = [&](juce::Component& c) { addAndMakeVisible(c); };
    play = std::make_unique<PlBtn>(*this, PlBtn::kPlay, kPlayer.playR, "plPlay"); play->onClick = [this] { togglePlay(); };
    prev = std::make_unique<PlBtn>(*this, PlBtn::kPrev, kPlayer.prevR, "plPrev"); prev->onClick = [this] { proc.midiPlayer.step(-1); repaint(); };
    next = std::make_unique<PlBtn>(*this, PlBtn::kNext, kPlayer.nextR, "plNext"); next->onClick = [this] { proc.midiPlayer.step(1); repaint(); };
    sel = std::make_unique<PlSel>(*this);
    bpm = std::make_unique<PlNum>(*this, true); tr = std::make_unique<PlNum>(*this, false);
    lock = std::make_unique<PlLock>(*this);
    load = std::make_unique<PlBtn>(*this, PlBtn::kText, kPlayer.loadR, "plLoad", "LOAD MIDI FILE"); load->onClick = [this] { doLoad(); };
    exp = std::make_unique<PlBtn>(*this, PlBtn::kText, kPlayer.expR, "plExp", "EXPORT MIDI"); exp->onClick = [this] { doExport(); };
    exp->onDragOut = [this] {                            // drag the button into the DAW: the file is written to a temporary folder first
        const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("JPurrr-8000-export"); dir.createDirectory();
        const auto f = exportToFile(dir); if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, exp.get());
    };
    pos = std::make_unique<PlPos>(*this, env); pos->setTopLeftPosition(juce::roundToInt(kPlayer.posR.x - kPlayer.bar.x), juce::roundToInt(kPlayer.posR.y));
    for (auto* c : std::initializer_list<juce::Component*> { play.get(), prev.get(), sel.get(), next.get(), bpm.get(), lock.get(), tr.get(), load.get(), exp.get(), pos.get() }) add(*c);
    menu = std::make_unique<PlMenu>(*this);
    setSize(juce::roundToInt(kPlayer.bar.w), juce::roundToInt(kPlayer.h));
}
PlayerBar::~PlayerBar() = default;

void PlayerBar::place(float y) {
    barY = y;
    setBounds(juce::roundToInt(kPlayer.bar.x), juce::roundToInt(y), juce::roundToInt(kPlayer.bar.w), juce::roundToInt(kPlayer.h));
}
juce::Component& PlayerBar::menuComponent() { return *menu; }
bool PlayerBar::menuOpen() const { return menu->isOpen(); }
void PlayerBar::closeMenu() { menu->close(false); sel->repaint(); }
void PlayerBar::openMenu() { if (menu->isOpen() || !isVisible()) return; hideTip(); ensurePiece(); menu->open(); sel->repaint(); }
bool PlayerBar::menuOwns(juce::Component* c) const { return c != nullptr && (c == menu.get() || menu->isParentOf(c) || c == sel.get()); }
void PlayerBar::stopAll() { proc.midiPlayer.setPlaying(false); demoOn = false; closeMenu(); }
bool PlayerBar::playingShown() const { return demoOn || proc.midiPlayer.isPlaying(); }
float PlayerBar::posBeats() const {
    const auto v = proc.midiPlayer.view(); const double bt = v.beats > 0 ? v.beats : 4.0;
    if (demoOn) return (float) std::fmod((double) demoBeat, bt);
    return proc.midiPlayer.isPlaying() ? (float) std::fmod((double) proc.midiPlayer.position(), bt) : 0.0f;
}
juce::String PlayerBar::posText() const {
    const auto v = proc.midiPlayer.view(); const double bt = v.beats > 0 ? v.beats : 4.0;
    return juce::String((int) std::floor(posBeats() / 4.0f) + 1) + " / " + juce::String((int) (bt / 4.0));
}
void PlayerBar::ensurePiece() {                          // the mockup starts on the first piece; a file of your own keeps its place
    auto& h = proc.midiPlayer;
    if (h.selected() >= 0 || h.fileLoaded()) return;
    const bool lk = h.bpmLock(); h.setBpmLock(true); h.select(0); h.setBpmLock(lk);
}
void PlayerBar::togglePlay() { demoOn = false; proc.midiPlayer.setPlaying(!proc.midiPlayer.isPlaying()); repaint(); }

void PlayerBar::paint(juce::Graphics& g) {
    g.addTransform(juce::AffineTransform::translation(-kPlayer.bar.x, 0.0f));
    const Look& l = L();
    const auto r = snapR(kPlayer.bar);
    g.setColour(l[pnl3]); g.fillRoundedRectangle(r, kPlayer.radius);
    g.setColour(l[hi]); g.fillRect(juce::Rectangle<float>(r.getX() + 6, r.getY() + 1, r.getWidth() - 12, 1.0f));
    g.setColour(l[sbd]); g.drawRoundedRectangle(r.reduced(0.5f), kPlayer.radius, 1.0f);
    g.setColour(l[ttl]); g.setFont(fontOf(kPlayer.title)); drawTextAt(g, "PLAYER", kPlayer.tTitle.x, kPlayer.tTitle.base, 'l');
    for (auto& s : kPlayer.seps) { g.setColour(l[edge]); g.fillRect(R(s)); g.setColour(l[hi]); g.fillRect(juce::Rectangle<float>(s.x + 1, s.y, 1.0f, s.h)); }     // width 1 px, box-shadow 1px 0 0 hi
    g.setColour(l[lbl2]); g.setFont(fontOf(kPlayer.label));
    drawTextAt(g, "BPM", kPlayer.bpmL.x, kPlayer.bpmL.base, 'l'); drawTextAt(g, "LOCK", kPlayer.lockL.x, kPlayer.lockL.base, 'l'); drawTextAt(g, "TRANSPOSE", kPlayer.trL.x, kPlayer.trL.base, 'l');
    g.setColour(l[lbl2]); g.setFont(fontOf(kPlayer.posSmall)); drawTextAt(g, "BAR", kPlayer.barL.x, kPlayer.barL.base, 'l');
    g.setColour(l[lbl]); g.setFont(fontOf(kPlayer.posVal)); drawTextAt(g, posText(), kPlayer.valL.x, kPlayer.valL.base, 'l');
}

void PlayerBar::refresh() {
    ensurePiece();
    auto& h = proc.midiPlayer;
    const juce::String sig = h.pieceName() + "|" + juce::String(h.bpm()) + "|" + juce::String(h.transpose()) + "|" + (h.bpmLock() ? "L" : "-") + "|" + (playingShown() ? "P" : "-") + "|" + posText();
    const bool changed = sig != lastSig; lastSig = sig;
    if (changed) { for (auto* c : std::initializer_list<juce::Component*> { play.get(), sel.get(), bpm.get(), tr.get(), lock.get() }) c->repaint(); repaint(juce::roundToInt(kPlayer.barL.x - kPlayer.bar.x), 0, 140, getHeight()); }
    if (playingShown() || changed) pos->repaint();
}
void PlayerBar::lookChanged() { repaint(); for (auto& c : getChildren()) c->repaint(); }

const Tip* PlayerBar::tipAt(juce::Point<float> p) {
    if (!isVisible()) return nullptr;
    int i = 0; const Tip* best = nullptr; float ba = 1e12f;
    for (auto& t : kPlayer.tips) {
        Tip& b = tipBuf[i++]; b = { { t.r.x, barY + t.r.y, t.r.w, t.r.h }, t.tip, "" };
        if (p.x >= b.r.x && p.x <= b.r.x + b.r.w && p.y >= b.r.y && p.y <= b.r.y + b.r.h) { const float a = b.r.w * b.r.h; if (a <= ba) { best = &b; ba = a; } }
    }
    return best;
}

// ------------------------------------------------------------------------------ LOAD and EXPORT
bool PlayerBar::loadTestFile(const juce::File& f) {
    juce::String err;
    if (!proc.midiPlayer.loadFile(f, err)) { say("That MIDI file could not be read (" + err.trimCharactersAtEnd(".") + ")."); return false; }
    demoOn = false;
    say(proc.midiPlayer.pieceName().endsWith("(cut)") ? juce::String("MIDI file loaded. Only the first 8000 notes are used.") : "MIDI file loaded: " + f.getFileName());
    repaint(); return true;
}
void PlayerBar::doLoad() {
    chooser = std::make_unique<juce::FileChooser>("Load a MIDI file", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.mid;*.midi");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [sp = juce::Component::SafePointer<PlayerBar>(this)](const juce::FileChooser& fc) {
        const auto f = fc.getResult(); if (sp != nullptr && f != juce::File()) sp->loadTestFile(f);
    });
}
juce::String PlayerBar::midiFileName(const juce::String& name) {
    juce::String out; bool space = false;
    for (auto c : (name.isEmpty() ? juce::String("piece") : name)) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '#' || c == '-';
        if (ok) { if (space && out.isNotEmpty()) out << ' '; space = false; out << juce::String::charToString(c); } else space = true;
    }
    // the mockup keeps spaces that were typed as such: runs of anything else, and white space, become one space
    return out.substring(0, 60) + ".mid";
}
juce::File PlayerBar::exportToFile(const juce::File& dir) {
    auto& h = proc.midiPlayer; const auto v = h.view(); if (v.ev == nullptr || v.n <= 0) return {};
    const auto bytes = jpurrr::writeMidiFile(v, std::round(h.bpm()), h.transpose(), ("JPurrr-8000 " + h.pieceName()).toStdString());
    const auto f = dir.getChildFile(midiFileName(h.pieceName()));
    if (!f.replaceWithData(bytes.data(), bytes.size())) return {};
    return f;
}
void PlayerBar::doExport() {
    auto& h = proc.midiPlayer; const auto v = h.view(); if (v.ev == nullptr || v.n <= 0) { say("Export failed"); return; }
    chooser = std::make_unique<juce::FileChooser>("Save the piece as a MIDI file", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(midiFileName(h.pieceName())), "*.mid");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [sp = juce::Component::SafePointer<PlayerBar>(this)](const juce::FileChooser& fc) {
        auto f = fc.getResult(); if (sp == nullptr || f == juce::File()) return;
        if (f.getFileExtension().isEmpty()) f = f.withFileExtension("mid");
        auto& h2 = sp->proc.midiPlayer; const auto v2 = h2.view(); if (v2.ev == nullptr || v2.n <= 0) return;
        const auto bytes = jpurrr::writeMidiFile(v2, std::round(h2.bpm()), h2.transpose(), ("JPurrr-8000 " + h2.pieceName()).toStdString());
        sp->say(f.replaceWithData(bytes.data(), bytes.size()) ? "Exported " + f.getFileName() : juce::String("Export failed"));
    });
}

// ------------------------------------------------------------------------------ test hooks
void PlayerBar::demo(int libraryIndex, float beat) {
    proc.midiPlayer.setPlaying(false); if (libraryIndex >= 0) proc.midiPlayer.select(libraryIndex);
    demoOn = true; demoBeat = beat; lastSig = {}; repaint(); pos->repaint();
}
void PlayerBar::stopDemo() { demoOn = false; proc.midiPlayer.setPlaying(false); lastSig = {}; repaint(); pos->repaint(); }
