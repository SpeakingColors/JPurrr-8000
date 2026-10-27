#include "Dialogs.h"
#include "Root.h"
#include <cmath>
using namespace jp;
using namespace jpurrr;
using layout::Tok;

// All numbers below are the mockup's CSS pixels. The dialogs use `zoom: 1.3`, so every length is multiplied by Z.
// Borders stay one device pixel. Line heights follow Chrome: "normal" = round(ascent) + round(descent).
namespace {
constexpr float Z = 1.3f;
using juce::String;
using Fl = juce::Rectangle<float>;

juce::Font fb(float css, int w = 400, float ls = 0) { return fontOf(layout::kBody, css * Z, w, ls); }
juce::Font fc(float css, int w = 600, float ls = 0) { return fontOf(layout::kCond, css * Z, w, ls); }
juce::Font fm(float css, int w = 400) { return fontOf(layout::kMono, css * Z, w); }
float asc(const juce::Font& f) { return std::round(f.getAscent()); }
float dsc(const juce::Font& f) { return std::round(f.getDescent()); }
float nrm(const juce::Font& f) { return asc(f) + dsc(f); }

constexpr float kPadX = 16 * Z, kPadY = 12 * Z, kGap = 10 * Z;
const juce::Colour kGoText(0xff10141a);

// Focus ring: the mockup's accent outline on buttons and selects (2px, offset 1px, zoomed), Chrome's light ring on inputs.
void drawRing(juce::Graphics& g, Fl r, float radius, juce::Colour c = juce::Colour(0xffe8eefc), float width = 2.0f, float off = 1.0f) {
    g.setColour(c); g.drawRoundedRectangle(r.expanded(off + width / 2), radius + off + width / 2, width);
}

// ------------------------------------------------------------------------------------------------ plain widgets
class DBtn : public juce::Component {
public:
    DBtn(Root& r, const String& t, bool g) : root(r), text(t.toUpperCase()), go(g) { setWantsKeyboardFocus(true); setPaintingIsUnclipped(true); }
    std::function<void()> onClick; bool ring = false;
    static juce::Font font() { return fc(18.4, 600, 0.06f); }
    float prefWidth() const { return 2.0f + 2 * 10 * Z + jp::textWidth(font(), text); }
    void paint(juce::Graphics& g) override {
        const Look& lk = root.look(); auto r = getLocalBounds().toFloat();
        auto bg = go ? lk[layout::acc] : lk[layout::btnbg]; if (hover && !go) bg = bg.brighter(0.25f);
        g.setColour(bg); g.fillRoundedRectangle(r, 4 * Z);
        g.setColour(lk[layout::hi]); g.fillRect(Fl(r.getX() + 2, r.getY() + 1, r.getWidth() - 4, 1.0f));
        g.setColour(go ? lk[layout::acc] : lk[layout::btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 4 * Z, 1.0f);
        const auto f = font(); g.setFont(f); g.setColour(go ? kGoText : lk[layout::lbl]);
        drawTextAt(g, text, r.getX() + 1 + 10 * Z, std::round(r.getY() + 1 + (r.getHeight() - 2 - nrm(f)) / 2 + asc(f)), 'l');
        if (ring || hasKeyboardFocus(false)) drawRing(g, r, 4 * Z, lk[layout::acc], 2 * Z, 1 * Z);
    }
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp(const juce::MouseEvent& e) override { if (contains(e.getPosition()) && !e.mods.isPopupMenu() && onClick) onClick(); }
    bool keyPressed(const juce::KeyPress& k) override {
        if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) { if (onClick) onClick(); return true; }
        return false;
    }
    void focusLost(FocusChangeType) override { ring = false; repaint(); }
    void focusGained(FocusChangeType) override { repaint(); }
private:
    Root& root; String text; bool go, hover = false;
};

class DChk : public juce::Component {                          // label.chk with a native checkbox
public:
    DChk(Root& r, const String& t, bool v) : root(r), text(t), on(v) { setWantsKeyboardFocus(true); }
    bool on; bool ring = false; std::function<void()> onChange;
    void paint(juce::Graphics& g) override {
        const Look& lk = root.look(); const float bs = 13 * Z, by = std::round((getHeight() - bs) / 2.0f), bx = 4 * Z;
        Fl b(bx, by, bs, bs);
        if (on) {
            g.setColour(juce::Colour(0xff0075ff)); g.fillRoundedRectangle(b, 2 * Z);
            juce::Path p; p.startNewSubPath(b.getX() + 0.22f * bs, b.getY() + 0.54f * bs); p.lineTo(b.getX() + 0.42f * bs, b.getY() + 0.74f * bs); p.lineTo(b.getX() + 0.78f * bs, b.getY() + 0.28f * bs);
            g.setColour(juce::Colours::white); g.strokePath(p, juce::PathStrokeType(1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        } else {
            g.setColour(juce::Colours::white); g.fillRoundedRectangle(b, 2 * Z);
            g.setColour(juce::Colour(0xff767676)); g.drawRoundedRectangle(b.reduced(0.5f), 2 * Z, 1.0f);
        }
        if (ring || hasKeyboardFocus(false)) drawRing(g, b, 2 * Z);
        const auto f = fb(19.5f, 500); g.setFont(f); g.setColour(lk[layout::lbl]);
        drawTextAt(g, text, bx + bs + 3 * Z + 8 * Z, std::round((getHeight() - nrm(f)) / 2.0f + asc(f)), 'l');
    }
    void mouseDown(const juce::MouseEvent&) override { ring = false; on = !on; repaint(); if (onChange) onChange(); }
    bool keyPressed(const juce::KeyPress& k) override { if (k == juce::KeyPress::spaceKey) { on = !on; repaint(); if (onChange) onChange(); return true; } return false; }
    void focusLost(FocusChangeType) override { ring = false; repaint(); }
private:
    Root& root; String text;
};

// Caption of a form field: `.dlg label` text.
class DCap : public juce::Component {
public:
    DCap(Root& r, const String& t, const String& extra = {}) : root(r), text(t.toUpperCase()), extra(extra.toUpperCase()) {}
    String text, extra;                                         // extra: the orange value of the parameter pages
    static constexpr float kH = 26.0f;
    void paint(juce::Graphics& g) override {
        const Look& lk = root.look(); const auto f = fc(16.7, 600, 0.1f); g.setFont(f); g.setColour(lk[layout::lbl2]);
        drawTextAt(g, text, 0, asc(f), 'l');
    }
private:
    Root& root;
};

class DValue : public juce::Component {                          // the `<b>` value in a page row
public:
    explicit DValue(Root& r) : root(r) {}
    String text;
    void setText(const String& t) { if (t != text) { text = t; repaint(); } }
    void paint(juce::Graphics& g) override {
        const auto f = fc(16.7, 700, 0.1f); g.setFont(f); g.setColour(root.look()[layout::acc]);
        drawTextAt(g, text.toUpperCase(), 0, asc(f), 'l');
    }
private:
    Root& root;
};

// Text input (single line or textarea) with the mockup's box.
class DField : public juce::Component {
public:
    DField(Root& r, const String& value, const String& placeholder, bool multiline, bool readOnly) : root(r), multi(multiline) {
        setPaintingIsUnclipped(true); addAndMakeVisible(ed);
        ed.setMultiLine(multiline, multiline); ed.setReturnKeyStartsNewLine(multiline);
        ed.setReadOnly(readOnly); ed.setScrollbarsShown(multiline); ed.setPopupMenuEnabled(true);
        ed.setBorder(juce::BorderSize<int>(0)); ed.setIndents(0, 0);
        const auto f = multiline ? fm(16.7f) : fb(19.5f, 500);
        ed.setFont(f);
        const Look& lk = root.look();
        ed.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        ed.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        ed.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        ed.setColour(juce::TextEditor::textColourId, lk[layout::selfg]);
        ed.setColour(juce::TextEditor::highlightColourId, lk[layout::acc].withAlpha(0.45f));
        ed.setColour(juce::TextEditor::highlightedTextColourId, lk[layout::selfg]);
        ed.setColour(juce::CaretComponent::caretColourId, lk[layout::selfg]);
        ed.setText(value, false);
        if (placeholder.isNotEmpty()) ed.setTextToShowWhenEmpty(placeholder, juce::Colour(0xff757575));
        ed.applyFontToAllText(f);
        ed.onTextChange = [this] { if (onChange) onChange(); };
    }
    juce::TextEditor ed; bool ring = false; std::function<void()> onChange;
    float heightCss() const { return multi ? 170.0f * Z : 45.0f; }
    void paint(juce::Graphics& g) override {
        const Look& lk = root.look(); auto r = getLocalBounds().toFloat();
        g.setColour(lk[layout::sel]); g.fillRoundedRectangle(r, 3 * Z);
        g.setColour(lk[layout::btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 3 * Z, 1.0f);
        if (ring || ed.hasKeyboardFocus(true)) drawRing(g, r, 3 * Z);
    }
    void resized() override {
        auto r = getLocalBounds().toFloat().reduced(1.0f + 8 * Z, 1.0f + 5 * Z);
        ed.setBounds(r.toNearestInt());
    }
    void mouseDown(const juce::MouseEvent&) override { ed.grabKeyboardFocus(); }
private:
    Root& root; bool multi;
};

// A <select>: the box with its chevron; the popup is a JUCE menu.
class DSel : public juce::Component {
public:
    DSel(Root& r, juce::StringArray items, int sel) : root(r), items(std::move(items)), selected(sel) { setWantsKeyboardFocus(true); setPaintingIsUnclipped(true); }
    std::function<void(int)> onChange; bool ring = false;
    int selected; juce::StringArray items;
    void setSelected(int i) { if (i != selected) { selected = i; repaint(); } }
    static constexpr float kH = 47.0f;
    void paint(juce::Graphics& g) override {
        const Look& lk = root.look(); auto r = getLocalBounds().toFloat();
        g.setColour(lk[layout::sel]); g.fillRoundedRectangle(r, 3 * Z);
        g.setColour(lk[layout::btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 3 * Z, 1.0f);
        if (hover) { g.setColour(lk[layout::hi]); g.fillRoundedRectangle(r, 3 * Z); }
        const auto f = fb(19.5f, 500, 0.05f); g.setFont(f); g.setColour(lk[layout::selfg]);
        g.saveState(); g.reduceClipRegion(r.reduced(1.0f).withTrimmedRight(26).toNearestInt());
        drawTextAt(g, juce::isPositiveAndBelow(selected, items.size()) ? items[selected] : String(), 1 + 8 * Z + 4 * Z, std::round((r.getHeight() - nrm(f)) / 2.0f + asc(f)), 'l');
        g.restoreState();
        juce::Path ch; const float cx = r.getRight() - 11.5f, cy = r.getCentreY();
        ch.startNewSubPath(cx - 4.2f, cy - 2.0f); ch.lineTo(cx, cy + 2.2f); ch.lineTo(cx + 4.2f, cy - 2.0f);
        g.setColour(lk[layout::selfg]); g.strokePath(ch, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (ring || hasKeyboardFocus(false)) drawRing(g, r, 3 * Z, lk[layout::acc], 2 * Z, 1 * Z);
    }
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) return;
        ring = false;
        juce::PopupMenu m; auto& lf = popupLnF(root.look()); m.setLookAndFeel(&lf);
        for (int k = 0; k < items.size(); ++k) m.addItem(k + 1, items[k], true, k == selected);
        auto opt = juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()).withMaximumNumColumns(1).withStandardItemHeight(26);
        m.showMenuAsync(opt, [sp = juce::Component::SafePointer<DSel>(this)](int r) {
            if (sp == nullptr || r <= 0) return;
            sp->selected = r - 1; sp->repaint(); if (sp->onChange) sp->onChange(r - 1);
        });
    }
    bool keyPressed(const juce::KeyPress& k) override {
        int d = k == juce::KeyPress::upKey ? -1 : k == juce::KeyPress::downKey ? 1 : 0;
        if (d == 0) return false;
        const int n = juce::jlimit(0, items.size() - 1, selected + d);
        if (n != selected) { selected = n; repaint(); if (onChange) onChange(n); }
        return true;
    }
    void focusLost(FocusChangeType) override { ring = false; repaint(); }
private:
    Root& root; bool hover = false;
};

// <input type=range>: Chrome's default look (blue fill, light track, round thumb).
class DRange : public juce::Component {
public:
    DRange(Root& r, int mn, int mx, int v) : root(r), lo(mn), hi(mx), value(v) { setWantsKeyboardFocus(true); }
    int lo, hi, value;
    std::function<void()> onBegin, onEnd; std::function<void(int)> onValue;
    void setValue(int v) { if (v != value && !dragging) { value = v; repaint(); } }
    static constexpr float kH = 18.0f * Z;
    void paint(juce::Graphics& g) override {
        (void) root;
        const float th = 15.5f * Z, tx0 = 2 * Z + th / 2, tx1 = getWidth() - 2 * Z - th / 2, cy = getHeight() / 2.0f;
        const float t = hi > lo ? (float) (value - lo) / (float) (hi - lo) : 0.0f, x = tx0 + (tx1 - tx0) * t, tr = 5.5f;
        const Fl track(2 * Z, cy - tr, getWidth() - 4 * Z, 2 * tr);
        g.setColour(juce::Colour(0xffefefef)); g.fillRoundedRectangle(track, tr);
        g.saveState(); g.reduceClipRegion(juce::Rectangle<int>(juce::roundToInt(x), 0, getWidth(), getHeight()));
        g.setColour(juce::Colour(0xff9fa0a5)); g.drawRoundedRectangle(track.reduced(0.5f), tr, 1.0f); g.restoreState();
        g.saveState(); g.reduceClipRegion(juce::Rectangle<int>(0, 0, juce::roundToInt(x), getHeight()));
        g.setColour(juce::Colour(0xff0075ff)); g.fillRoundedRectangle(track, tr); g.restoreState();
        g.setColour(juce::Colour(0xff0075ff)); g.fillEllipse(x - th / 2, cy - th / 2, th, th);
        if (hasKeyboardFocus(false)) drawRing(g, Fl(x - th / 2, cy - th / 2, th, th), th / 2);
    }
    void mouseDown(const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) return; dragging = true; if (onBegin) onBegin(); set(e.x); }
    void mouseDrag(const juce::MouseEvent& e) override { if (dragging) set(e.x); }
    void mouseUp(const juce::MouseEvent&) override { if (dragging) { dragging = false; if (onEnd) onEnd(); } }
    bool keyPressed(const juce::KeyPress& k) override {
        int d = (k == juce::KeyPress::rightKey || k == juce::KeyPress::upKey) ? 1 : (k == juce::KeyPress::leftKey || k == juce::KeyPress::downKey) ? -1 : 0;
        if (d == 0) return false;
        if (onBegin) onBegin(); apply(juce::jlimit(lo, hi, value + d)); if (onEnd) onEnd(); return true;
    }
private:
    Root& root; bool dragging = false;
    void set(int px) {
        const float th = 15.5f * Z, tx0 = 2 * Z + th / 2, tx1 = getWidth() - 2 * Z - th / 2;
        const float t = juce::jlimit(0.0f, 1.0f, ((float) px - tx0) / (tx1 - tx0));
        apply(lo + juce::roundToInt(t * (float) (hi - lo)));
    }
    void apply(int v) { if (v == value) return; value = v; repaint(); if (onValue) onValue(v); }
};

// Paragraphs with <b> and boxed key names (span.kb2); wraps like the browser, line-height 1.45.
class Para : public juce::Component {
public:
    enum Kind { N, B, K };
    struct Run { String t; Kind k; };
    Para(Root& r, std::vector<Run> rs, bool hintStyle = false) : root(r), runs(std::move(rs)), hint(hintStyle) {}
    void setRuns(std::vector<Run> rs) { runs = std::move(rs); }
    float layoutFor(float w) {
        items.clear();
        const float css = hint ? 16.7f : 19.5f; const auto fN = fb(css, 400), fB = fb(css, 600), fK = fm(16.7f, 600);
        lh = std::round(css * Z * 1.45f * 100.0f) / 100.0f; A = asc(fN); D = dsc(fN);
        const float spw = jp::textWidth(fN, "a a") - jp::textWidth(fN, "aa");
        struct Tok { String t; Kind k; bool sp; }; std::vector<Tok> toks; bool pending = false;
        for (auto& r : runs) {
            if (r.k == K) { toks.push_back({ r.t, K, pending }); pending = false; continue; }
            const String& s = r.t; int i = 0;
            while (i < s.length()) {
                if (s[i] == ' ') { pending = true; ++i; continue; }
                int j = i; while (j < s.length() && s[j] != ' ') ++j;
                toks.push_back({ s.substring(i, j), r.k, pending }); pending = false; i = j;
            }
        }
        float x = 0; int line = 0;
        for (auto& t : toks) {
            const juce::Font& f = t.k == K ? fK : t.k == B ? fB : fN;
            const float tw = jp::textWidth(f, t.t) + (t.k == K ? 2 * 5 * Z + 2.0f : 0.0f);
            float sp = (t.sp && x > 0) ? spw : 0;
            if (x > 0 && x + sp + tw > w + 0.01f) { ++line; x = 0; sp = 0; }
            items.push_back({ t.t, t.k, x + sp, line, tw });
            x += sp + tw;
        }
        nLines = items.empty() ? 0 : line + 1;
        const float h = lh * (float) nLines;
        setSize(juce::roundToInt(w), juce::roundToInt(h)); return h;
    }
    float firstBaseline() const { return (lh - (A + D)) / 2.0f + A; }
    void paint(juce::Graphics& g) override {
        const Look& lk = root.look(); const float css = hint ? 16.7f : 19.5f;
        const auto fN = fb(css, 400), fB = fb(css, 600), fK = fm(16.7f, 600);
        for (auto& it : items) {
            const float base = std::round((float) it.line * lh + (lh - (A + D)) / 2.0f + A);
            if (it.k == K) {
                Fl b(it.x, base - asc(fK) - 1.0f, it.w, nrm(fK) + 2.0f);
                g.setColour(lk[layout::sel]); g.fillRoundedRectangle(b, 3 * Z);
                g.setColour(lk[layout::btnbd]); g.drawRoundedRectangle(b.reduced(0.5f), 3 * Z, 1.0f);
                g.setFont(fK); g.setColour(lk[layout::lbl]); drawTextAt(g, it.t, it.x + 1 + 5 * Z, base, 'l');
            } else {
                g.setFont(it.k == B ? fB : fN); g.setColour(hint ? lk[layout::lbl2] : lk[layout::lbl]); drawTextAt(g, it.t, it.x, base, 'l');
            }
        }
    }
private:
    struct Item { String t; Kind k; float x; int line; float w; };
    Root& root; std::vector<Run> runs; bool hint; std::vector<Item> items; float lh = 0, A = 0, D = 0; int nLines = 0;
};

class BulletList : public juce::Component {
public:
    explicit BulletList(Root& r) : root(r) {}
    void add(std::vector<Para::Run> runs) { auto p = std::make_unique<Para>(root, std::move(runs)); addAndMakeVisible(*p); items.push_back(std::move(p)); }
    float layoutFor(float w) {
        float y = 0;
        for (auto& p : items) { const float h = p->layoutFor(w - 18 * Z); p->setTopLeftPosition(juce::roundToInt(18 * Z), juce::roundToInt(y)); y += h; }
        setSize(juce::roundToInt(w), juce::roundToInt(y)); return y;
    }
    void paint(juce::Graphics& g) override {
        g.setColour(root.look()[layout::lbl]);
        for (auto& p : items) {
            const float base = (float) p->getY() + p->firstBaseline(), d = 6.5f;
            g.fillEllipse(18 * Z - 14.5f - d / 2, base - 7.4f - d / 2, d, d);
        }
    }
private:
    Root& root; std::vector<std::unique_ptr<Para>> items;
};

class Chip : public juce::Component {
public:
    Chip(Root& r, const String& t, const String& key) : root(r), text(t.toUpperCase()), id(key) { setPaintingIsUnclipped(true); }
    String text, id; bool on = false; bool ring = false; std::function<void()> onClick;
    static juce::Font font() { return fc(16.7, 600, 0.06f); }
    float prefWidth() const { return 2.0f + 2 * 9 * Z + jp::textWidth(font(), text); }
    static constexpr float kH = 2.0f + 2 * Z + 26.0f;
    void paint(juce::Graphics& g) override {
        const Look& lk = root.look(); auto r = getLocalBounds().toFloat();
        g.setColour(on ? lk[layout::acc] : lk[layout::sel]); g.fillRoundedRectangle(r, 11 * Z);
        g.setColour(on ? lk[layout::acc] : lk[layout::btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 11 * Z, 1.0f);
        const auto f = font(); g.setFont(f); g.setColour(on ? kGoText : lk[layout::lbl]);
        drawTextAt(g, text, r.getX() + 1 + 9 * Z, std::round(r.getY() + 1 + Z + asc(f)), 'l');
        if (ring) drawRing(g, r, 11 * Z);
    }
    void mouseUp(const juce::MouseEvent& e) override { if (contains(e.getPosition()) && !e.mods.isPopupMenu() && onClick) onClick(); }
private:
    Root& root;
};

// Flow of chips with wrapping (.chips: gap 4)
float layoutChips(std::vector<std::unique_ptr<Chip>>& chips, float w) {
    float x = 0, y = 0; const float gap = 4 * Z;
    for (auto& c : chips) {
        const float cw = c->prefWidth();
        if (x > 0 && x + cw > w + 0.01f) { x = 0; y += Chip::kH + gap; }
        c->setBounds(Fl(x, y, cw, Chip::kH).toNearestInt()); x += cw + gap;
    }
    return chips.empty() ? 0.0f : y + Chip::kH;
}

} // namespace

// ================================================================================================= dialog frame
namespace jp {

class DialogFrame : public juce::Component {
public:
    DialogFrame(Root& r, const String& t, float wCss, bool page) : root(r), title(t.toUpperCase()), widthCss(wCss), pageMode(page) {
        addAndMakeVisible(clip); clip.setInterceptsMouseClicks(false, true); clip.addAndMakeVisible(content); content.setPaintingIsUnclipped(true);
        setWantsKeyboardFocus(false);
    }
    Root& root; String title; float widthCss; bool pageMode;
    juce::Component clip, content;
    std::vector<std::unique_ptr<DBtn>> buttons;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::function<float(float)> layoutBody;              // positions the children inside `content`; returns the content height
    std::function<void()> onRefresh;
    float bodyW = 0, contentH = 0, scrollY = 0, viewH = 0;
    std::unique_ptr<juce::FileChooser> chooser;

    template <class T, class... A> T* add(A&&... a) {
        auto p = std::make_unique<T>(std::forward<A>(a)...); T* raw = p.get(); content.addAndMakeVisible(*p); owned.push_back(std::move(p)); return raw;
    }
    DBtn* button(const String& text, bool go, std::function<void()> fn) {
        auto b = std::make_unique<DBtn>(root, text, go); b->onClick = std::move(fn); addAndMakeVisible(*b); buttons.push_back(std::move(b)); return buttons.back().get();
    }

    // size and place the dialog inside the layer
    void fit(juce::Rectangle<int> layer) {
        const float W = std::min(widthCss * Z + 2.0f, 0.9f * (float) layer.getWidth());
        bodyW = W - 2.0f - 2 * kPadX;
        contentH = layoutBody ? layoutBody(bodyW) : 0.0f;
        const float maxH = pageMode ? 420 * Z + 2 : 0.88f * (float) layer.getHeight() + 2.0f;
        const float chrome = 2.0f + titleH() + footH();
        viewH = std::min(contentH + 2 * kPadY, maxH - chrome);
        const float H = chrome + viewH;
        const float x = ((float) layer.getWidth() - W) / 2.0f, y = pageMode ? 66.0f : ((float) layer.getHeight() - H) / 2.0f;
        setBounds(Fl(layer.getX() + x, layer.getY() + y, W, H).toNearestInt());
        const float bodyTop = 1.0f + titleH();
        clip.setBounds(Fl(1.0f, bodyTop, W - 2.0f, viewH).toNearestInt());
        scrollY = juce::jlimit(0.0f, std::max(0.0f, contentH + 2 * kPadY - viewH), scrollY);
        content.setBounds(Fl(kPadX, kPadY - scrollY, bodyW, contentH).toNearestInt());
        // footer buttons: right aligned, gap 8, padding 10 x 16
        float xr = W - 1.0f - kPadX; const float fy = bodyTop + viewH + 1.0f + 10 * Z;
        for (int i = (int) buttons.size() - 1; i >= 0; --i) {
            const float bw = buttons[(size_t) i]->prefWidth();
            buttons[(size_t) i]->setBounds(Fl(xr - bw, fy, bw, 30 * Z).toNearestInt()); xr -= bw + 8 * Z;
        }
        repaint();
    }
    static float titleH() { return 2 * kPadY + 44.0f + 1.0f; }   // h2: padding 12 + line 44 + 1px rule
    static float footH() { return 2 * 10 * Z + 30 * Z + 1.0f; }

    void paint(juce::Graphics& g) override {
        const Look& lk = root.look(); auto r = getLocalBounds().toFloat();
        g.setColour(lk[layout::hdr]); g.fillRoundedRectangle(r, 6 * Z);
        g.setColour(lk[layout::acc]); g.drawRoundedRectangle(r.reduced(0.5f), 6 * Z, 1.0f);
        const auto f = fc(28.2f, 700, 0.1f); g.setFont(f); g.setColour(lk[layout::ttl]);
        drawTextAt(g, title, 1 + kPadX, std::round(1 + kPadY + asc(f)), 'l');
        g.setColour(lk[layout::edge]); g.fillRect(Fl(1, 1 + titleH() - 1, r.getWidth() - 2, 1.0f));
        const float fy = 1 + titleH() + viewH; g.fillRect(Fl(1, fy, r.getWidth() - 2, 1.0f));
    }
    void paintOverChildren(juce::Graphics& g) override {
        const float total = contentH + 2 * kPadY;                  // scroll thumb, as the browser shows one when the body overflows
        if (total <= viewH + 0.5f) return;
        const float th = std::max(24.0f, viewH * viewH / total), ty = 1 + titleH() + (viewH - th) * (scrollY / (total - viewH));
        g.setColour(juce::Colours::white.withAlpha(0.28f)); g.fillRoundedRectangle(Fl((float) getWidth() - 10.0f, ty, 6.0f, th), 3.0f);
    }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override {
        const float total = contentH + 2 * kPadY; if (total <= viewH + 0.5f) return;
        scrollY = juce::jlimit(0.0f, total - viewH, scrollY - w.deltaY * 120.0f);
        content.setTopLeftPosition(content.getX(), juce::roundToInt(kPadY - scrollY));
        repaint();
    }
    bool keyPressed(const juce::KeyPress& k) override { (void) k; return false; }
    void lookChanged() { repaint(); for (auto& o : owned) o->repaint(); for (auto& b : buttons) b->repaint(); }
};

// ================================================================================================= layer
DialogLayer::DialogLayer(Root& r) : root(r) { setVisible(false); setWantsKeyboardFocus(false); }
DialogLayer::~DialogLayer() = default;

void DialogLayer::paint(juce::Graphics& g) {
    if (panel == nullptr) return;
    if (!pageMode) { g.setColour(juce::Colour::fromFloatRGBA(5 / 255.0f, 8 / 255.0f, 12 / 255.0f, 0.62f)); g.fillAll(); }
    const float a = pageMode ? 0.7f : 0.6f;
    juce::DropShadow(juce::Colours::black.withAlpha(a), juce::roundToInt(26.0f), { 0, juce::roundToInt(12 * Z) }).drawForRectangle(g, panel->getBounds());
}
void DialogLayer::resized() { if (panel != nullptr) panel->fit(getLocalBounds()); }
bool DialogLayer::hitTest(int x, int y) {
    if (panel == nullptr) return false;
    return pageMode ? panel->getBounds().contains(x, y) : true;
}
void DialogLayer::mouseDown(const juce::MouseEvent& e) {
    if (panel != nullptr && !panel->getBounds().contains(e.getPosition())) close();
}
bool DialogLayer::keyPressed(const juce::KeyPress& k) {
    if (panel != nullptr && k == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}
void DialogLayer::close() {
    if (panel == nullptr) return;
    removeChildComponent(panel.get());
    graveyard.push_back(std::move(panel));
    setVisible(false); root.repaint();
}
void DialogLayer::refresh() {
    graveyard.clear();
    if (panel != nullptr && panel->onRefresh) panel->onRefresh();
}
void DialogLayer::relayout() { if (panel != nullptr) panel->fit(getLocalBounds()); }
void DialogLayer::lookChanged() { if (panel != nullptr) panel->lookChanged(); repaint(); }

DialogFrame& DialogLayer::begin(const String& title, float widthCss, bool page) {
    close();
    panel = std::make_unique<DialogFrame>(root, title, widthCss, page);
    pageMode = page; root.hideTip();
    return *panel;
}
void DialogLayer::finish(DialogFrame& f) {
    addAndMakeVisible(f); setVisible(true); toFront(false);
    if (root.isShowing() && !root.hasKeyboardFocus(true)) root.grabKeyboardFocus();
    f.fit(getLocalBounds()); repaint();
}

namespace {
void focusIf(juce::Component* c) { if (c != nullptr && c->isShowing()) c->grabKeyboardFocus(); }
using PR = Para::Run;
PR n(const String& t) { return { t, Para::N }; }
PR b(const String& t) { return { t, Para::B }; }
PR k(const String& t) { return { t, Para::K }; }
}

// ------------------------------------------------------------------------------------------------ Guide
void DialogLayer::openGuide() {
    auto& f = begin("Guide", 640);
    auto* p0 = f.add<Para>(root, std::vector<PR>{ n("Every control has a bubble after about one second of hovering. Turn bubbles off in Settings.") });
    auto* ul = f.add<BulletList>(root);
    ul->add({ n("Drag a knob or slider up and down. Hold "), k("Shift"), n(" for fine steps. Mouse wheel and arrow keys work too; double click (or "), k("Enter"), n(") resets.") });
    ul->add({ n("Display: BANK 1 to 8 and NUMBER 1 to 8 pick a sound in the current group; GROUP DOWN and UP change group (A1 to G2, 64 sounds each); DEC and INC step one sound.") });
    ul->add({ n("EDIT then a BANK button opens a parameter page: PFM COMMON, VOICE MOD, PART, PATCH, MIDI, SETUP, INIT/UTIL, SCOPE. EXIT closes it.") });
    ul->add({ n("RIBBON / CTRL ASSIGN, then touch a knob or slider: the ribbon and the control lever now move it. The bend and mod pad springs back; the ribbon can hold its value.") });
    ul->add({ n("SOUND and FX tabs at the top: SOUND is the front panel, FX holds the trance gate.") });
    ul->add({ n("Double click the preset name to rename. Saving over a factory sound makes your own copy. WRITE opens Save.") });
    ul->add({ n("Resize from any edge or corner; the shape stays the same. Double click a wood cheek for 100%.") });
    ul->add({ n("Voices: 8 up to 128.") });
    f.layoutBody = [p0, ul](float w) { float y = 0; y += p0->layoutFor(w); p0->setTopLeftPosition(0, 0); y += kGap; ul->setTopLeftPosition(0, juce::roundToInt(y)); y += ul->layoutFor(w); return y; };
    auto* close = f.button("Close", false, [this] { this->close(); });
    close->ring = true;
    finish(f); focusIf(close);
}

// ------------------------------------------------------------------------------------------------ Settings
void DialogLayer::openSettings() {
    auto& f = begin("Settings", 640);
    auto& S = Settings::get();
    auto* bub = f.add<DChk>(root, "Show info bubbles", S.bubbles);
    auto* pow = f.add<DChk>(root, "Power saving: pause meters and displays when the window is hidden", S.power);
    auto* cap = f.add<DCap>(root, "Colour look");
    static const char* const ids[] = { "jp", "navy", "soda", "purr" };
    int cur = 0; for (int i = 0; i < 4; ++i) if (S.look == ids[i]) cur = i;
    auto* sel = f.add<DSel>(root, juce::StringArray { "JP Blue (default)", "Neon Navy", "Purple Soda Pop", "PurrOne charcoal" }, cur);
    auto* hint = f.add<Para>(root, std::vector<PR>{ n("The look is saved with the plugin settings, not with each preset. Wood cheeks, logo, bubbles and meter colours stay the same in every look.") }, true);
    f.layoutBody = [=](float w) {
        float y = 0;
        bub->setBounds(0, juce::roundToInt(y), juce::roundToInt(w), 30); y += 30 + kGap;
        pow->setBounds(0, juce::roundToInt(y), juce::roundToInt(w), 30); y += 30 + kGap;
        cap->setBounds(0, juce::roundToInt(y), juce::roundToInt(w), 26); sel->setBounds(0, juce::roundToInt(y + 26 + 3 * Z), juce::roundToInt(w), 47); y += 26 + 3 * Z + 47 + kGap;
        hint->setTopLeftPosition(0, juce::roundToInt(y)); y += hint->layoutFor(w);
        return y;
    };
    auto* done = f.button("Done", true, [this, bub, pow, sel] {
        auto& s = Settings::get(); s.bubbles = bub->on; s.power = pow->on; s.look = ids[juce::jlimit(0, 3, sel->selected)]; s.save();
        root.applyLook(s.look); this->close();
    });
    f.button("Close", false, [this] { this->close(); });
    bub->ring = true; (void) done;
    finish(f); focusIf(bub);
}

// ------------------------------------------------------------------------------------------------ About
void DialogLayer::openAbout() {
    auto& f = begin("About & Support", 640);
    auto* p1 = f.add<Para>(root, std::vector<PR>{ b("JPurrr-8000"), n(" by Studio 87. Free and open source (AGPL-3.0-or-later). Works 100% offline.") });
    auto* p2 = f.add<Para>(root, std::vector<PR>{ n("A virtual analog synth in the spirit of the late 90s JP series: Super Saw oscillators, 12 or 24 dB filter, multi effects and delay. Not affiliated with or endorsed by Roland or Arturia. All names belong to their owners.") });
    auto* ul = f.add<BulletList>(root);
    ul->add({ n("Version: v" JucePlugin_VersionString ".") });
    ul->add({ n("Your presets: "), k("Studio 87/JPurrr-8000/presets"), n(" in your user data folder. Uninstalling keeps them unless you choose to delete them.") });
    ul->add({ n("Install: Windows installer, macOS package, Linux Flatpak or plain VST3.") });
    auto* p3 = f.add<Para>(root, std::vector<PR>{ n("Support is optional. No features are locked and there are no reminders.") }, true);
    f.layoutBody = [=](float w) {
        float y = 0; for (juce::Component* c : { (juce::Component*) p1, (juce::Component*) p2 }) { auto* p = static_cast<Para*>(c); p->setTopLeftPosition(0, juce::roundToInt(y)); y += p->layoutFor(w) + kGap; }
        ul->setTopLeftPosition(0, juce::roundToInt(y)); y += ul->layoutFor(w) + kGap;
        p3->setTopLeftPosition(0, juce::roundToInt(y)); y += p3->layoutFor(w);
        return y;
    };
    auto link = [](const char* url) { return [url] { juce::URL(url).launchInDefaultBrowser(); }; };
    auto* gh = f.button("GitHub", false, link("https://github.com/SpeakingColors/JPurrr-8000"));
    f.button("Releases", false, link("https://github.com/SpeakingColors/JPurrr-8000/releases"));
    f.button("Ko-fi", false, link("https://ko-fi.com/studio87"));
    f.button("Close", false, [this] { this->close(); });
    gh->ring = true;
    finish(f); focusIf(gh);
}

// ------------------------------------------------------------------------------------------------ Save / replace / delete
void DialogLayer::openSave() {
    auto& c = root.controller();
    auto& f = begin("Save sound", 640);
    auto* capN = f.add<DCap>(root, "Name"); auto* name = f.add<DField>(root, c.pm.cur.name, String(), false, false);
    name->ed.setInputRestrictions(40);
    auto* capT = f.add<DCap>(root, "Type");
    juce::StringArray types; for (int i = 0; i < 10; ++i) types.add(String::fromUTF8(layout::kTypes[i]));
    auto* type = f.add<DSel>(root, types, std::max(0, types.indexOf(c.pm.cur.type)));
    auto* capB = f.add<DCap>(root, "Bank (A to D are factory)");
    auto* bank = f.add<DSel>(root, juce::StringArray { "E", "F", "G" }, 0);
    auto* capG = f.add<DCap>(root, "Genres, comma separated"); auto* genres = f.add<DField>(root, String(), "Trance, Rave", false, false);
    auto* capX = f.add<DCap>(root, "Your own tags"); auto* tags = f.add<DField>(root, String(), "warm, wide", false, false);
    auto* hint = f.add<Para>(root, std::vector<PR>{}, true);
    f.layoutBody = [=](float w) {
        const int W = juce::roundToInt(w); float y = 0; const float cg = 3 * Z;
        capN->setBounds(0, juce::roundToInt(y), W, 26); name->setBounds(0, juce::roundToInt(y + 26 + cg), W, 45); y += 26 + cg + 45 + kGap;
        const float colW = (w - kGap) / 2.0f;
        capT->setBounds(0, juce::roundToInt(y), juce::roundToInt(colW), 26); type->setBounds(0, juce::roundToInt(y + 26 + cg), juce::roundToInt(colW), 47);
        capB->setBounds(juce::roundToInt(colW + kGap), juce::roundToInt(y), juce::roundToInt(colW), 26); bank->setBounds(juce::roundToInt(colW + kGap), juce::roundToInt(y + 26 + cg), juce::roundToInt(colW), 47);
        y += 26 + cg + 47 + kGap;
        capG->setBounds(0, juce::roundToInt(y), W, 26); genres->setBounds(0, juce::roundToInt(y + 26 + cg), W, 45); y += 26 + cg + 45 + kGap;
        capX->setBounds(0, juce::roundToInt(y), W, 26); tags->setBounds(0, juce::roundToInt(y + 26 + cg), W, 45); y += 26 + cg + 45 + kGap;
        hint->setTopLeftPosition(0, juce::roundToInt(y)); y += hint->layoutFor(w);
        return y;
    };
    auto split = [](const String& s) { juce::StringArray a; a.addTokens(s, ",", ""); a.trim(); a.removeEmptyStrings(); return a; };
    f.button("Save", true, [=] {
        String nm = name->ed.getText().trim(); if (nm.isEmpty()) nm = "Untitled";
        const String ty = types[type->selected], bk = bank->items[bank->selected];
        auto g = split(genres->ed.getText()), x = split(tags->ed.getText());
        auto& ctl = root.controller();
        if (ctl.pm.findUserByName(nm, bk) >= 0) { openReplace(nm, ty, g, x, bk); return; }
        if (ctl.saveAs(nm, ty, g, x, bk, false)) { this->close(); ctl.say("Saved to bank " + bk); }
    });
    f.button("Close", false, [this] { this->close(); });
    name->ring = true;
    finish(f); focusIf(&name->ed);
}

void DialogLayer::openReplace(const String& name, const String& type, const juce::StringArray& genres, const juce::StringArray& tags, const String& bank) {
    auto& f = begin("Replace this sound?", 640);
    auto* p = f.add<Para>(root, std::vector<PR>{ n("There is already a sound called "), b(name), n(" in bank " + bank + ".") });
    f.layoutBody = [p](float w) { p->setTopLeftPosition(0, 0); return p->layoutFor(w); };
    auto* ow = f.button("Overwrite", true, [=] { auto& c = root.controller(); if (c.saveAs(name, type, genres, tags, bank, true)) { this->close(); c.say("Overwritten"); } });
    f.button("Save as new", false, [=] { auto& c = root.controller(); if (c.saveAs(name + " 2", type, genres, tags, bank, false)) { this->close(); c.say("Saved as new"); } });
    f.button("Cancel", false, [this] { openSave(); });
    ow->ring = true; finish(f); focusIf(ow);
}

void DialogLayer::openDelete() {
    auto& c = root.controller();
    if (c.pm.cur.factory) { c.say("Factory sounds cannot be deleted"); return; }
    auto& f = begin("Delete this sound?", 640);
    auto* p = f.add<Para>(root, std::vector<PR>{ n(c.pm.cur.name + " will be removed from bank " + c.pm.cur.bank + ".") });
    f.layoutBody = [p](float w) { p->setTopLeftPosition(0, 0); return p->layoutFor(w); };
    auto* del = f.button("Delete", true, [this] { auto& cc = root.controller(); if (cc.deleteCurrent()) this->close(); });
    f.button("Close", false, [this] { this->close(); });
    del->ring = true; finish(f); focusIf(del);
}

// ------------------------------------------------------------------------------------------------ Export / import
void DialogLayer::openExport() {
    auto& c = root.controller();
    auto& f = begin("Export preset", 640);
    auto* p = f.add<Para>(root, std::vector<PR>{ n("This is the preset as text. Save file writes .json, or .syx as a JP-8080 patch dump. Put 8000 in the file name (for example Lead-JP8000.syx) for a JP-8000 patch dump. Copy it to keep it.") });
    auto* ta = f.add<DField>(root, Controller::exportText(c.snapshot(c.pm.cur.name)), String(), true, true);
    f.layoutBody = [=](float w) { float y = 0; p->setTopLeftPosition(0, 0); y += p->layoutFor(w) + kGap; ta->setBounds(0, juce::roundToInt(y), juce::roundToInt(w), juce::roundToInt(170 * Z)); y += 170 * Z; return y; };
    auto* copy = f.button("Copy", true, [=] { juce::SystemClipboard::copyTextToClipboard(ta->ed.getText()); root.controller().say("Copied"); });
    // File export: plumbing for the SysEx helper (Controller::exportPatch)
    auto* fr = &f;
    f.button("Save file", false, [this, fr] {
        fr->chooser = std::make_unique<juce::FileChooser>("Export preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(root.controller().pm.cur.name + ".json"), "*.json;*.syx");
        fr->chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult(); if (file == juce::File()) return;
            String err; if (root.controller().exportPatch(file, err)) root.controller().say("Exported"); else root.controller().say(err);
        });
    });
    f.button("Close", false, [this] { this->close(); });
    ta->ring = true; finish(f); focusIf(&ta->ed); (void) copy;
}

void DialogLayer::openImport() {
    auto& f = begin("Import preset", 640);
    auto* p = f.add<Para>(root, std::vector<PR>{ n("Paste preset text, or press Open file for a .json, .syx (JP-8000 or JP-8080 dump) or .mid file. It is checked first: unknown keys are ignored and wrong types are rejected.") });
    auto* ta = f.add<DField>(root, String(), "{\"app\":\"JPurrr-8000\",\"format\":1,\"name\":\"...\",\"v\":{}}", true, false);
    auto* err = f.add<Para>(root, std::vector<PR>{}, true);
    f.layoutBody = [=](float w) {
        float y = 0; p->setTopLeftPosition(0, 0); y += p->layoutFor(w) + kGap;
        ta->setBounds(0, juce::roundToInt(y), juce::roundToInt(w), juce::roundToInt(170 * Z)); y += 170 * Z + kGap;
        err->setTopLeftPosition(0, juce::roundToInt(y)); y += err->layoutFor(w); return y;
    };
    auto* imp = f.button("Import", true, [=] {
        String e; if (!root.controller().importText(ta->ed.getText(), e)) { err->setRuns({ n(e) }); if (panel) panel->fit(getLocalBounds()); }
    });
    auto* fr = &f;
    f.button("Open file", false, [this, fr, err] {
        fr->chooser = std::make_unique<juce::FileChooser>("Import preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.json;*.jpurrr;*.syx;*.mid;*.midi");
        fr->chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, err](const juce::FileChooser& fc) {
            auto file = fc.getResult(); if (file == juce::File()) return;
            String e; if (!root.controller().importFile(file, e)) { err->setRuns({ n(e) }); if (panel) panel->fit(getLocalBounds()); }
        });
    });
    f.button("Close", false, [this] { this->close(); });
    ta->ring = true; finish(f); focusIf(&ta->ed);
    (void) imp;
}

// ------------------------------------------------------------------------------------------------ Preset browser
namespace {

juce::StringArray wrapText(const juce::Font& f, const String& s, float maxW) {
    juce::StringArray words, lines; words.addTokens(s, " ", ""); String cur;
    for (auto& w : words) {
        const String t = cur.isEmpty() ? w : cur + " " + w;
        if (cur.isNotEmpty() && jp::textWidth(f, t) > maxW) { lines.add(cur); cur = w; } else cur = t;
    }
    if (cur.isNotEmpty()) lines.add(cur);
    return lines;
}

// .plist: the scrolling list of sounds, with the START HERE / ALL SOUNDS captions
class PList : public juce::Component {
public:
    struct Row { bool caption; String text, type; int li; bool cur; };
    explicit PList(Root& r) : root(r) {}
    std::vector<Row> rows; String message; std::function<void(int)> onPick; float scroll = 0;
    static constexpr float kMax = 330 * Z + 2.0f, kCap = 2 * 6 * Z + 31.5f, kItem = 2 * 6 * Z + 30.0f + 1.0f;
    float messageH(float w) const { return message.isEmpty() ? 0.0f : 2 * 10 * Z + 31.5f * (float) wrapText(fb(16.7f), message, w - 2 * 10 * Z).size(); }
    float contentH() const { float y = 0; for (auto& r : rows) y += r.caption ? kCap : kItem; return y + messageH((float) getWidth() - 2.0f); }
    void paint(juce::Graphics& g) override {
        const Look& lk = root.look(); auto r = getLocalBounds().toFloat();
        g.setColour(lk[layout::sel]); g.fillRoundedRectangle(r, 3 * Z);
        g.saveState(); g.reduceClipRegion(getLocalBounds().reduced(1)); g.setOrigin(1, 1 - juce::roundToInt(scroll));
        const float w = r.getWidth() - 2; float y = 0;
        const auto fName = fb(19.5f, 500), fHint = fb(16.7f), fType = fc(15.5f, 600, 0.08f);
        for (size_t i = 0; i < rows.size(); ++i) {
            auto& row = rows[i]; const float h = row.caption ? kCap : kItem;
            if (row.caption) {
                g.setFont(fHint); g.setColour(lk[layout::lbl2]); drawTextAt(g, row.text, 10 * Z, std::round(y + 6 * Z + (31.5f - nrm(fHint)) / 2 + asc(fHint)), 'l');
            } else {
                if (row.cur) { g.setColour(lk[layout::btnbg]); g.fillRect(Fl(0, y, w, h)); }
                else if ((int) i == hot) { g.setColour(lk[layout::hi]); g.fillRect(Fl(0, y, w, h)); }
                g.setColour(lk[layout::edge]); g.fillRect(Fl(0, y + h - 1, w, 1.0f));
                g.setFont(fName); g.setColour(row.cur ? lk[layout::acc] : lk[layout::lbl]);
                drawTextAt(g, row.text, 10 * Z, std::round(y + 6 * Z + (30.0f - nrm(fName)) / 2 + asc(fName)), 'l');
                g.setFont(fType); g.setColour(lk[layout::lbl2]); drawTextAt(g, row.type.toUpperCase(), w - 10 * Z, std::round(y + 6 * Z + asc(fType)), 'r');
            }
            y += h;
        }
        if (message.isNotEmpty()) {
            g.setFont(fHint); g.setColour(lk[layout::lbl2]); float ly = y + 10 * Z;
            for (auto& l : wrapText(fHint, message, w - 20 * Z)) { drawTextAt(g, l, 10 * Z, std::round(ly + (31.5f - nrm(fHint)) / 2 + asc(fHint)), 'l'); ly += 31.5f; }
        }
        g.restoreState();
        g.setColour(lk[layout::edge]); g.drawRoundedRectangle(r.reduced(0.5f), 3 * Z, 1.0f);
        const float total = contentH(), vh = (float) getHeight() - 2;
        if (total > vh + 0.5f) { const float th = std::max(24.0f, vh * vh / total); g.setColour(juce::Colours::white.withAlpha(0.28f)); g.fillRoundedRectangle(Fl((float) getWidth() - 9.0f, 1 + (vh - th) * scroll / (total - vh), 6.0f, th), 3.0f); }
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
        const float total = contentH(), vh = (float) getHeight() - 2;
        if (total <= vh + 0.5f) { if (auto* p = getParentComponent()) p->mouseWheelMove(e.getEventRelativeTo(p), w); return; }
        scroll = juce::jlimit(0.0f, total - vh, scroll - w.deltaY * 120.0f); repaint();
    }
    void mouseMove(const juce::MouseEvent& e) override { const int h = rowAt(e.y); if (h != hot) { hot = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hot = -1; repaint(); }
    void mouseUp(const juce::MouseEvent& e) override { const int i = rowAt(e.y); if (i >= 0 && !rows[(size_t) i].caption && !e.mods.isPopupMenu() && onPick) onPick(rows[(size_t) i].li); }
private:
    Root& root; int hot = -1;
    int rowAt(int py) const {
        float y = -scroll + 1; const float yy = (float) py;
        for (size_t i = 0; i < rows.size(); ++i) { const float h = rows[i].caption ? kCap : kItem; if (yy >= y && yy < y + h) return (int) i; y += h; }
        return -1;
    }
};

struct BrowserUi {
    Root& root; DialogLayer& layer; DialogFrame& f;
    String bank { "A" }, q, typ, gen;
    std::vector<std::unique_ptr<Chip>> banks, types, genres, hist;
    DCap* cap = nullptr; DField* search = nullptr; PList* list = nullptr;
    Para *hType = nullptr, *hGenre = nullptr, *hHist = nullptr, *none = nullptr, *note = nullptr;
    float listH = 0;
    BrowserUi(Root& r, DialogLayer& l, DialogFrame& fr) : root(r), layer(l), f(fr) {}

    Chip* mk(std::vector<std::unique_ptr<Chip>>& v, const String& text, const String& key) {
        auto c = std::make_unique<Chip>(root, text, key); f.content.addAndMakeVisible(*c); v.push_back(std::move(c)); return v.back().get();
    }
    void render() {
        auto& ctl = root.controller(); auto& pm = ctl.pm; const auto& cur = pm.cur;
        const bool start = bank == "A" && q.isEmpty() && typ.isEmpty() && gen.isEmpty();
        std::vector<PList::Row> items;
        for (int i = 0; i < pm.size(); ++i) {
            const PresetEntry& e = pm.at(i); if (e.bank != bank) continue;
            if (q.isNotEmpty()) { const String hay = (e.name + " " + e.type + " " + e.genres.joinIntoString(" ") + " " + e.tags.joinIntoString(" ")).toLowerCase(); if (!hay.contains(q)) continue; }
            if (typ.isNotEmpty() && e.type != typ) continue;
            if (gen.isNotEmpty() && !e.genres.contains(gen)) continue;
            items.push_back({ false, e.name, e.type, i, e.bank == cur.bank && e.slot == cur.slot });
        }
        list->rows.clear();
        if (start) {
            list->rows.push_back({ true, "START HERE", {}, -1, false });
            for (auto& it : items) if (pm.at(it.li).start) list->rows.push_back(it);
            list->rows.push_back({ true, "ALL SOUNDS", {}, -1, false });
        }
        for (auto& it : items) list->rows.push_back(it);
        list->message = items.empty() ? (bank == "B" || bank == "C" || bank == "D" ? "Factory bank, empty for now. More sounds come later." : "Nothing here yet.") : String();
        for (auto& c : banks) c->on = c->id == bank;
        for (auto& c : types) c->on = c->id == typ;
        for (auto& c : genres) c->on = c->id == gen;
        hist.clear();
        for (auto& h : pm.history) { auto* c = mk(hist, h, h); c->onClick = [this, h] { historyPick(h); }; }
        none->setVisible(hist.empty());
        for (auto& c : banks) c->repaint();
        list->repaint();
        layer.relayout();
    }
    void historyPick(const String& h) {
        auto& pm = root.controller().pm; const String b = h.substring(0, 1); const int slot = h.substring(1).getIntValue();
        const int li = pm.find(b, slot); if (li >= 0) { root.controller().load(li); render(); }
    }
    float layout(float w) {
        const float leftW = 230 * Z, gap = 12 * Z, rx = leftW + gap, rw = w - rx;
        auto place = [&](juce::Component& c, float x, float y, float cw, float ch) { c.setBounds(Fl(x, y, cw, ch).toNearestInt()); };
        // left column
        const float by = layoutChips(banks, leftW);
        list->setSize(juce::roundToInt(leftW), list->getHeight());
        listH = std::min(PList::kMax, list->contentH() + 2.0f);
        const float ly = by + 6 * Z; place(*list, 0, ly, leftW, listH);
        const float leftBottom = ly + listH;
        // right column
        float y = 0; const float cg = 3 * Z, g8 = 8 * Z;
        place(*cap, rx, y, rw, 26); place(*search, rx, y + 26 + cg, rw, 45); y += 26 + cg + 45 + g8;
        auto block = [&](Para* head, std::vector<std::unique_ptr<Chip>>& chips) {
            head->setTopLeftPosition(juce::roundToInt(rx), juce::roundToInt(y)); const float hh = head->layoutFor(rw); y += hh;
            const float ch = layoutChips(chips, rw);
            for (auto& c : chips) c->setTopLeftPosition(c->getX() + juce::roundToInt(rx), c->getY() + juce::roundToInt(y));
            y += ch + g8;
        };
        block(hType, types); block(hGenre, genres);
        hHist->setTopLeftPosition(juce::roundToInt(rx), juce::roundToInt(y)); y += hHist->layoutFor(rw);
        if (hist.empty()) { none->setTopLeftPosition(juce::roundToInt(rx), juce::roundToInt(y)); y += none->layoutFor(rw); }
        else { const float ch = layoutChips(hist, rw); for (auto& c : hist) c->setTopLeftPosition(c->getX() + juce::roundToInt(rx), c->getY() + juce::roundToInt(y)); y += ch; }
        y += g8;
        note->setTopLeftPosition(juce::roundToInt(rx), juce::roundToInt(y)); y += note->layoutFor(rw);
        return std::max({ leftBottom, y, 310 * Z });
    }
};

} // namespace

void DialogLayer::openBrowser() {
    auto& f = begin("Preset browser", 820);
    auto ui = std::make_shared<BrowserUi>(root, *this, f);
    ui->cap = f.add<DCap>(root, "Search");
    ui->search = f.add<DField>(root, String(), "name, type, genre or tag", false, false);
    ui->list = f.add<PList>(root);
    ui->hType = f.add<Para>(root, std::vector<PR>{ n("TYPE") }, true);
    ui->hGenre = f.add<Para>(root, std::vector<PR>{ n("GENRE") }, true);
    ui->hHist = f.add<Para>(root, std::vector<PR>{ n("HISTORY (click to go back)") }, true);
    ui->none = f.add<Para>(root, std::vector<PR>{ n("Nothing yet.") }, true);
    ui->note = f.add<Para>(root, std::vector<PR>{ n("Banks A to D are factory, E to G are yours (128 slots each).") }, true);
    for (const char* b : { "A", "B", "C", "D", "E", "F", "G" }) { auto* c = ui->mk(ui->banks, b, b); c->onClick = [u = ui.get(), key = String(b)] { u->bank = key; u->list->scroll = 0; u->render(); }; }
    for (int i = 0; i < 10; ++i) { const String t = String::fromUTF8(layout::kTypes[i]); auto* c = ui->mk(ui->types, t, t); c->onClick = [u = ui.get(), t] { u->typ = u->typ == t ? String() : t; u->list->scroll = 0; u->render(); }; }
    for (int i = 0; i < 9; ++i) { const String t = String::fromUTF8(layout::kGenres[i]); auto* c = ui->mk(ui->genres, t, t); c->onClick = [u = ui.get(), t] { u->gen = u->gen == t ? String() : t; u->list->scroll = 0; u->render(); }; }
    ui->search->onChange = [u = ui.get()] { u->q = u->search->ed.getText().toLowerCase(); u->list->scroll = 0; u->render(); };
    ui->list->onPick = [u = ui.get()](int li) { u->root.controller().load(li); u->render(); };
    f.layoutBody = [ui](float w) { return ui->layout(w); };       // the lambda keeps the state alive
    // like the mockup, the browser opens on bank A whatever is loaded
    ui->render();
    finish(f);
    if (!ui->banks.empty()) { ui->banks[0]->ring = true; }
}

// ------------------------------------------------------------------------------------------------ EDIT pages
namespace {

class ScopeList : public juce::Component {
public:
    ScopeList(Root& r) : root(r) {
        for (int i = 0; i < layout::kNumScope; ++i) { const int pi = paramIndexById(layout::kScope[i]); if (pi >= 0) ids.push_back(pi); }
        last.assign(ids.size(), -99999);
    }
    static constexpr float kRowH = 1.0f * Z * 2 + 24.5f + 1.0f, kColGap = 20 * Z;
    int perCol() const { return (int) std::ceil((float) ids.size() / 3.0f); }
    float heightFor() const { return (float) perCol() * kRowH; }
    bool poll() {
        bool ch = false; auto& c = root.controller();
        for (size_t i = 0; i < ids.size(); ++i) { const int v = c.get(ids[i]); if (v != last[i]) { last[i] = v; ch = true; } }
        if (ch) repaint(); return ch;
    }
    void paint(juce::Graphics& g) override {
        const Look& lk = root.look(); auto& c = root.controller(); const auto f = fb(13), fb2 = fb(13, 600);
        const float colW = ((float) getWidth() - 2 * kColGap) / 3.0f; const auto clip = g.getClipBounds();
        for (size_t i = 0; i < ids.size(); ++i) {
            const int col = (int) i / perCol(), row = (int) i % perCol();
            const float x = (float) col * (colW + kColGap), y = (float) row * kRowH;
            if (y + kRowH < (float) clip.getY() || y > (float) clip.getBottom()) continue;
            const float base = std::round(y + Z + (24.5f - nrm(f)) / 2 + asc(f));
            g.setFont(f); g.setColour(lk[layout::lbl]); drawTextAt(g, String::fromUTF8(kParams[ids[i]].label), x, base, 'l');
            g.setFont(fb2); g.setColour(lk[layout::acc]); drawTextAt(g, c.fmt(ids[i]), x + colW, base, 'r');
            g.setColour(lk[layout::edge]); g.fillRect(Fl(x, y + kRowH - 1, colW, 1.0f));
        }
    }
private:
    Root& root; std::vector<int> ids, last;
};

struct PageRow { int pi = 0; DCap* cap = nullptr; DValue* val = nullptr; DRange* range = nullptr; DSel* sel = nullptr; };

} // namespace

void DialogLayer::openPage(int bankNo) {
    auto& c = root.controller(); c.editOn = true;
    const int i = juce::jlimit(1, 8, bankNo);
    auto& f = begin(String::fromUTF8(layout::kBanks[i - 1]) + " page", 880, true);
    if (i >= 1 && i <= 6) {
        auto rows = std::make_shared<std::vector<PageRow>>();
        for (int k = 0; layout::kParamPages[i][k] != nullptr; ++k) {
            PageRow r; r.pi = paramIndexById(layout::kParamPages[i][k]); if (r.pi < 0) continue;
            const ParamDef& d = kParams[r.pi]; r.cap = f.add<DCap>(root, String::fromUTF8(d.label));
            if (d.optCount > 0) {
                juce::StringArray o; for (int j = 0; j < d.optCount; ++j) o.add(String::fromUTF8(kOptionText[d.optStart + j]));
                r.sel = f.add<DSel>(root, o, c.get(r.pi) - d.min);
                r.sel->onChange = [this, pi = r.pi](int idx) { auto& cc = root.controller(); cc.begin(pi); cc.user(pi, kParams[pi].min + idx); cc.end(pi); };
            } else {
                r.val = f.add<DValue>(root); r.val->setText(c.fmt(r.pi));
                r.range = f.add<DRange>(root, d.min, d.max, c.get(r.pi));
                r.range->onBegin = [this, pi = r.pi] { root.controller().begin(pi); };
                r.range->onValue = [this, pi = r.pi, v = r.val](int x) { auto& cc = root.controller(); cc.user(pi, x); v->setText(cc.fmt(pi)); };
                r.range->onEnd = [this, pi = r.pi] { root.controller().end(pi); };
            }
            rows->push_back(r);
        }
        const char* hintText = i == 4 ? "The patch name and sound settings are saved with the sound. Use Rename in the header for the name."
                             : (i == 1 || i == 2 || i == 3) ? "Performance settings are saved with the sound in this plugin."
                             : i == 5 ? "MIDI settings belong to the plugin, not to a sound. In the plugin they sit in the plugin settings."
                                      : "These settings belong to the plugin, not to a sound.";
        auto* hint = f.add<Para>(root, std::vector<PR>{ n(hintText) }, true);
        f.layoutBody = [rows, hint](float w) {
            const float colW = (w - kGap) / 2.0f, cg = 28.6f; float y = 0;
            for (size_t k = 0; k < rows->size(); k += 2) {
                float rowH = 0;
                for (size_t j = k; j < std::min(k + 2, rows->size()); ++j) {
                    auto& r = (*rows)[j]; const float x = (j - k) * (colW + kGap);
                    r.cap->setBounds(Fl(x, y, colW, 26).toNearestInt());
                    if (r.sel) { r.sel->setBounds(Fl(x, y + cg, colW, 47).toNearestInt()); rowH = std::max(rowH, cg + 47.0f); }
                    else {
                        r.val->setBounds(Fl(x, y + cg, colW, 26).toNearestInt());
                        r.range->setBounds(Fl(x + 2 * Z, y + cg + 31.2f, colW - 4 * Z, DRange::kH).toNearestInt()); rowH = std::max(rowH, cg + 31.2f + DRange::kH + 2.6f);
                    }
                }
                y += rowH + kGap;
            }
            hint->setTopLeftPosition(0, juce::roundToInt(y)); y += hint->layoutFor(w); return y;
        };
        f.onRefresh = [rows, this] {
            auto& cc = root.controller();
            for (auto& r : *rows) {
                const int v = cc.get(r.pi);
                if (r.sel) r.sel->setSelected(v - kParams[r.pi].min);
                else { r.range->setValue(v); r.val->setText(cc.fmt(r.pi)); }
            }
        };
    } else if (i == 7) {
        auto* p = f.add<Para>(root, std::vector<PR>{ n("Initialize and utility functions.") });
        auto* init = f.add<DBtn>(root, "Initialize this sound", true);
        auto* hint = f.add<Para>(root, std::vector<PR>{ n("Factory reset, bulk dump and memory card of the hardware are replaced by the preset manager: Browse, Save, Export and Import.") }, true);
        init->onClick = [this] {
            auto& cc = root.controller(); const auto d = cc.pm.defaults();
            for (int k = 0; k < kNumParams; ++k) if (!(kParams[k].flags & kNoSave)) cc.quiet(k, d[(size_t) k]);
            cc.afterLoad(); cc.markDirty(); this->close(); cc.say("Sound initialized");
        };
        f.layoutBody = [p, init, hint](float w) {
            float y = 0; p->setTopLeftPosition(0, 0); y += p->layoutFor(w) + kGap;
            init->setBounds(Fl(0, y, init->prefWidth(), 30 * Z).toNearestInt()); y += 30 * Z + kGap;
            hint->setTopLeftPosition(0, juce::roundToInt(y)); y += hint->layoutFor(w); return y;
        };
    } else {
        auto* hint = f.add<Para>(root, std::vector<PR>{ n("Scope: the current value of every panel control (the real hardware shows one value at a time while you move a control).") }, true);
        auto* list = f.add<ScopeList>(root);
        f.layoutBody = [hint, list](float w) {
            float y = 0; hint->setTopLeftPosition(0, 0); y += hint->layoutFor(w) + 13 * Z;
            list->setBounds(Fl(0, y, w, list->heightFor()).toNearestInt()); y += list->heightFor(); return y;
        };
        f.onRefresh = [list] { list->poll(); };
    }
    f.button("Close", false, [this] { this->close(); });
    finish(f);
}

} // namespace jp
