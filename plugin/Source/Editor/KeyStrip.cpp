#include "KeyStrip.h"
#include "../PluginProcessor.h"
using namespace jp;
using namespace jp::layout;
using namespace jpurrr;

namespace jp {

// ------------------------------------------------------------------------------ keys
class KeysW : public juce::Component {
public:
    explicit KeysW(KeyboardStrip& s) : strip(s) {
        setComponentID("keys");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        for (auto& k : kStrip.keys) boxes.push_back({ k.note, k.black, k.r.x, k.r.y, k.r.w, k.r.h });
        for (auto& d : down) d = -1;
    }
    void paint(juce::Graphics& g) override {
        g.addTransform(juce::AffineTransform::translation(-(kStrip.r.x + (float) getX()), -(float) getY()));
        drawKeyStripKeys(g, strip.L(), 0.0f, strip.lit, strip.tint, strip.ck.lowNote());
    }
    // the point is in strip coordinates (x is a window x)
    int noteAt(juce::Point<float> p) const { return keyAtPoint(boxes.data(), (int) boxes.size(), p.x, p.y); }
    bool hitTest(int x, int y) override { return noteAt(toStrip({ (float) x, (float) y })) >= 0; }
    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) return;
        strip.hideTip();
        const int n = noteAt(toStrip(e.position)); if (n < 0) return;
        const int s = src(e); down[s] = n; strip.noteOn(n);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        const int s = src(e); if (down[s] < 0) return;
        const int n = noteAt(toStrip(e.position));                     // glissando; off the keys keeps the last note (as the mockup)
        if (n >= 0 && n != down[s]) { strip.noteOff(down[s]); down[s] = n; strip.noteOn(n); }
    }
    void mouseUp(const juce::MouseEvent& e) override {
        const int s = src(e); if (down[s] < 0) return;
        strip.noteOff(down[s]); down[s] = -1;
    }
    bool pointerDown() const { for (int d : down) if (d >= 0) return true; return false; }
    void forget() { for (auto& d : down) d = -1; }
private:
    KeyboardStrip& strip; std::vector<KeyBox> boxes; int down[16];
    static int src(const juce::MouseEvent& e) { return juce::jlimit(0, 15, e.source.getIndex()); }
    juce::Point<float> toStrip(juce::Point<float> local) const { return { local.x + (float) getX() + kStrip.r.x, local.y + (float) getY() }; }
};

// ------------------------------------------------------------------------------ split marker (mockup v3.5 #spl)
// Over the keys while the PARTS band is shown: in SPLIT the stepped split line with the draggable SPLIT handle (LOWER and UPPER captions beside
// it), in DUAL the caption "UPPER + LOWER on every key". The handle snaps to a key, arrow keys move it, it sets the Split Point parameter.
class SplitW : public juce::Component {
public:
    explicit SplitW(KeyboardStrip& s) : strip(s) { setComponentID("split"); setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); setWantsKeyboardFocus(true); setMouseClickGrabsKeyboardFocus(true); }
    int km() const { return strip.tint.km; }
    int sp() const { return strip.ctl.get(P_sp); }
    bool active() const { return strip.tint.on && km() != 0; }
    // the key area (padding box of #keys) in this component's frame (the component covers the border box of #keys)
    juce::Rectangle<float> pad() const { return { 1.0f, 1.0f, (float) getWidth() - 2.0f, (float) getHeight() - 2.0f }; }
    struct Geo { juce::Rectangle<float> handle, capL, capR; bool showL = false, showR = false; float wSmall = 0, wNote = 0; };
    static juce::Font fSmall() { return fontOf(kCond, 16.7f, 700, 0.14f); }
    static juce::Font fNote() { return fontOf(kMono, 18.4f, 600); }
    static juce::Font fCap() { return fontOf(kCond, 16.7f, 700, 0.14f); }
    Geo geo() const {
        Geo g; const auto P = pad(); const auto s = splitX(sp());
        g.wSmall = textWidth(fSmall(), "SPLIT"); g.wNote = textWidth(fNote(), noteNameOf(sp()));
        const float w = 2 + 20 + g.wSmall + 8 + g.wNote;
        const float cx = juce::jlimit(P.getX() + 76.0f, P.getRight() - 76.0f, P.getX() + s.xt * P.getWidth() / 100.0f);
        g.handle = { cx - w / 2.0f, P.getY() + 6.0f, w, 26.0f };
        const float wl = textWidth(fCap(), juce::String::fromUTF8("\xe2\x97\x80 LOWER")) + 20, wr = textWidth(fCap(), juce::String::fromUTF8("UPPER \xe2\x96\xb6")) + 20;
        g.capL = { g.handle.getX() + 1.0f - 8.0f - wl, g.handle.getY(), wl, 26.0f };
        g.capR = { g.handle.getRight() - 1.0f + 8.0f, g.handle.getY(), wr, 26.0f };
        g.showL = s.xt > 14.0f; g.showR = s.xt < 86.0f;
        return g;
    }
    juce::Rectangle<float> handleRect() const { return geo().handle; }
    void paint(juce::Graphics& g) override {
        if (!active()) return;
        const Look& L = strip.L(); const auto P = pad();
        auto cap = [&](juce::Rectangle<float> r, const juce::String& text, juce::Colour c) {
            g.setColour(L[hdr].withAlpha(0.88f)); g.fillRoundedRectangle(r, 4.0f);
            g.setColour(L[edge]); g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
            g.setColour(c); g.setFont(fCap()); drawCentredBase(g, text, r);
        };
        if (km() == 1) {                                                // DUAL
            const float wU = textWidth(fCap(), "UPPER"), wP = textWidth(fCap(), juce::String::fromUTF8("\xc2\xa0+\xc2\xa0")), wL = textWidth(fCap(), "LOWER"), wT = textWidth(fCap(), juce::String::fromUTF8("\xc2\xa0on every key"));
            const juce::Rectangle<float> r(P.getX() + 8.0f, P.getY() + 6.0f, 20.0f + wU + wP + wL + wT, 26.0f);
            g.setColour(L[hdr].withAlpha(0.88f)); g.fillRoundedRectangle(r, 4.0f);
            g.setColour(L[edge]); g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
            g.setFont(fCap()); const float base = baselineIn(r, fCap()); float x = r.getX() + 10.0f;
            g.setColour(L[acc]); drawTextAt(g, "UPPER", x, base, 'l'); x += wU;
            g.setColour(L[lbl]); drawTextAt(g, juce::String::fromUTF8("\xc2\xa0+\xc2\xa0"), x, base, 'l'); x += wP;
            g.setColour(L[acc2]); drawTextAt(g, "LOWER", x, base, 'l'); x += wL;
            g.setColour(L[lbl2]); drawTextAt(g, juce::String::fromUTF8("\xc2\xa0on every key"), x, base, 'l');
            return;
        }
        drawSplitLines(g, L, P, sp());
        const auto G = geo(); const bool hot = dragging || ring || hover;
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillRoundedRectangle(G.handle.translated(0, 2).expanded(1.0f), 5.0f);       // 0 2px 6px rgba(0,0,0,.6)
        if (hot) { g.setColour(L[acc]); g.drawRoundedRectangle(G.handle.expanded(2.0f), 6.0f, 2.0f); }
        g.setColour(L[hdr]); g.fillRoundedRectangle(G.handle, 4.0f);
        g.setColour(hot ? L[acc] : L[lbl]); g.drawRoundedRectangle(G.handle.reduced(0.5f), 4.0f, 1.0f);
        const float x0 = G.handle.getX() + 1 + 10, base = baselineIn(G.handle, fNote());
        g.setColour(L[lbl2]); g.setFont(fSmall()); drawTextAt(g, "SPLIT", x0, baselineIn(G.handle, fSmall()), 'l');
        g.setColour(L[lbl]); g.setFont(fNote()); drawTextAt(g, noteNameOf(sp()), x0 + G.wSmall + 8, base, 'l');
        if (G.showL) cap(G.capL, juce::String::fromUTF8("\xe2\x97\x80 LOWER"), L[acc2]);
        if (G.showR) cap(G.capR, juce::String::fromUTF8("UPPER \xe2\x96\xb6"), L[acc]);
    }
    bool hitTest(int x, int y) override {
        if (!active() || km() != 2) return false;
        const juce::Point<float> p((float) x, (float) y); const auto G = geo(); const auto P = pad(); const auto s = splitX(sp());
        if (G.handle.contains(p) || (G.showL && G.capL.contains(p)) || (G.showR && G.capR.contains(p))) return true;
        const float xt = P.getX() + s.xt * P.getWidth() / 100.0f, xb = P.getX() + s.xb * P.getWidth() / 100.0f;
        if (juce::Rectangle<float>(xt - 2.0f, P.getY(), 4.0f, P.getHeight() * 0.6f).contains(p)) return true;
        if (juce::Rectangle<float>(xb - 2.0f, P.getY() + P.getHeight() * 0.6f, 4.0f, P.getHeight() * 0.4f).contains(p)) return true;
        return xt != xb && juce::Rectangle<float>(std::min(xt, xb), P.getY() + P.getHeight() * 0.6f - 2.0f, std::abs(xt - xb), 4.0f).contains(p);
    }
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) return;
        strip.hideTip(); dragging = true; ring = false; setTo(e.position); repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override { if (dragging) setTo(e.position); }
    void mouseUp(const juce::MouseEvent&) override { dragging = false; repaint(); }
    bool keyPressed(const juce::KeyPress& k) override {
        if (!active() || km() != 2) return false;
        const int c = k.getKeyCode(); const bool shift = k.getModifiers().isShiftDown();
        const int st = (c == juce::KeyPress::leftKey || c == juce::KeyPress::downKey) ? -1 : (c == juce::KeyPress::rightKey || c == juce::KeyPress::upKey) ? 1
                     : c == juce::KeyPress::pageDownKey ? -12 : c == juce::KeyPress::pageUpKey ? 12 : 0;
        if (!st) return false;
        set(juce::jlimit(36, 96, sp() + st * (shift && std::abs(st) == 1 ? 12 : 1)));
        return true;
    }
    void focusGained(FocusChangeType t) override { ring = (t == focusChangedByTabKey); repaint(); }
    void focusLost(FocusChangeType) override { ring = false; dragging = false; repaint(); }
    void refresh() { if (lastKm != km() || lastSp != sp() || lastOn != active()) { lastKm = km(); lastSp = sp(); lastOn = active(); repaint(); } }
private:
    KeyboardStrip& strip; bool dragging = false, ring = false, hover = false; int lastKm = -1, lastSp = -1; bool lastOn = false;
    static float baselineIn(juce::Rectangle<float> r, const juce::Font& f) { return r.getY() + (r.getHeight() - (f.getAscent() + f.getDescent())) / 2.0f + f.getAscent(); }
    static void drawCentredBase(juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r) { drawTextAt(g, s, r.getCentreX(), baselineIn(r, g.getCurrentFont()), 'c'); }
    void set(int v) { if (v != sp()) strip.ctl.user(P_sp, v); repaint(); strip.keysRepaint(); }
    // the nearest key edge (as the top of the split line is seen) to the pointer
    void setTo(juce::Point<float> p) {
        const float f = juce::jlimit(0.0f, 100.0f, p.x / (float) getWidth() * 100.0f);
        int best = 36; float bd = 1e9f;
        for (int n = 36; n <= 96; ++n) { const float d = std::abs(splitX(n).xt - f); if (d < bd) { bd = d; best = n; } }
        set(best);
    }
};

// ------------------------------------------------------------------------------ KEYS / VEL readout (mockup v3.7 #kinfo)
// A small label over the top right corner of the keys: the notes the computer keys play now ("KEYS C4 - F5") and their velocity ("VEL 100").
// Under the SPLIT handle row it steps down (mockup #keys.sp2). It only takes the mouse where it is drawn (nothing under it plays).
class InfoW : public juce::Component {
public:
    explicit InfoW(KeyboardStrip& s) : strip(s) { setComponentID("kinfo"); setInterceptsMouseClicks(true, false); }
    static juce::Font fSmall() { return fontOf(kCond, 16.7f, 700, 0.14f); }
    static juce::Font fNote() { return fontOf(kMono, 18.4f, 600); }
    juce::Rectangle<float> pad() const { return { 1.0f, 1.0f, (float) getWidth() - 2.0f, (float) getHeight() - 2.0f }; }
    juce::String keysText() const { char b[32]; ckKeysText(strip.ck.lowNote(), b, 32); return b; }
    juce::String velText() const { return juce::String(strip.ck.velocity()); }
    juce::Rectangle<float> box() const {
        const auto P = pad();
        const float w = 2 + 20 + textWidth(fSmall(), "KEYS") + 8 + textWidth(fNote(), keysText()) + 16 + textWidth(fSmall(), "VEL") + 8 + textWidth(fNote(), velText());
        const float top = strip.tint.on && strip.tint.km == 2 ? 38.0f : 6.0f;
        return { P.getRight() - 8.0f - w, P.getY() + top, w, 26.0f };
    }
    bool hitTest(int x, int y) override { return box().contains((float) x, (float) y); }
    void paint(juce::Graphics& g) override {
        const Look& L = strip.L(); const auto r = box();
        g.setColour(L[hdr].withAlpha(0.88f)); g.fillRoundedRectangle(r, 4.0f);
        g.setColour(L[edge]); g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
        auto base = [&](const juce::Font& f) { return r.getY() + (r.getHeight() - (f.getAscent() + f.getDescent())) / 2.0f + f.getAscent(); };
        float x = r.getX() + 1 + 10;
        auto part = [&](const char* label, const juce::String& v) {
            g.setColour(L[lbl2]); g.setFont(fSmall()); drawTextAt(g, label, x, base(fSmall()), 'l'); x += textWidth(fSmall(), label) + 8;
            g.setColour(L[lbl]); g.setFont(fNote()); drawTextAt(g, v, x, base(fNote()), 'l'); x += textWidth(fNote(), v);
        };
        part("KEYS", keysText()); x += 16; part("VEL", velText());
    }
    void refresh() { if (lastBase != strip.ck.lowNote() || lastVel != strip.ck.velocity() || lastSp != (strip.tint.on && strip.tint.km == 2)) { lastBase = strip.ck.lowNote(); lastVel = strip.ck.velocity(); lastSp = strip.tint.on && strip.tint.km == 2; repaint(); } }
private:
    KeyboardStrip& strip; int lastBase = -1, lastVel = -1; bool lastSp = false;
};

// ------------------------------------------------------------------------------ wheels
class WheelW : public juce::Component, private juce::Timer {
public:
    WheelW(KeyboardStrip& s, int index) : strip(s), idx(index), w(kStrip.wheels[(size_t) index]), mod(!w.pitch) {
        setComponentID(w.id);                                          // wBend, wMod (the mockup ids; the headless check finds them)
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus(true); setMouseClickGrabsKeyboardFocus(true);
        target = shown = norm();
    }
    ~WheelW() override { stopTimer(); }
    int param() const { return mod ? P_mod : P_bend; }
    int value() const { return strip.ctl.get(param()); }
    float norm() const { return mod ? modWheelNorm(value()) : bendWheelNorm(value()); }

    void paint(juce::Graphics& g) override {
        g.addTransform(juce::AffineTransform::translation(-(kStrip.r.x + (float) getX()), -(float) getY()));
        drawKeyStripWheel(g, strip.L(), 0.0f, idx, norm(), shown, dragging, ring);
    }
    bool hitTest(int x, int y) override {
        const float ax = (float) (x + getX()) + kStrip.r.x, ay = (float) (y + getY());
        return ax >= w.r.x && ax < w.r.x + w.r.w && ay >= w.r.y && ay < w.r.y + w.r.h;
    }

    // follow the value (called at 30 Hz and after every change)
    void refresh() {
        const float n = norm();
        if (n != target) { target = n; if (dragging) shown = n; else { from = shown; t0 = now(); startTimer(16); } }
        if (n != lastPainted) { lastPainted = n; repaint(); }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) return;
        stopSpring(); strip.hideTip(); ring = false;
        dragging = true; startScreenY = e.getScreenY(); startN = norm(); shown = target = startN;
        show(); repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!dragging) return;
        const double sc = juce::Component::getApproximateScaleFactorForComponent(this);
        const float travelUp = (float) ((double) (startScreenY - e.getScreenY()) / sc);
        setNorm(wheelDragNorm(startN, travelUp, w.r.h));
        show();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (!dragging) return;
        dragging = false; hideBubbleNow(); if (!mod) springBack(); repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override { if (mod) { setNorm(0.0f); flash(); } }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& d) override {
        if (d.deltaY == 0) return;
        const int dir = (d.deltaY > 0) != d.isReversed ? 1 : -1;
        strip.hideTip();
        if (!mod) { stopSpring(); setNorm(bendWheelStep(norm(), dir, false)); springAt = now() + 260.0; startTimer(16); }
        else setNorm(modWheelScroll(norm(), dir, e.mods.isShiftDown()));
        flash();
    }
    bool keyPressed(const juce::KeyPress& k) override {
        const int c = k.getKeyCode();
        const int dir = (c == juce::KeyPress::upKey || c == juce::KeyPress::pageUpKey) ? 1 : (c == juce::KeyPress::downKey || c == juce::KeyPress::pageDownKey) ? -1 : 0;
        const bool page = c == juce::KeyPress::pageUpKey || c == juce::KeyPress::pageDownKey;
        if (mod && c == juce::KeyPress::homeKey) { setNorm(0.0f); flash(); return true; }
        if (!dir) return false;
        if (!mod) { stopSpring(); setNorm(bendWheelStep(norm(), dir, page)); }
        else setNorm(modWheelKey(norm(), dir, page, k.getModifiers().isShiftDown()));
        flash(); return true;
    }
    bool keyStateChanged(bool) override {                               // arrow or page key let go: BEND springs back
        if (mod || dragging) return false;                                  // (only the focused wheel is asked)
        for (int c : { juce::KeyPress::upKey, juce::KeyPress::downKey, juce::KeyPress::pageUpKey, juce::KeyPress::pageDownKey })
            if (juce::KeyPress::isKeyCurrentlyDown(c)) return false;
        if (norm() != 0.5f && !springing()) springBack();
        return false;
    }
    void focusGained(FocusChangeType t) override { ring = (t == focusChangedByTabKey); repaint(); }   // :focus-visible: keyboard focus only
    void focusLost(FocusChangeType) override { ring = false; repaint(); }
    void step() { timerCallback(); }                                  // test hook: one animation step
    void snapshotView(bool drag, bool focus) { dragging = drag; ring = focus; shown = target = norm(); lastPainted = -1; repaint(); }
    void stopAll() { stopSpring(); dragging = false; ring = false; hideBubbleNow(); }

private:
    KeyboardStrip& strip; int idx; const Wheel& w; bool mod;
    bool dragging = false, ring = false;
    int startScreenY = 0; float startN = 0.5f;
    float target = 0, shown = 0, from = 0, lastPainted = -1; double t0 = 0;       // handle slide (CSS transition 0.12 s ease-out)
    double springStart = -1, springAt = -1, hideAt = -1; int springV0 = 0;
    static double now() { return juce::Time::getMillisecondCounterHiRes(); }
    bool springing() const { return springStart >= 0; }

    void setNorm(float n) {
        n = clamp01(n);
        const int v = mod ? modFromWheelNorm(n) : bendFromWheelNorm(n);
        if (v != value()) strip.ctl.quiet(param(), v);
        refresh();
    }
    void show() {
        if (!strip.showBubble) return;
        char b[64]; wheelBubbleText(mod, value(), strip.ctl.get(P_bu), strip.ctl.get(P_bd), b, 64);
        strip.showBubble(*this, juce::String::fromUTF8(b));
    }
    void flash() { show(); hideAt = now() + 700.0; startTimer(16); }
    void hideBubbleNow() { hideAt = -1; if (strip.hideBubble) strip.hideBubble(); }
    void stopSpring() { springStart = springAt = -1; }
    void springBack() { springStart = now(); springAt = -1; springV0 = value(); startTimer(16); }

    void timerCallback() override {
        const double t = now(); bool busy = false;
        if (springAt >= 0 && t >= springAt) springBack();
        if (springStart >= 0) {
            const float el = (float) (t - springStart);
            const int v = bendSpringValue(springV0, el);
            if (v != value()) strip.ctl.quiet(P_bend, v);
            if (el >= kSpringMs) springStart = -1; else busy = true;
        }
        if (springAt >= 0) busy = true;
        if (hideAt >= 0) { if (t >= hideAt) hideBubbleNow(); else busy = true; }
        const float n = norm();
        if (n != target) { target = n; if (dragging) shown = n; else { from = shown; t0 = t; } }
        if (!dragging && shown != target) {
            const float el = (float) (t - t0);
            shown = el >= kHandleMs ? target : handleEase(from, target, el);
            repaint(); if (shown != target) busy = true;
        }
        if (n != lastPainted) { lastPainted = n; repaint(); }
        if (!busy) stopTimer();
    }
};

} // namespace jp

static juce::Rectangle<int> snapRect(const layout::Rect& r) {
    return { (int) std::round(r.x), (int) std::round(r.y), (int) (std::round(r.x + r.w) - std::round(r.x)), (int) (std::round(r.y + r.h) - std::round(r.y)) };
}

// ============================================================================================== KeyboardStrip
KeyboardStrip::KeyboardStrip(JPurrr8000AudioProcessor& p, Ctx& c) : proc(p), x(c), ctl(c.c) {
    setInterceptsMouseClicks(false, true);
    keys = std::make_unique<KeysW>(*this);
    addAndMakeVisible(*keys);
    split = std::make_unique<SplitW>(*this);
    addAndMakeVisible(*split);
    info = std::make_unique<InfoW>(*this);
    addAndMakeVisible(*info);
    for (int i = 0; i < 2; ++i) { wheels[i] = std::make_unique<WheelW>(*this, i); addAndMakeVisible(*wheels[i]); }
    place(0.0f);
}
KeyboardStrip::~KeyboardStrip() { releaseAll(); }

void KeyboardStrip::place(float stripY) {
    // children are placed in strip coordinates relative to the strip box: x - r.x, y
    keys->setBounds(snapRect(kStrip.keysRect).translated(-(int) std::lround(kStrip.r.x), 0));
    split->setBounds(keys->getBounds());
    info->setBounds(keys->getBounds());
    for (int i = 0; i < 2; ++i) {
        const auto& r = kStrip.wheels[(size_t) i].r;
        wheels[i]->setBounds(juce::Rectangle<int>((int) std::floor(r.x - kStrip.r.x) - 5, (int) std::floor(r.y) - 5, (int) std::ceil(r.w) + 10, (int) std::ceil(r.h) + 10));
    }
    setBounds((int) std::lround(kStrip.r.x), (int) std::lround(stripY), (int) std::lround(kStrip.r.w), (int) std::lround(kStrip.r.h));
}

void KeyboardStrip::paint(juce::Graphics& g) {
    g.addTransform(juce::AffineTransform::translation(-kStrip.r.x, 0.0f));
    drawKeyStripBox(g, L(), 0.0f);
    drawKeyStripCaptions(g, L(), 0.0f);
}
void KeyboardStrip::lookChanged() { repaint(); }
void KeyboardStrip::animateForTest(int ms) {
    const double end = juce::Time::getMillisecondCounterHiRes() + ms;
    while (juce::Time::getMillisecondCounterHiRes() < end) { for (auto& w : wheels) w->step(); juce::Thread::sleep(8); }
}
void KeyboardStrip::snapshotState(int dragWheel, int focusWheel) { for (int i = 0; i < 2; ++i) wheels[i]->snapshotView(i == dragWheel, i == focusWheel); }

void KeyboardStrip::setPartsMode(bool on) {
    const KeyTint t { on, ctl.get(P_km), ctl.get(P_sp) };
    if (t.on != tint.on || t.km != tint.km || t.sp != tint.sp) { tint = t; keys->repaint(); split->repaint(); info->repaint(); }
}
void KeyboardStrip::keysRepaint() { setPartsMode(tint.on); keys->repaint(); }
juce::Rectangle<float> KeyboardStrip::infoRect() const { return info->box().translated((float) info->getX() + kStrip.r.x, (float) info->getY()); }
void KeyboardStrip::setComputerKeys(int base, int vel) { ck.setBase(base); ck.setVelocity(vel); keys->repaint(); info->repaint(); }
juce::Rectangle<float> KeyboardStrip::splitHandle() const {
    if (!tint.on || tint.km != 2) return {};
    auto r = split->handleRect().translated((float) split->getX() + kStrip.r.x, (float) split->getY());
    return r;
}
void KeyboardStrip::refresh() {
    setPartsMode(tint.on);                             // follow KEY MODE and the split point (panel, host, MIDI)
    split->refresh();
    info->refresh();
    for (auto& w : wheels) w->refresh();
    // computer keys: a key that went up without us noticing, or focus that left the editor
    if (ck.keysDown() > 0) {
        const bool focused = getParentComponent() != nullptr && getParentComponent()->hasKeyboardFocus(true);
        if (!focused) releaseComputerKeys(); else keyStateChanged();
    }
    // a mouse note whose button is no longer down (window switched away during the press)
    if (keys->pointerDown() && juce::Desktop::getInstance().getNumDraggingMouseSources() == 0) {
        for (int n = kKeyFirstNote; n <= kKeyLastNote; ++n) if (held.isDown(n)) {
            if (!ck.plays(n)) noteOff(n);
        }
        keys->forget();
    }
}

void KeyboardStrip::noteOn(int note, int vel) {
    if (!held.press(note)) return;
    if (note >= 0 && note < 128) lit[note] = true;
    proc.stripNoteOn(note, vel);
    keys->repaint();
}
void KeyboardStrip::noteOff(int note) {
    if (!held.release(note)) return;
    if (note >= 0 && note < 128) lit[note] = false;
    proc.stripNoteOff(note);
    keys->repaint();
}
void KeyboardStrip::releaseAll() {
    held.releaseAll([this](int n) { if (n < 128) lit[n] = false; proc.stripNoteOff(n); });
    ck.releaseAll([](int) {});                         // the notes themselves ended just above
    if (keys) { keys->forget(); keys->repaint(); }
    for (auto& w : wheels) if (w) w->stopAll();
}
void KeyboardStrip::releaseComputerKeys() { ck.releaseAll([this](int n) { noteOff(n); }); }

bool KeyboardStrip::keyDown(const juce::KeyPress& k) {
    if (!isVisible()) return false;
    const auto m = k.getModifiers();
    if (m.isCtrlDown() || m.isCommandDown() || m.isAltDown()) return false;
    if (modalOpen && modalOpen()) return false;
    if (auto* fc = juce::Component::getCurrentlyFocusedComponent()) {             // not while typing, and not on a focused knob or slider (as the mockup)
        if (dynamic_cast<juce::TextInputTarget*>(fc) != nullptr) return false;
        if (dynamic_cast<KnobW*>(fc) != nullptr || dynamic_cast<SliderW*>(fc) != nullptr) return false;
    }
    // Keys are told apart by the key code the host gives us (the typed key, so the letters follow the layout; see the notes in the manual).
    const bool ours = ck.press(k.getKeyCode(), (uint32_t) k.getTextCharacter(), [this](int n, int v) { noteOn(n, v); }, [this](int n) { noteOff(n); });
    if (ours) { keys->repaint(); info->repaint(); }
    return ours;
}

void KeyboardStrip::keyStateChanged() {
    auto isUp = [](int c) {
        const bool down = juce::KeyPress::isKeyCurrentlyDown(c) || (c >= 'A' && c <= 'Z' && juce::KeyPress::isKeyCurrentlyDown(c + 32)) || (c >= 'a' && c <= 'z' && juce::KeyPress::isKeyCurrentlyDown(c - 32));
        return !down;
    };
    ck.releaseIf(isUp, [this](int n) { noteOff(n); });
}
