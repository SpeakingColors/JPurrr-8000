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
        drawKeyStripKeys(g, 0.0f, strip.lit);
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
    for (int i = 0; i < 2; ++i) { wheels[i] = std::make_unique<WheelW>(*this, i); addAndMakeVisible(*wheels[i]); }
    place(0.0f);
}
KeyboardStrip::~KeyboardStrip() { releaseAll(); }

void KeyboardStrip::place(float stripY) {
    // children are placed in strip coordinates relative to the strip box: x - r.x, y
    keys->setBounds(snapRect(kStrip.keysRect).translated(-(int) std::lround(kStrip.r.x), 0));
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

void KeyboardStrip::refresh() {
    for (auto& w : wheels) w->refresh();
    // computer keys: a key that went up without us noticing, or focus that left the editor
    if (!ckeys.empty()) {
        const bool focused = getParentComponent() != nullptr && getParentComponent()->hasKeyboardFocus(true);
        if (!focused) releaseComputerKeys(); else keyStateChanged();
    }
    // a mouse note whose button is no longer down (window switched away during the press)
    if (keys->pointerDown() && juce::Desktop::getInstance().getNumDraggingMouseSources() == 0) {
        for (int n = kKeyFirstNote; n <= kKeyLastNote; ++n) if (held.isDown(n)) {
            bool fromKey = false; for (auto& k : ckeys) if (k.note == n) fromKey = true;
            if (!fromKey) noteOff(n);
        }
        keys->forget();
    }
}

void KeyboardStrip::noteOn(int note) {
    if (!held.press(note)) return;
    if (note >= 0 && note < 128) lit[note] = true;
    proc.stripNoteOn(note);
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
    ckeys.clear();
    if (keys) { keys->forget(); keys->repaint(); }
    for (auto& w : wheels) if (w) w->stopAll();
}
void KeyboardStrip::releaseComputerKeys() {
    auto ks = ckeys; ckeys.clear();
    for (auto& k : ks) noteOff(k.note);
}

bool KeyboardStrip::keyDown(const juce::KeyPress& k) {
    if (!isVisible()) return false;
    const auto m = k.getModifiers();
    if (m.isCtrlDown() || m.isCommandDown() || m.isAltDown()) return false;
    if (modalOpen && modalOpen()) return false;
    if (auto* fc = juce::Component::getCurrentlyFocusedComponent()) {             // not while typing, and not on a focused knob or slider (as the mockup)
        if (dynamic_cast<juce::TextInputTarget*>(fc) != nullptr) return false;
        if (dynamic_cast<KnobW*>(fc) != nullptr || dynamic_cast<SliderW*>(fc) != nullptr) return false;
    }
    const int n = keyboardNoteForChar((uint32_t) k.getTextCharacter());
    if (n < 0) return false;
    for (auto& c : ckeys) if (c.code == k.getKeyCode()) return true;              // auto repeat
    ckeys.push_back({ k.getKeyCode(), n });
    noteOn(n);
    return true;
}

void KeyboardStrip::keyStateChanged() {
    for (size_t i = 0; i < ckeys.size();) {
        const int c = ckeys[i].code;
        const bool down = juce::KeyPress::isKeyCurrentlyDown(c) || (c >= 'A' && c <= 'Z' && juce::KeyPress::isKeyCurrentlyDown(c + 32)) || (c >= 'a' && c <= 'z' && juce::KeyPress::isKeyCurrentlyDown(c - 32));
        if (!down) { const int n = ckeys[i].note; ckeys.erase(ckeys.begin() + (long) i); noteOff(n); } else ++i;
    }
}
