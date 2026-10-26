#include "Widgets.h"
#include "Fonts.h"
using namespace jp;
using namespace jp::layout;
using namespace jpurrr;

namespace {
constexpr double kKnobPixels = 170.0;       // mockup: a full turn takes 170 design px
double span(int i) { return (double) (kParams[i].max - kParams[i].min); }
float norm(int i, int v) { return (float) (v - kParams[i].min) / (float) (kParams[i].max - kParams[i].min); }

struct MenuLnF : juce::LookAndFeel_V4 {
    const Look* L = &lookByIndex(0);
    juce::Font getPopupMenuFont() override { return fontOf(kCond, 17.5f, 600, 0.04f); }
    void drawPopupMenuBackground(juce::Graphics& g, int w, int h) override {
        g.fillAll((*L)[hdr]); g.setColour((*L)[acc]); g.drawRect(0, 0, w, h, 1);
    }
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*) override {
        (void) hasSubMenu;
        if (isSeparator) { g.setColour((*L)[edge]); g.fillRect(area.reduced(6, 0).withHeight(1).translated(0, area.getHeight() / 2)); return; }
        if (isHighlighted && isActive) { g.setColour((*L)[acc]); g.fillRect(area); }
        g.setColour(isHighlighted && isActive ? juce::Colour(0xff10141a) : isActive ? (*L)[lbl] : (*L)[lbl2]);
        g.setFont(getPopupMenuFont());
        g.drawText(text, area.reduced(18, 0), juce::Justification::centredLeft, true);
        if (isTicked) { g.fillEllipse((float) area.getX() + 7.0f, (float) area.getCentreY() - 2.5f, 5.0f, 5.0f); }
    }
    int getPopupMenuBorderSize() override { return 2; }
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardHeight, int& idealWidth, int& idealHeight) override {
        if (isSeparator) { idealWidth = 50; idealHeight = 6; return; }
        idealHeight = 26; idealWidth = (int) jp::textWidth(getPopupMenuFont(), text) + 40; (void) standardHeight;
    }
};
MenuLnF& menuLnF() { static MenuLnF l; return l; }
}

void jp::styleMenu(juce::LookAndFeel_V4& lf, const Look& L) { (void) lf; (void) L; }
juce::LookAndFeel_V4& jp::popupLnF(const Look& L) { auto& l = menuLnF(); l.L = &L; return l; }

// =========================================================================================== knob
KnobW::KnobW(Ctx& c, int i) : Ctrl(c), idx(i) {
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setWantsKeyboardFocus(true); setMouseClickGrabsKeyboardFocus(false);
}
void KnobW::refresh() { const int v = x.c.get(idx); if (v != cached) { cached = v; repaint(); } }

void KnobW::paint(juce::Graphics& g) {
    const Look& L = x.L();
    const float n = norm(idx, x.c.get(idx));
    // tick ring: 11 dots, lit up to the current value
    for (int i = 0; i < 11; ++i) {
        const float a = juce::degreesToRadians(-135.0f + 27.0f * (float) i);
        const float cx = 24.0f + 20.5f * std::sin(a), cy = 24.0f - 20.5f * std::cos(a);
        const bool on = (float) i / 10.0f <= n + 0.001f && n > 0.0f;
        g.setColour(on ? L[acc] : L[lbl2]);
        g.fillEllipse(cx - 1.35f, cy - 1.35f, 2.7f, 2.7f);
    }
    // cap: radial-gradient(circle at 35% 28%, knobhi, knob 62%), shadow 0 2px 3px rgba(0,0,0,.6), ring
    const juce::Rectangle<float> body(9.0f, 9.0f, 30.0f, 30.0f);
    juce::Path bp; bp.addEllipse(body);
    juce::DropShadow(juce::Colours::black.withAlpha(0.6f), 3, { 0, 2 }).drawForPath(g, bp);
    const juce::Point<float> gc(body.getX() + 10.5f, body.getY() + 8.4f);
    g.setGradientFill(juce::ColourGradient(L[knobhi], gc, L[knob], gc.translated(29.1f * 0.62f, 0), true));
    g.fillEllipse(body);
    g.setColour(juce::Colours::white.withAlpha(0.1f)); g.drawEllipse(body.reduced(0.5f), 1.0f);
    // indicator
    juce::Path ind; ind.addRoundedRectangle(23.0f, 12.0f, 2.0f, 10.0f, 1.0f);
    g.setColour(L[jp::layout::ind]);
    g.fillPath(ind, juce::AffineTransform::rotation(juce::degreesToRadians(-135.0f + 270.0f * n), 24.0f, 24.0f));
    if (hasKeyboardFocus(false)) { g.setColour(L[acc]); g.drawEllipse(getLocalBounds().toFloat().reduced(0.5f), 2.0f); }
}

void KnobW::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) return;
    x.hideTip();
    if (x.c.assigning != Controller::kNone) x.c.controlTouched(idx);
    dragging = true; dragStartV = x.c.get(idx); dragStartY = e.getScreenY();
    x.c.begin(idx);
    x.showVal(*this, idx);
}
void KnobW::mouseDrag(const juce::MouseEvent& e) {
    if (!dragging) return;
    const double fine = e.mods.isShiftDown() ? 0.2 : 1.0;
    // screen pixels to design pixels: the editor is scaled by a transform, so ask the component for its scale
    const double sc = juce::Component::getApproximateScaleFactorForComponent(this) * 1.0;
    const double dv = -((double) (e.getScreenY() - dragStartY)) / sc / kKnobPixels * span(idx) * fine;
    x.c.user(idx, (int) std::lround(dragStartV + dv));
    x.showVal(*this, idx); refresh();
}
void KnobW::mouseUp(const juce::MouseEvent&) { if (dragging) { dragging = false; x.c.end(idx); x.hideVal(); } }
void KnobW::mouseDoubleClick(const juce::MouseEvent&) { x.c.begin(idx); x.c.user(idx, kParams[idx].def); x.c.end(idx); refresh(); }
void KnobW::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    const int step = e.mods.isShiftDown() ? 1 : juce::jmax(1, (int) (span(idx) / 64));
    const int dir = (w.deltaY > 0) != w.isReversed ? 1 : -1;
    if (w.deltaY == 0) return;
    x.hideTip(); x.c.begin(idx); x.c.user(idx, x.c.get(idx) + dir * step); x.c.end(idx);
    x.showVal(*this, idx); refresh();
    juce::Timer::callAfterDelay(700, [sp = juce::Component::SafePointer<KnobW>(this)] { if (sp) sp->x.hideVal(); });
}
bool KnobW::keyPressed(const juce::KeyPress& k) {
    const int st = k.getModifiers().isShiftDown() ? 1 : juce::jmax(1, (int) std::lround(span(idx) / 32));
    const int c = k.getKeyCode(); int v = x.c.get(idx);
    if (c == juce::KeyPress::upKey || c == juce::KeyPress::rightKey) v += st;
    else if (c == juce::KeyPress::downKey || c == juce::KeyPress::leftKey) v -= st;
    else if (c == juce::KeyPress::homeKey) v = kParams[idx].min;
    else if (c == juce::KeyPress::endKey) v = kParams[idx].max;
    else if (c == juce::KeyPress::returnKey) v = kParams[idx].def;
    else return false;
    x.c.begin(idx); x.c.user(idx, v); x.c.end(idx); x.showVal(*this, idx); refresh();
    return true;
}

// =========================================================================================== slider
SliderW::SliderW(Ctx& c, int i, int h, float t) : Ctrl(c), idx(i), hh(h), tk(t) {
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setWantsKeyboardFocus(true); setMouseClickGrabsKeyboardFocus(false);
}
void SliderW::refresh() { const int v = x.c.get(idx); if (v != cached) { cached = v; repaint(); } }
void SliderW::paint(juce::Graphics& g) {
    const Look& L = x.L(); const float h = (float) hh;
    // track
    g.setColour(juce::Colours::black); g.fillRoundedRectangle(13.0f, 6.0f, 8.0f, h - 12.0f, 2.0f);
    g.setColour(juce::Colour(0xff2a2f36)); g.drawRoundedRectangle(13.5f, 6.5f, 7.0f, h - 13.0f, 2.0f, 1.0f);
    // ticks on both sides: repeating 1px lines every tk px, opacity .8
    g.setColour(L[lbl2].withMultipliedAlpha(0.8f));
    for (float y = 6.0f; y < h - 6.0f; y += tk) { g.fillRect(4.0f, y, 3.0f, 1.0f); g.fillRect(27.0f, y, 3.0f, 1.0f); }
    // cap
    const float n = norm(idx, x.c.get(idx));
    const float top = (1.0f - n) * (h - 13.0f);
    juce::Rectangle<float> cp(5.0f, top, 24.0f, 13.0f);
    juce::Path pp; pp.addRoundedRectangle(cp, 2.0f);
    juce::DropShadow(juce::Colours::black.withAlpha(0.7f), 2, { 0, 1 }).drawForPath(g, pp);
    juce::ColourGradient cg(juce::Colour(0xff2b2d31), 0, cp.getY(), juce::Colours::black, 0, cp.getBottom(), false);
    cg.addColour(0.4, L[cap]);
    g.setGradientFill(cg); g.fillPath(pp);
    g.setColour(juce::Colour(0xff3a3d44)); g.drawRoundedRectangle(cp.reduced(0.5f), 2.0f, 1.0f);
    g.setColour(juce::Colour(0xffe8ecf2)); g.fillRect(cp.getX() + 3.0f, cp.getY() + 6.0f, cp.getWidth() - 6.0f, 1.0f);
    if (hasKeyboardFocus(false)) { g.setColour(L[acc]); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 3.0f, 2.0f); }
}
void SliderW::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) return;
    x.hideTip();
    if (x.c.assigning != Controller::kNone) x.c.controlTouched(idx);
    dragging = true; dragStartV = x.c.get(idx); dragStartY = e.getScreenY(); x.c.begin(idx); x.showVal(*this, idx);
}
void SliderW::mouseDrag(const juce::MouseEvent& e) {
    if (!dragging) return;
    const double fine = e.mods.isShiftDown() ? 0.2 : 1.0;
    const double sc = juce::Component::getApproximateScaleFactorForComponent(this);
    const double dv = -((double) (e.getScreenY() - dragStartY)) / sc / ((double) hh - 2.0) * span(idx) * fine;
    x.c.user(idx, (int) std::lround(dragStartV + dv)); x.showVal(*this, idx); refresh();
}
void SliderW::mouseUp(const juce::MouseEvent&) { if (dragging) { dragging = false; x.c.end(idx); x.hideVal(); } }
void SliderW::mouseDoubleClick(const juce::MouseEvent&) { x.c.begin(idx); x.c.user(idx, kParams[idx].def); x.c.end(idx); refresh(); }
void SliderW::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    const int step = e.mods.isShiftDown() ? 1 : juce::jmax(1, (int) (span(idx) / 64));
    if (w.deltaY == 0) return;
    const int dir = (w.deltaY > 0) != w.isReversed ? 1 : -1;
    x.hideTip(); x.c.begin(idx); x.c.user(idx, x.c.get(idx) + dir * step); x.c.end(idx); x.showVal(*this, idx); refresh();
    juce::Timer::callAfterDelay(700, [sp = juce::Component::SafePointer<SliderW>(this)] { if (sp) sp->x.hideVal(); });
}
bool SliderW::keyPressed(const juce::KeyPress& k) {
    const int st = k.getModifiers().isShiftDown() ? 1 : juce::jmax(1, (int) std::lround(span(idx) / 32));
    const int c = k.getKeyCode(); int v = x.c.get(idx);
    if (c == juce::KeyPress::upKey || c == juce::KeyPress::rightKey) v += st;
    else if (c == juce::KeyPress::downKey || c == juce::KeyPress::leftKey) v -= st;
    else if (c == juce::KeyPress::homeKey) v = kParams[idx].min;
    else if (c == juce::KeyPress::endKey) v = kParams[idx].max;
    else if (c == juce::KeyPress::returnKey) v = kParams[idx].def;
    else return false;
    x.c.begin(idx); x.c.user(idx, v); x.c.end(idx); x.showVal(*this, idx); refresh();
    return true;
}

// =========================================================================================== switch
SwitchW::SwitchW(Ctx& c, int i, juce::String a, bool m) : Ctrl(c), idx(i), id(a), mom(m) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
void SwitchW::refresh() {
    const juce::uint32 now = juce::Time::getMillisecondCounter();
    const bool fl = (juce::int32) (flashUntil - now) > 0;
    const bool l = x.c.lit(id);
    const bool b = x.c.blink(id);
    int v = idx >= 0 && !mom ? x.c.get(idx) : 0;
    int state = v | (l ? 2 : 0) | (fl ? 4 : 0) | (b ? 8 : 0);
    if (b) state |= ((now / 500) & 1) ? 16 : 0;           // 1 s blink, half on, half off
    if (state != cached) { cached = state; repaint(); }
}
void SwitchW::paint(juce::Graphics& g) {
    const Look& L = x.L();
    auto r = getLocalBounds().toFloat();
    auto bg = L[btnbg];
    if (down && mom) bg = bg.brighter(0.6f); else if (hover) bg = bg.brighter(0.2f);
    g.setColour(bg); g.fillRoundedRectangle(r, 3.0f);
    g.setColour(L[hi]); g.fillRect(juce::Rectangle<float>(1, 1, r.getWidth() - 2, 1));
    g.setColour(L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
    const bool v = idx >= 0 && !mom && x.c.get(idx) != 0;
    const juce::uint32 now = juce::Time::getMillisecondCounter();
    bool on = v || x.c.lit(id) || (juce::int32) (flashUntil - now) > 0;
    if (x.c.blink(id)) on = ((now / 500) & 1) == 0;
    drawLed(g, juce::Rectangle<float>(r.getCentreX() - 3.5f, r.getCentreY() - 3.5f, 7.0f, 7.0f), on, L);
}
void SwitchW::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) return;
    down = true; x.hideTip(); repaint();
    if (idx >= 0 && !mom) { x.c.begin(idx); x.c.user(idx, x.c.get(idx) ? 0 : 1); x.c.end(idx); refresh(); return; }
}
void SwitchW::mouseUp(const juce::MouseEvent& e) {
    down = false; repaint();
    if (!getLocalBounds().contains(e.getPosition())) return;
    if (idx >= 0 && !mom) return;
    if (onPress) { onPress(); return; }
    flashUntil = juce::Time::getMillisecondCounter() + 160; refresh();
    x.c.action(id);
}

// =========================================================================================== options
OptionsW::OptionsW(Ctx& c, int i, const Opts& oo, juce::Array<const Text*> l) : Ctrl(c), idx(i), o(oo), labels(l) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
void OptionsW::refresh() { const int v = x.c.get(idx); if (v != cached) { cached = v; repaint(); } }
int OptionsW::hit(juce::Point<int> p) const {
    for (int k = 0; k < o.opts.n; ++k) {
        const auto& r = o.opts[k].r;
        if (juce::Rectangle<float>(r.x - o.r.x, r.y - o.r.y, r.w, r.h).contains(p.toFloat())) return k;
    }
    return -1;
}
void OptionsW::mouseMove(const juce::MouseEvent& e) { const int h = hit(e.getPosition()); if (h != hot) { hot = h; repaint(); } }
void OptionsW::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) return;
    const int k = hit(e.getPosition()); if (k < 0) return;
    x.hideTip(); x.c.begin(idx); x.c.user(idx, o.opts[k].v); x.c.end(idx); refresh();
}
void OptionsW::paint(juce::Graphics& g) {
    const Look& L = x.L(); const int cur = x.c.get(idx);
    for (int k = 0; k < o.opts.n; ++k) {
        const auto& op = o.opts[k];
        const juce::Rectangle<float> r(op.r.x - o.r.x, op.r.y - o.r.y, op.r.w, op.r.h);
        if (k == hot) { g.setColour(L[hi]); g.fillRoundedRectangle(r, 2.0f); }
        drawLed(g, juce::Rectangle<float>(op.led.x - o.r.x, op.led.y - o.r.y, op.led.w, op.led.h), op.v == cur, L);
        if (k < labels.size() && labels[k] != nullptr) {
            const Text& t = *labels[k]; const Line& ln = t.lines[0];
            g.setColour(t.tok >= 0 ? L.c[t.tok] : juce::Colour((juce::uint32) t.col)); g.setFont(fontOf(t.f));
            drawTextAt(g, ln.t, ln.x - o.r.x, ln.base - o.r.y, 'l');
        }
    }
}

// =========================================================================================== select
SelectW::SelectW(Ctx& c, const Select& sel, int i, int m) : Ctrl(c), idx(i), mp(m), s(sel) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(true); setMouseClickGrabsKeyboardFocus(false);
    text = currentText();
}
juce::String SelectW::currentText() const {
    if (mp >= 0) { const int m = mpMatch(x.c, mp); return m < 0 ? "Custom" : juce::String(kMp[mp].list[m].name); }
    const int v = x.c.get(idx); const ParamDef& d = kParams[idx];
    if (d.optCount > 0 && v >= d.min && v - d.min < d.optCount) return juce::String::fromUTF8(kOptionText[d.optStart + (v - d.min)]);
    return juce::String(v);
}
void SelectW::refresh() { auto t = currentText(); if (t != text) { text = t; repaint(); } }
void SelectW::drawBox(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, const Look& L, const juce::Font& f, bool hover) {
    g.setColour(L[sel]); g.fillRoundedRectangle(r, 3.0f);
    g.setColour(L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
    if (hover) { g.setColour(L[hi]); g.fillRoundedRectangle(r, 3.0f); }
    g.setColour(L[selfg]); g.setFont(f);
    const float asc = f.getAscent(), desc = f.getDescent();
    const float base = r.getY() + 1.0f + (r.getHeight() - 2.0f - (asc + desc)) / 2.0f + asc;
    g.saveState(); g.reduceClipRegion(r.reduced(1.0f).withTrimmedRight(16.0f).toNearestInt());
    drawTextAt(g, text, r.getX() + 8.6f, std::round(base) + 1.0f, 'l');
    g.restoreState();
    // chevron
    juce::Path ch; const float cx = r.getRight() - 9.5f, cy = r.getCentreY();
    ch.startNewSubPath(cx - 3.0f, cy - 1.5f); ch.lineTo(cx, cy + 1.5f); ch.lineTo(cx + 3.0f, cy - 1.5f);
    g.setColour(L[selfg]); g.strokePath(ch, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
void SelectW::paint(juce::Graphics& g) {
    drawBox(g, getLocalBounds().toFloat(), text, x.L(), fontOf(s.f), hover);
    if (hasKeyboardFocus(false)) { g.setColour(x.L()[acc]); g.drawRoundedRectangle(getLocalBounds().toFloat().expanded(1.0f), 3.0f, 2.0f); }
}
void SelectW::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) return;
    x.hideTip();
    juce::PopupMenu m; auto& lf = menuLnF(); lf.L = &x.L(); m.setLookAndFeel(&lf);
    int cur = -1;
    if (mp >= 0) {
        cur = mpMatch(x.c, mp);
        for (int k = 0; k < kMp[mp].nList; ++k) m.addItem(k + 1, juce::String::fromUTF8(kMp[mp].list[k].name), true, k == cur);
        m.addItem(10000, "Custom", false, cur < 0);
    } else {
        const ParamDef& d = kParams[idx]; cur = x.c.get(idx) - d.min;
        for (int k = 0; k < d.optCount; ++k) m.addItem(k + 1, juce::String::fromUTF8(kOptionText[d.optStart + k]), true, k == cur);
    }
    auto opt = juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()).withMaximumNumColumns(1).withStandardItemHeight(26);
    m.showMenuAsync(opt, [sp = juce::Component::SafePointer<SelectW>(this)](int r) {
        if (!sp || r <= 0 || r == 10000) return;
        auto* me = sp.getComponent();
        if (me->mp >= 0) { mpApply(me->x.c, me->mp, r - 1); }
        else { me->x.c.begin(me->idx); me->x.c.user(me->idx, kParams[me->idx].min + r - 1); me->x.c.end(me->idx); }
        me->refresh();
    });
}
bool SelectW::keyPressed(const juce::KeyPress& k) {
    if (mp >= 0) return false;
    const ParamDef& d = kParams[idx]; int v = x.c.get(idx);
    if (k.getKeyCode() == juce::KeyPress::upKey) v--; else if (k.getKeyCode() == juce::KeyPress::downKey) v++; else return false;
    x.c.begin(idx); x.c.user(idx, juce::jlimit(d.min, d.max, v)); x.c.end(idx); refresh(); return true;
}

// block preset menus
int jp::mpMatch(Controller& c, int m) {
    const MpDef& d = kMp[m];
    int ids[40]; jassert(d.nIds <= 40);
    for (int k = 0; k < d.nIds; ++k) ids[k] = Controller::idx(d.ids[k]);
    for (int p = 0; p < d.nList; ++p) {
        bool all = true;
        for (int k = 0; k < d.nIds && all; ++k) all = c.get(ids[k]) == d.list[p].vals[k];
        if (all) return p;
    }
    return -1;
}
void jp::mpApply(Controller& c, int m, int p) {
    const MpDef& d = kMp[m]; const MpPreset& pr = d.list[p];
    int gpt = -1;
    for (int k = 0; k < d.nIds; ++k) { const int i = Controller::idx(d.ids[k]); if (juce::String(d.ids[k]) == "gpt") { gpt = k; continue; } c.user(i, pr.vals[k]); }
    if (gpt >= 0) c.user(Controller::idx("gpt"), pr.vals[gpt]);
    c.say(juce::String(d.title) + ": " + pr.name);
}

// =========================================================================================== LCD
LcdW::LcdW(Ctx& c, const Lcd& ll, const Text* a, const Text* b) : Ctrl(c), l(ll), t1(a), t2(b) { setInterceptsMouseClicks(false, false); }
void LcdW::refresh() { auto a = x.c.lcd1(), b = x.c.lcd2(); if (a != s1 || b != s2) { s1 = a; s2 = b; repaint(); } }
void LcdW::paint(juce::Graphics& g) {
    const Look& L = x.L(); auto r = getLocalBounds().toFloat();
    g.setColour(L[lcdbg]); g.fillRoundedRectangle(r, 3.0f);
    // inset 0 0 10px rgba(0,0,0,.8)
    for (int i = 0; i < 10; ++i) { const float a = 0.8f * std::pow(1.0f - (float) i / 10.0f, 2.0f) * 0.5f; g.setColour(juce::Colours::black.withAlpha(a)); g.drawRoundedRectangle(r.reduced(1.0f + (float) i), 3.0f, 1.0f); }
    g.setColour(L[edge]); g.drawRoundedRectangle(r.reduced(1.0f), 3.0f, 2.0f);
    g.setColour(L[lcdfg]); g.setFont(fontOf(l.f));
    const float asc = g.getCurrentFont().getAscent(), desc = g.getCurrentFont().getDescent();
    auto line = [&](const Rect& lr, const Text* t, const juce::String& s) {
        const float base = t ? t->lines[0].base - this->getY() : lr.y + (lr.h - (asc + desc)) / 2.0f + asc - getY();
        drawTextAt(g, s, lr.x - (float) getX(), base - (t ? 0.0f : 0.0f), 'l');
    };
    line(l.l1, t1, s1); line(l.l2, t2, s2);
}

// =========================================================================================== header buttons
HBtnW::HBtnW(Ctx& c, const HBtn& bb, const Text* t, float shiftY) : Ctrl(c), b(bb), txt(t), dy(shiftY) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
void HBtnW::refresh() { const bool o = isOn && isOn(); if (o != on) { on = o; repaint(); } }
void HBtnW::paint(juce::Graphics& g) {
    const Look& L = x.L(); auto r = getLocalBounds().toFloat();
    auto bg = on ? L[acc] : L[btnbg];
    if (hover && !on) bg = bg.brighter(0.25f);
    g.setColour(bg); g.fillRoundedRectangle(r, 4.0f);
    g.setColour(L[hi]); g.fillRect(juce::Rectangle<float>(1, 1, r.getWidth() - 2, 1));
    g.setColour(on ? L[acc] : L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
    if (txt == nullptr) return;
    g.setColour(on ? juce::Colour(0xff10141a) : L[lbl]);
    const float base = txt->lines[0].base - (float) getY() + dy;
    if (juce::String::fromUTF8(b.t) == juce::String::fromUTF8("\xe2\x98\xb0")) {                     // the hamburger glyph
        const float cx = r.getCentreX(), cy = r.getCentreY();
        for (int k = -1; k <= 1; ++k) g.fillRoundedRectangle(cx - 6.5f, cy + (float) k * 4.6f - 1.0f, 13.0f, 2.0f, 0.5f);
        return;
    }
    g.setFont(fontOf(txt->f));
    drawTextAt(g, txt->lines[0].t, txt->lines[0].x + txt->lines[0].w / 2.0f - (float) getX(), base, 'c');
}
void HBtnW::mouseDown(const juce::MouseEvent& e) { if (!e.mods.isPopupMenu()) x.hideTip(); }
void HBtnW::mouseUp(const juce::MouseEvent& e) { if (getLocalBounds().contains(e.getPosition()) && !e.mods.isPopupMenu() && onClick) onClick(); }
