#include "Root.h"
#include "../PluginProcessor.h"
using namespace jp;
using namespace jp::layout;
using namespace jpurrr;

namespace jp {

// ------------------------------------------------------------------------------ bubbles (#tip and #val)
class Bubble : public juce::Component {
public:
    explicit Bubble(bool tipStyle) : isTip(tipStyle) { setInterceptsMouseClicks(false, false); setVisible(false); }
    const Look* L = &lookByIndex(0);
    static constexpr float Z = 1.2f;                       // #tip{zoom:1.2}
    void setTip(const juce::String& text, const juce::String& nowBold, const juce::String& nowRest) {
        const float fs = 18.4f * Z, maxW = 300.0f * Z;
        const auto f = fontOf(kBody, fs, 500);
        lines.clear(); juce::String cur; juce::StringArray words; words.addTokens(text, " ", ""); words.removeEmptyStrings();
        for (auto& w : words) { auto t = cur.isEmpty() ? w : cur + " " + w; if (cur.isNotEmpty() && jp::textWidth(f, t) > maxW) { lines.add(cur); cur = w; } else cur = t; }
        if (cur.isNotEmpty()) lines.add(cur);
        bold = nowBold; rest = nowRest; lineH = fs * 1.3f; fontSize = fs;
        float w = 0; for (auto& l : lines) w = juce::jmax(w, jp::textWidth(f, l));
        if (bold.isNotEmpty()) w = juce::jmax(w, jp::textWidth(fontOf(kBody, fs, 600), bold) + jp::textWidth(f, rest));
        const int n = lines.size() + (bold.isNotEmpty() ? 1 : 0);
        setSize(juce::roundToInt(std::ceil(w) + 2 * 9 * Z + 2), juce::roundToInt(lineH * (float) n + 2 * 6 * Z + 2));
    }
    void setVal(const juce::String& t) {
        text = t; const auto f = fontOf(kMono, 18.4f, 600);
        setSize(juce::roundToInt(jp::textWidth(f, t) + 14 + 2), juce::roundToInt(18.4f * 1.3f + 2 + 2));
    }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        const juce::Colour txtc(0xfff1f3f7);
        if (isTip) {
            juce::DropShadow(juce::Colours::black.withAlpha(0.5f), 14, { 0, 4 }).drawForRectangle(g, getLocalBounds().reduced(2));
            g.setColour(juce::Colour(0xff0a0d12)); g.fillRoundedRectangle(r, 4.0f);
            g.setColour((*L)[acc]); g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
            const float px = 1 + 9 * Z, py = 1 + 6 * Z;
            auto f = fontOf(kBody, fontSize, 500);
            auto baseOf = [&](int i, const juce::Font& ff) { return py + lineH * (float) i + (lineH - (ff.getAscent() + ff.getDescent())) / 2 + ff.getAscent(); };
            g.setColour(txtc); g.setFont(f);
            for (int i = 0; i < lines.size(); ++i) drawTextAt(g, lines[i], px, baseOf(i, f), 'l');
            if (bold.isNotEmpty()) {
                const int i = lines.size(); auto fb = fontOf(kBody, fontSize, 600);
                g.setColour((*L)[acc]); g.setFont(fb); drawTextAt(g, bold, px, baseOf(i, fb), 'l');
                g.setColour(txtc); g.setFont(f); drawTextAt(g, rest, px + jp::textWidth(fb, bold), baseOf(i, f), 'l');
            }
        } else {
            g.setColour(juce::Colour(0xff0a0d12)); g.fillRoundedRectangle(r, 3.0f);
            g.setColour((*L)[acc]); g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
            const auto f = fontOf(kMono, 18.4f, 600); g.setColour((*L)[acc]); g.setFont(f);
            const float lh = 18.4f * 1.3f;
            drawTextAt(g, text, 8.0f, 2.0f + (lh - (f.getAscent() + f.getDescent())) / 2 + f.getAscent(), 'l');
        }
    }
private:
    bool isTip; juce::StringArray lines; juce::String bold, rest, text; float lineH = 20, fontSize = 18;
};

// ------------------------------------------------------------------------------ preset name (#pname)
class PnameW : public Ctrl, private juce::TextEditor::Listener {
public:
    PnameW(Ctx& c, Root& r, const layout::Text* t) : Ctrl(c), root(r), txt(t) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paint(juce::Graphics& g) override {
        const Look& L = x.L(); auto r = getLocalBounds().toFloat();
        g.setColour(L[lcdbg]); g.fillRoundedRectangle(r, 3.0f);
        g.setColour(hover ? L[acc] : L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
        if (editor) return;
        g.setColour(L[lcdfg]); const auto f = fontOf(kMono, 21.3f, 500); g.setFont(f);
        const float base = txt ? txt->lines[0].base - (float) getY() : 1 + (r.getHeight() - 2 - (f.getAscent() + f.getDescent())) / 2 + f.getAscent();
        g.saveState(); g.reduceClipRegion(getLocalBounds().reduced(11, 1));
        auto s = x.c.title();
        const float maxW = r.getWidth() - 22.0f;
        if (jp::textWidth(f, s) > maxW) { while (s.length() > 1 && jp::textWidth(f, s + juce::String::fromUTF8("\xe2\x80\xa6")) > maxW) s = s.dropLastCharacters(1); s += juce::String::fromUTF8("\xe2\x80\xa6"); }
        drawTextAt(g, s, 11.0f, base, 'l');
        g.restoreState();
    }
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override { x.hideTip(); if (e.mods.isPopupMenu()) return; }
    void mouseUp(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu() || editor || !getLocalBounds().contains(e.getPosition())) return;
        juce::Timer::callAfterDelay(230, [sp = juce::Component::SafePointer<PnameW>(this), t = ++clicks] { if (sp && sp->clicks == t && !sp->editor && x_open(sp)) sp->x.c.openBrowser(); });
    }
    void mouseDoubleClick(const juce::MouseEvent&) override { ++clicks; startEdit(); }
    void startEdit() {
        if (editor) return;
        editor = std::make_unique<juce::TextEditor>();
        editor->setFont(fontOf(kMono, 21.3f, 500)); editor->setMultiLine(false); editor->setBorder({}); editor->setIndents(0, 0);
        editor->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        editor->setColour(juce::TextEditor::textColourId, x.L()[lcdfg]); editor->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        editor->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        editor->setColour(juce::TextEditor::highlightColourId, x.L()[acc].withAlpha(0.5f));
        editor->setText(x.c.pm.cur.name, false); editor->setInputRestrictions(40);
        editor->setBounds(getLocalBounds().reduced(11, 3)); editor->addListener(this);
        addAndMakeVisible(*editor); editor->grabKeyboardFocus(); editor->selectAll();
        repaint();
    }
    void textEditorReturnKeyPressed(juce::TextEditor&) override { finish(true); }
    void textEditorEscapeKeyPressed(juce::TextEditor&) override { finish(false); }
    void textEditorFocusLost(juce::TextEditor&) override { finish(true); }
    void finish(bool ok) {
        if (!editor) return;
        const auto v = editor->getText().trim(); auto e = std::move(editor); editor.reset(); removeChildComponent(e.get());
        if (ok && v.isNotEmpty() && v != x.c.pm.cur.name) x.c.renameCurrent(v);
        repaint();
    }
private:
    static bool x_open(juce::Component::SafePointer<PnameW>) { return true; }
    Root& root; const layout::Text* txt; bool hover = false; int clicks = 0; std::unique_ptr<juce::TextEditor> editor;
};

// ------------------------------------------------------------------------------ preset menu (#pmenu)
class MenuPanel : public juce::Component {
public:
    explicit MenuPanel(Ctx& c) : x(c) { setVisible(false); setPaintingIsUnclipped(true); }
    static constexpr float Z = 1.25f;
    juce::StringArray items { "Save", "Rename", "Delete", "Export", "Import" };
    std::function<void(int)> onPick;
    // The mockup positions #pmenu in unzoomed header coordinates and then applies `zoom: 1.25` to the whole element, so the
    // menu lands at 1.25 times the position of its anchor. Same here (relX, relY are relative to the centre panel).
    void popup(float relX, float relY) {
        const float w = (150 + 12 + 2) * Z, h = (6 * 2 + 32 * 5 + 3 * 4 + 2) * Z;
        const float left = kCheekL.w + Z * juce::jmax(4.0f, juce::jmin(relX, (float) kDesignW - 34 - 170)), top = Z * relY;
        setBounds(juce::Rectangle<float>(left, top, w, h).toNearestInt()); setVisible(true); toFront(false);
    }
    void paint(juce::Graphics& g) override {
        const Look& L = x.L(); auto r = getLocalBounds().toFloat();
        juce::DropShadow(juce::Colours::black.withAlpha(0.6f), 30, { 0, 10 }).drawForRectangle(g, getLocalBounds());
        g.setColour(L[hdr]); g.fillRoundedRectangle(r, 6.0f);
        g.setColour(L[acc]); g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
        for (int i = 0; i < items.size(); ++i) {
            auto b = btn(i);
            g.setColour(i == hot ? L[btnbg].brighter(0.25f) : L[btnbg]); g.fillRoundedRectangle(b, 4.0f * Z);
            g.setColour(L[hi]); g.fillRect(juce::Rectangle<float>(b.getX() + 1, b.getY() + 1, b.getWidth() - 2, 1));
            g.setColour(L[btnbd]); g.drawRoundedRectangle(b.reduced(0.5f), 4.0f * Z, 1.0f);
            g.setColour(L[lbl]); g.setFont(fontOf(kCond, 19.5f * Z, 600, 0.06f));
            const auto f = g.getCurrentFont();
            drawTextAt(g, items[i].toUpperCase(), b.getX() + 10 * Z, b.getY() + (b.getHeight() - (f.getAscent() + f.getDescent())) / 2 + f.getAscent(), 'l');
        }
    }
    juce::Rectangle<float> btn(int i) const { return { 1 + 6 * Z, 1 + 6 * Z + (float) i * (32 + 3) * Z, (float) getWidth() - 2 - 12 * Z, 32 * Z }; }
    void mouseMove(const juce::MouseEvent& e) override { int h = -1; for (int i = 0; i < items.size(); ++i) if (btn(i).contains(e.position)) h = i; if (h != hot) { hot = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hot = -1; repaint(); }
    void mouseUp(const juce::MouseEvent& e) override { for (int i = 0; i < items.size(); ++i) if (btn(i).contains(e.position)) { setVisible(false); if (onPick) onPick(i); return; } }
private:
    Ctx& x; int hot = -1;
};

} // namespace jp

// ============================================================================================== Root
Root::Root(JPurrr8000AudioProcessor& p) : proc(p), ctl(p), ctx { ctl } {
    setWantsKeyboardFocus(true);                          // Escape closes dialogs wherever the pointer is, as in the mockup
    Settings& S = Settings::get();
    ctx.lookIdx = lookIndexOf(S.look);
    ctx.showVal = [this](juce::Component& c, int i) { showValueBubble(c, i); };
    ctx.hideVal = [this] { hideValueBubble(); };
    ctx.hideTip = [this] { hideTip(); };
    ctx.gotoPage = [this](int pi) { showPage(pi); };

    env.get = [this](int i) { return ctl.get(i); };
    env.set = [this](int i, int v) { ctl.fromDisplay(i, v); };
    env.beginGesture = [this](int i) { ctl.begin(i); if (i == P_rib) ctl.ribbonBegin(); };
    env.endGesture = [this](int i) { if (i == P_rib) ctl.ribbonEnd(); ctl.end(i); };
    env.live = &p.live;
    env.pal = paletteOf(look());
    env.sampleRate = p.getSampleRate() > 0 ? p.getSampleRate() : 44100.0;
    env.ribbonTarget = [this] { return ctl.ribbonTarget(); };
    ctl.touched = [](int i) { noteTouch(i); };

    setSize(kDesignW, kPages[0].h);
    setInterceptsMouseClicks(true, true);
    for (int i = 0; i < kNumPages; ++i) { pages.add(new PageView(ctx, i, env)); addChildComponent(pages.getLast()); }

    // header buttons
    for (auto& hb : kChrome.hbtns) {
        const juce::String id = hb.id;
        auto* w = new HBtnW(ctx, hb, [&]() -> const Text* { for (auto& t : kChrome.texts) if (juce::String(t.id) == id) return &t; return nullptr; }(), 0.0f);
        if (id == "bPrev") w->onClick = [this] { ctl.step(-1); };
        else if (id == "bNext") w->onClick = [this] { ctl.step(1); };
        else if (id == "bMenu") w->onClick = [this] { showMenu(!menu->isVisible()); };
        else if (id == "tSound") { w->isOn = [this] { return page == kSound; }; w->onClick = [this] { showPage(kSound); }; }
        else if (id == "tFx") { w->isOn = [this] { return page != kSound; }; w->onClick = [this] { showPage(lastFx); }; }
        else if (id == "bGuide") w->onClick = [this] { ctl.openGuide(); };
        else if (id == "bSet") w->onClick = [this] { ctl.openSettings(); };
        else if (id == "bAbout") w->onClick = [this] { ctl.openAbout(); };
        header.add(w); addAndMakeVisible(w);
        w->setBounds(juce::Rectangle<float>(hb.r.x, hb.r.y, hb.r.w, hb.r.h).toNearestInt());
    }
    pname = std::make_unique<PnameW>(ctx, *this, [&]() -> const Text* { for (auto& t : kChrome.texts) if (juce::String(t.id) == "pname") return &t; return nullptr; }());
    addAndMakeVisible(*pname); pname->setBounds(juce::Rectangle<float>(kPname.x, kPname.y, kPname.w, kPname.h).toNearestInt());

    menu = std::make_unique<MenuPanel>(ctx); addChildComponent(*menu);
    menu->onPick = [this](int i) {
        if (i == 0) ctl.openSave(); else if (i == 1) pname->startEdit(); else if (i == 2) ctl.openDelete();
        else if (i == 3) dialogs->openExport(); else dialogs->openImport();
    };
    dialogs = std::make_unique<DialogLayer>(*this); addChildComponent(*dialogs);
    dialogs->setBounds(juce::Rectangle<int>((int) kCheekL.w, 0, kDesignW - (int) kCheekL.w - (int) kCheekR.w, kPages[0].h));
    tipBubble = std::make_unique<Bubble>(true); valBubble = std::make_unique<Bubble>(false);
    addChildComponent(*tipBubble); addChildComponent(*valBubble);
    tipBubble->L = valBubble->L = &look();

    ctl.openSave = [this] { dialogs->openSave(); };
    ctl.openBrowser = [this] { dialogs->openBrowser(); };
    ctl.openDelete = [this] { dialogs->openDelete(); };
    ctl.startRename = [this] { pname->startEdit(); };
    ctl.closeDialog = [this] { dialogs->close(); };
    ctl.openPage = [this](int b) { dialogs->openPage(b); };
    ctl.openGuide = [this] { dialogs->openGuide(); };
    ctl.openSettings = [this] { dialogs->openSettings(); };
    ctl.openAbout = [this] { dialogs->openAbout(); };

    addMouseListener(this, true);
    texture = panelTexture(look());
    showPage(kSound);
    startTimerHz(20);
}
Root::~Root() { stopTimer(); removeMouseListener(this); }

void Root::applyLook(const juce::String& id) {
    ctx.lookIdx = lookIndexOf(id); env.pal = paletteOf(look());
    tipBubble->L = valBubble->L = &look();
    texture = panelTexture(look());
    for (auto* p : pages) p->lookChanged();
    for (auto* h : header) h->lookChanged();
    dialogs->lookChanged(); repaint();
}

void Root::showPage(int p) {
    page = juce::jlimit(0, kNumPages - 1, p); if (page != kSound) lastFx = page;
    ctx.page = page;
    for (int i = 0; i < pages.size(); ++i) pages[i]->setVisible(i == page);
    setSize(kDesignW, kPages[page].h);
    dialogs->setBounds(juce::Rectangle<int>((int) kCheekL.w, 0, kDesignW - (int) kCheekL.w - (int) kCheekR.w, kPages[page].h));
    pages[page]->refresh();
    // footer moves with the page height; header buttons stay
    hideTip(); repaint();
    if (onPageChanged) onPageChanged();
}

void Root::tick(double now) {
    env.nowSeconds = now;
    pages[page]->refresh();
    for (auto* h : header) h->refresh();
    pages[page]->tickDisplays(now);
    dialogs->refresh();
    const auto info = ctl.info(), msg = ctl.message(), title = ctl.title();
    if (info != lastInfo || msg != lastMsg || title != lastTitle) { lastInfo = info; lastMsg = msg; lastTitle = title; repaint(0, 0, kDesignW, 56); pname->repaint(); }
}

juce::Image jp::panelTexture(const Look& L) {
    (void) L;
    juce::Image img(juce::Image::ARGB, 35, 35, true);
    juce::Graphics g(img);
    // radial-gradient(rgba(255,255,255,.05) 1px, transparent 1.2px) every 5 px; black .08 dots every 7 px, offset (2,3); opacity .5
    // a 1 px radius dot lights the centre pixel and its four neighbours (as the browser rasterises it)
    auto dot = [&](int cx, int cy, juce::Colour c) { g.setColour(c); for (auto o : { std::pair<int,int>{0,0}, {1,0}, {-1,0}, {0,1}, {0,-1} }) g.fillRect(cx + o.first, cy + o.second, 1, 1); };
    for (int j = 0; j < 7; ++j) for (int i = 0; i < 7; ++i) dot(i * 5 + 2, j * 5 + 2, juce::Colours::white.withAlpha(0.027f));
    for (int j = 0; j < 5; ++j) for (int i = 0; i < 5; ++i) dot(i * 7 + 5, j * 7 + 6, juce::Colours::black.withAlpha(0.04f));
    return img;
}

void Root::paint(juce::Graphics& g) {
    const Look& L = look(); const float H = (float) designHeight(), dy = ftrDy();
    // centre panel
    const juce::Rectangle<float> center(kCheekL.w, 0, (float) kDesignW - kCheekL.w - kCheekR.w, H);
    g.setColour(L[pnl]); g.fillRect(center);
    g.saveState(); g.reduceClipRegion(center.toNearestInt());
    g.setTiledImageFill(texture, (int) center.getX(), 0, 1.0f); g.fillRect(center);
    g.restoreState();
    drawWood(g, { kCheekL.x, 0, kCheekL.w, H }, true);
    drawWood(g, { kCheekR.x, 0, kCheekR.w, H }, false);
    // header
    g.setColour(L[hdr]); g.fillRect(juce::Rectangle<float>(kHdr.x, kHdr.y, kHdr.w, kHdr.h));
    g.setColour(L[edge]); g.fillRect(juce::Rectangle<float>(kHdr.x, kHdr.y + kHdr.h - 2, kHdr.w, 2));
    // footer
    g.setColour(L[hdr]); g.fillRect(juce::Rectangle<float>(kFtr.x, kFtr.y + dy, kFtr.w, kFtr.h));
    g.setColour(L[edge]); g.fillRect(juce::Rectangle<float>(kFtr.x, kFtr.y + dy, kFtr.w, 2));
    for (auto& c : kChrome.cats) drawCat(g, juce::Rectangle<float>(c.r.x, c.r.y, c.r.w, c.r.h), L[logo]);
    // header texts
    for (auto& t : kChrome.texts) {
        const juce::String id = t.id;
        if (id.isNotEmpty() && id != "sizeRO") continue;        // buttons and the name draw their own
        const bool foot = t.r.y > 1250.0f;
        const float dyy = foot ? dy : 0.0f;
        g.setColour(t.tok >= 0 ? L.c[t.tok] : juce::Colour((juce::uint32) t.col)); g.setFont(fontOf(t.f));
        if (id == "sizeRO") {
            const int w = juce::roundToInt((float) kDesignW * scale), h = juce::roundToInt(H * scale);
            const auto s = juce::String(w) + " " + juce::String::fromUTF8("\xc3\x97") + " " + juce::String(h) + " px " + juce::String::fromUTF8("\xc2\xb7") + " " + juce::String(juce::roundToInt(scale / 0.72f * 100.0f)) + "%";
            drawTextAt(g, s, t.lines[0].x + t.lines[0].w, t.lines[0].base + dyy, 'r');
            continue;
        }
        juce::String s0 = juce::String::fromUTF8(t.lines[0].t);
        if (foot) s0 = juce::String::fromUTF8("Studio 87 \xc2\xb7 JPurrr-8000 v") + JucePlugin_VersionString;
        for (auto& l : t.lines) drawTextAt(g, foot ? s0 : juce::String::fromUTF8(l.t), t.al == 'c' ? l.x + l.w / 2 : l.x, l.base + dyy, t.al == 'c' ? 'c' : 'l', t.f.italic);
    }
    // info and message
    {
        g.setColour(L[lbl2]); const auto* ti = [&]() -> const Text* { for (auto& t : kChrome.texts) if (juce::String(t.id) == "pinfo") return &t; return nullptr; }();
        if (ti) { g.setFont(fontOf(ti->f)); drawTextAt(g, ctl.info(), ti->lines[0].x, ti->lines[0].base, 'l'); }
        const auto m = ctl.message();
        if (m.isNotEmpty()) {
            g.setColour(L[acc]); const auto f = fontOf(kCond, 18.4f, 500); g.setFont(f);
            const juce::Rectangle<float> r(kMsg.x, kMsg.y, kMsg.w, kMsg.h);
            const float base = r.getY() + (r.getHeight() - (f.getAscent() + f.getDescent())) / 2 + f.getAscent();
            g.saveState(); g.reduceClipRegion(r.toNearestInt()); drawTextAt(g, m, r.getX(), base, 'l'); g.restoreState();
        }
    }
}

// ------------------------------------------------------------------------------ tips and bubbles
const Tip* Root::tipAtPoint(juce::Point<float> p, juce::Rectangle<float>&) const {
    const Tip* best = nullptr; float ba = 1e12f;
    for (auto& t : kChrome.tips) if (p.x >= t.r.x && p.x <= t.r.x + t.r.w && p.y >= t.r.y && p.y <= t.r.y + t.r.h) { const float a = t.r.w * t.r.h; if (a <= ba) { best = &t; ba = a; } }
    if (best) return best;
    return pages[page]->tipAt(p);
}
juce::String Root::tipText(const Tip& t) const { return juce::String::fromUTF8(t.tip); }

void Root::mouseMove(const juce::MouseEvent& e) {
    auto p = e.getEventRelativeTo(this).position;
    if (dialogs->isOpen()) { hideTip(); return; }
    juce::Rectangle<float> dummyRect;
    const Tip* t = tipAtPoint(p, dummyRect);
    if (t != hoverTip) { hideTip(); hoverTip = t; hoverSince = juce::Time::getMillisecondCounter(); startTimer(50); }
    hoverPos = p;
}
void Root::mouseExit(const juce::MouseEvent&) { hideTip(); hoverTip = nullptr; tipDone = nullptr; }
void Root::mouseDown(const juce::MouseEvent& e) {
    hideTip();
    if (menu->isVisible() && !e.eventComponent->isParentOf(menu.get()) && e.eventComponent != menu.get()) {
        auto p = e.getEventRelativeTo(this).position;
        if (!menu->getBounds().toFloat().contains(p) && e.eventComponent != header[2]) menu->setVisible(false);
    }
    if (e.mods.isPopupMenu()) {
        auto p = e.getEventRelativeTo(this).position;
        if (p.y < kHdr.h && !dialogs->isOpen()) showMenu(false), menu->popup(p.x - kCheekL.w, p.y + 6);
    }
}
void Root::mouseUp(const juce::MouseEvent&) {}
void Root::mouseDrag(const juce::MouseEvent&) { hideTip(); }
bool Root::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress::escapeKey && menu->isVisible()) { menu->setVisible(false); return true; }
    if (dialogs->isOpen() && dialogs->keyPressed(k)) return true;
    return false;
}
void Root::mouseDoubleClick(const juce::MouseEvent& e) {
    const auto p = e.getEventRelativeTo(this).position;
    if ((p.x < kCheekL.w || p.x >= kCheekR.x) && onDefaultSize) onDefaultSize();   // the mockup: double click a wood cheek for 100%
}

void Root::showMenu(bool show) {
    if (!show) { menu->setVisible(false); return; }
    auto* b = header[2];                                  // bMenu
    menu->popup((float) b->getX() - kCheekL.w, (float) b->getBottom() + 4.0f);
}

void Root::hideTip() { tipBubble->setVisible(false); tipDone = hoverTip; }
void Root::hideValueBubble() { valBubble->setVisible(false); }

void Root::timerCallback() {
    if (!Settings::get().bubbles || hoverTip == nullptr || dialogs->isOpen()) { stopTimer(); startTimerHz(20); return; }
    if (tipBubble->isVisible() || hoverTip == tipDone) return;
    if (juce::Time::getMillisecondCounter() - hoverSince < 1000) return;
    const Tip& t = *hoverTip;
    juce::String text = juce::String::fromUTF8(t.tip), bold, rest;
    const juce::String fid = t.forId;
    if (fid == "o1c1" || fid == "o1c2") {
        const int o1 = ctl.get(P_o1w);
        if (o1 != 0) text = juce::String("CONTROL ") + (fid == "o1c1" ? "1" : "2") + " of OSC 1: " + juce::String(kO1L[juce::jlimit(0, 6, o1)][fid == "o1c1" ? 0 : 1]).toLowerCase() + ".";
    }
    if (fid.isNotEmpty()) { const int pi = paramIndexById(fid.toRawUTF8()); if (pi >= 0) { bold = "Now: " + ctl.fmt(pi); rest = juce::String::fromUTF8(" \xc2\xb7 double click resets"); } }
    tipBubble->L = &look(); tipBubble->setTip(text, bold, rest);
    // placeNear(below)
    const float nw = (float) tipBubble->getWidth(), nh = (float) tipBubble->getHeight(), H = (float) designHeight();
    float x = t.r.x + (t.r.w - nw) / 2.0f, y = t.r.y + t.r.h + 4.0f;
    x = juce::jlimit(40.0f, juce::jmax(40.0f, (float) kDesignW - 34.0f - nw - 4.0f), x);
    if (y + nh > H - 30.0f) y = t.r.y - nh - 4.0f;
    tipBubble->setTopLeftPosition(juce::roundToInt(x), juce::roundToInt(y)); tipBubble->setVisible(true); tipBubble->toFront(false);
    stopTimer(); startTimerHz(20);
}

void Root::showValueBubble(juce::Component& c, int idx) {
    valBubble->L = &look();
    valBubble->setVal(ctl.label(idx) + "  " + ctl.fmt(idx));
    auto r = getLocalArea(&c, c.getLocalBounds()).toFloat();
    const float nw = (float) valBubble->getWidth(), nh = (float) valBubble->getHeight(), H = (float) designHeight();
    float x = r.getX() + (r.getWidth() - nw) / 2.0f, y = r.getBottom() + 4.0f;
    x = juce::jlimit(40.0f, juce::jmax(40.0f, (float) kDesignW - 34.0f - nw - 4.0f), x);
    if (y + nh > H - 30.0f) y = r.getY() - nh - 4.0f;
    valBubble->setTopLeftPosition(juce::roundToInt(x), juce::roundToInt(y)); valBubble->setVisible(true); valBubble->toFront(false);
}

bool Root::applyState(const juce::String& s) {
    if (s == "reset") { dialogs->close(); menu->setVisible(false); applyLook("jp"); ctl.editOn = false; showPage(kSound); return true; }
    if (s == "sound") { showPage(kSound); return true; }
    if (s == "chain") { showPage(kChain); return true; }
    if (s == "master") { showPage(kMaster); return true; }
    if (s.startsWith("look_")) { showPage(kSound); applyLook(s.fromFirstOccurrenceOf("look_", false, false)); return true; }
    showPage(kSound);
    if (s == "guide") dialogs->openGuide();
    else if (s == "settings") dialogs->openSettings();
    else if (s == "about") dialogs->openAbout();
    else if (s == "save") dialogs->openSave();
    else if (s == "browser") dialogs->openBrowser();
    else if (s == "export") dialogs->openExport();
    else if (s == "import") dialogs->openImport();
    else if (s == "menu") showMenu(true);
    else if (s.startsWith("page")) { ctl.editOn = true; dialogs->openPage(s.substring(4).getIntValue()); }
    else return false;
    return true;
}
