#include "PageView.h"
#include "BlockPresets.h"
using namespace jp;
using namespace jp::layout;
using namespace jpurrr;

namespace {
// placeholder for a canvas whose display is not available
struct Placeholder : Display {
    Placeholder(DisplayEnv& e, const Canvas& c) : Display(e) { setBounds((int) c.r.x, (int) c.r.y, (int) c.r.w, (int) c.r.h); setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override {
        g.setColour(env.pal.bg); g.fillRoundedRectangle(getLocalBounds().toFloat(), 2.0f);
        g.setColour(env.pal.edge); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 2.0f, 1.0f);
    }
    void tick() override {}
};

bool wrapsCentred(const juce::String& id) { return id.startsWith("lb_") || id == "osv" || id == "kshv"; }

juce::StringArray wrapWords(const juce::String& s, const juce::Font& f, float maxW) {
    juce::StringArray words; words.addTokens(s, " ", ""); words.removeEmptyStrings();
    juce::StringArray lines; juce::String cur;
    for (auto& w : words) {
        auto t = cur.isEmpty() ? w : cur + " " + w;
        if (cur.isNotEmpty() && jp::textWidth(f, t) > maxW) { lines.add(cur); cur = w; } else cur = t;
    }
    if (cur.isNotEmpty()) lines.add(cur);
    return lines;
}
}

PageView::PageView(Ctx& ctx, int pi, DisplayEnv& e) : page(kPages[pi]), index(pi), x(ctx), env(e) {
    setInterceptsMouseClicks(false, true);
    setBounds(0, 0, kDesignW, page.h);

    skipText.assign((size_t) page.texts.n, 0);
    // option labels are painted by the option group (hover background sits behind the text)
    juce::Array<juce::Array<const Text*>> optLabels;
    for (int oi = 0; oi < page.opts.n; ++oi) {
        const Opts& o = page.opts[oi]; juce::Array<const Text*> lab;
        for (int j = 0; j < o.opts.n; ++j) lab.add(nullptr);
        for (int ti = 0; ti < page.texts.n; ++ti) {
            const Line& l = page.texts[ti].lines[0];
            for (int j = 0; j < o.opts.n; ++j) {
                const Rect& r = o.opts[j].r;
                if (l.x >= r.x - 1 && l.x <= r.x + r.w && l.base >= r.y && l.base <= r.y + r.h && lab[j] == nullptr) { lab.set(j, &page.texts[ti]); skipText[(size_t) ti] = 1; }
            }
        }
        optLabels.add(lab);
    }
    for (int ti = 0; ti < page.texts.n; ++ti) { const juce::String id = page.texts[ti].id; if (id == "l1" || id == "l2") skipText[(size_t) ti] = 1; }

    auto place = [&](juce::Component* c, const Rect& r) {
        const int x0 = juce::roundToInt(r.x), y0 = juce::roundToInt(r.y);     // browsers snap both edges of a box to device pixels
        c->setBounds(x0, y0, juce::roundToInt(r.x + r.w) - x0, juce::roundToInt(r.y + r.h) - y0);
        comps.add(c); addAndMakeVisible(c);
    };
    for (auto& k : page.knobs) { auto* w = new KnobW(x, paramIndexById(k.id)); ctrls.add(w); place(w, k.r); }
    for (auto& s : page.sliders) { auto* w = new SliderW(x, paramIndexById(s.id), s.hh, s.tk); ctrls.add(w); place(w, s.r); }
    for (auto& s : page.switches) {
        const int pi2 = paramIndexById(s.id);
        auto* w = new SwitchW(x, pi2, s.id, s.mom);
        if (juce::String(s.id) == "bBrowse") w->onPress = [this] { if (x.c.openBrowser) x.c.openBrowser(); };
        ctrls.add(w); place(w, s.r);
    }
    for (int oi = 0; oi < page.opts.n; ++oi) { auto* w = new OptionsW(x, paramIndexById(page.opts[oi].id), page.opts[oi], optLabels[oi]); ctrls.add(w); place(w, page.opts[oi].r); }
    for (auto& s : page.selects) {
        int pidx = -1, mp = -1;
        if (juce::String(s.mp).isNotEmpty()) { for (int m = 0; m < kNumMp; ++m) if (juce::String(kMp[m].key) == s.mp) mp = m; }
        else pidx = paramIndexById(s.id);
        auto* w = new SelectW(x, s, pidx, mp); ctrls.add(w); place(w, s.r);
    }
    for (auto& hb : page.hbtns) {
        for (int ti = 0; ti < page.texts.n; ++ti) if (juce::String(page.texts[ti].id) == hb.id) skipText[(size_t) ti] = 1;
        auto* w = new HBtnW(x, hb, findText(hb.id), 0.0f);
        const juce::String id = hb.id;
        if (id == "bEditParts") {                           // v3.5: shows or hides the PARTS band and the split marker
            w->isOn = [this] { return x.partsOpen && x.partsOpen(); };
            w->onClick = [this] { if (x.toggleParts) x.toggleParts(); };
        } else {
            w->isOn = [this, id] { return id == "tfx1" ? x.page == kChain : id == "tfx2" ? x.page == kMaster : false; };
            w->onClick = [this, id] { if (x.gotoPage) x.gotoPage(id == "tfx1" ? kChain : kMaster); };
        }
        ctrls.add(w); place(w, hb.r);
    }
    for (auto& bb : page.bbtns) {                           // v3.3: SAVE and "..." of the block presets
        for (int ti = 0; ti < page.texts.n; ++ti) if (juce::String(page.texts[ti].id) == bb.b.id) skipText[(size_t) ti] = 1;
        auto* w = new HBtnW(x, bb.b, findText(bb.b.id), 0.0f);
        const int mp = mpIndexOf(bb.mp); const bool more = bb.more;
        w->onClick = [this, w, mp, more] { if (mp < 0) return; if (more) { if (x.c.blockMore) x.c.blockMore(mp, *w); } else if (x.c.blockSave) x.c.blockSave(mp); };
        ctrls.add(w); place(w, bb.b.r);
    }
    if (page.hasLcd) { auto* w = new LcdW(x, page.lcd, findText("l1"), findText("l2")); ctrls.add(w); place(w, page.lcd.r); }
    for (auto& c : page.canvases) {
        std::unique_ptr<Display> d = makeDisplay(c.id, env);
        if (!d) d = std::make_unique<Placeholder>(env, c);
        d->setTopLeftPosition((int) std::lround(c.r.x), (int) std::lround(c.r.y));
        displays.add(d.get());
        if (juce::String(c.id) == "cv_o1") o1Display = d.get();
        if (juce::String(c.id) == "cv_qdm") mbDisplay = d.get();
        comps.add(d.release()); addAndMakeVisible(displays.getLast());
    }
}
PageView::~PageView() = default;

const Text* PageView::findText(const char* id) const {
    for (auto& t : page.texts) if (juce::String(t.id) == id) return &t;
    return nullptr;
}

void PageView::refresh() {
    for (auto* c : ctrls) c->refresh();
    auto set = [&](const char* k, const juce::String& v) { auto it = dyn.find(k); if (it == dyn.end() || it->second != v) { dyn[k] = v; repaint(); } };
    if (index == kSound) {
        const int o1 = x.c.get(P_o1w), o2 = x.c.get(P_o2w);
        set("lb_o1c1", kO1L[juce::jlimit(0, 6, o1)][0]); set("lb_o1c2", kO1L[juce::jlimit(0, 6, o1)][1]);
        set("lb_o2c1", kO2L[juce::jlimit(0, 3, o2)][0]); set("lb_o2c2", kO2L[juce::jlimit(0, 3, o2)][1]);
        set("lb_vmc1", kVmc[juce::jlimit(0, 26, x.c.get(P_vc1))]); set("lb_vmc2", kVmc[juce::jlimit(0, 26, x.c.get(P_vc2))]);
        set("osv", x.c.fmt(P_os)); set("kshv", x.c.fmt(P_ksh));
        if (o1Display) set("o1info", o1Display->infoText(0));
    }
    if (mbDisplay) for (int b = 1; b <= 4; ++b) set(("mbr" + juce::String(b)).toRawUTF8(), mbDisplay->infoText(b - 1));
}
void PageView::tickDisplays(double) { for (auto* d : displays) d->tick(); }
void PageView::lookChanged() { for (auto* c : ctrls) c->lookChanged(); repaint(); }

const Tip* PageView::tipAt(juce::Point<float> p) const {
    const Tip* best = nullptr; float bestA = 1e12f;
    for (auto& t : page.tips) {
        if (p.x >= t.r.x && p.x <= t.r.x + t.r.w && p.y >= t.r.y && p.y <= t.r.y + t.r.h) {
            const float a = t.r.w * t.r.h; if (a <= bestA) { best = &t; bestA = a; }
        }
    }
    return best;
}

void PageView::drawText(juce::Graphics& g, const Text& t, const juce::String* ov) {
    const Look& L = x.L();
    g.setColour(t.tok >= 0 ? L.c[t.tok] : juce::Colour((juce::uint32) t.col));
    const juce::Font f = fontOf(t.f); g.setFont(f);
    const juce::String id = t.id;
    if (ov == nullptr) {
        for (auto& l : t.lines) drawTextAt(g, l.t, t.al == 'c' ? l.x + l.w / 2.0f : l.x, l.base, t.al == 'c' ? 'c' : 'l', t.f.italic);
        return;
    }
    if (wrapsCentred(id)) {
        const float maxW = id.startsWith("lb_") ? 76.0f : t.r.w;
        auto lines = wrapWords((id.startsWith("lb_") ? ov->toUpperCase() : *ov), f, maxW);
        const float lh = t.f.size * 1.05f, last = t.lines[t.lines.n - 1].base, cx = t.r.x + t.r.w / 2.0f;
        for (int i = 0; i < lines.size(); ++i) drawTextAt(g, lines[i], cx, last - lh * (float) (lines.size() - 1 - i), 'c');
    } else {
        const Line& l = t.lines[0];
        drawTextAt(g, *ov, t.al == 'c' ? l.x + l.w / 2.0f : l.x, l.base, t.al == 'c' ? 'c' : 'l');
    }
}

void PageView::paint(juce::Graphics& g) {
    const Look& L = x.L();
    for (auto& s : page.secs) drawSection(g, L, s);
    for (auto& m : page.mbps) {
        const float mx0 = std::round(m.x), my0 = std::round(m.y);
        juce::Rectangle<float> r(mx0, my0, std::round(m.x + m.w) - mx0, std::round(m.y + m.h) - my0);
        g.setColour(L[pnl2]); g.fillRoundedRectangle(r, 6.0f);
        g.setColour(L[sbd]); g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    }
    for (auto& c : page.cats) drawCat(g, juce::Rectangle<float>(c.r.x, c.r.y, c.r.w, c.r.h), L[logo]);
    for (int i = 0; i < page.texts.n; ++i) {
        if (skipText[(size_t) i]) continue;
        const Text& t = page.texts[i];
        auto it = dyn.find(t.id);
        drawText(g, t, (t.id[0] != 0 && it != dyn.end()) ? &it->second : nullptr);
    }
    for (auto& v : page.vtexts) {
        // writing-mode vertical-rl rotated 180: reads bottom to top
        g.setColour(L[lbl2]); g.setFont(fontOf(v.f));
        // per-glyph placement (tracking added by hand), then the whole run turned a quarter circle
        const float cx = v.r.x + v.r.w / 2.0f, cy = v.r.y + v.r.h / 2.0f;
        const juce::Font vf = fontOf(v.f.fam, v.f.size, v.f.weight, 0.0f);
        g.setFont(vf);
        const juce::String txt = juce::String::fromUTF8(v.t);
        const float step = v.f.ls * v.f.size;
        float totalW = -step;
        for (int i = 0; i < txt.length(); ++i) totalW += vf.getStringWidthFloat(txt.substring(i, i + 1)) + step;
        const float capMid = vf.getAscent() * 0.36f;
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi).translated(cx, cy));
        float x = -totalW / 2.0f;
        for (int i = 0; i < txt.length(); ++i) {
            const juce::String ch = txt.substring(i, i + 1);
            g.drawSingleLineText(ch, juce::roundToInt(x), juce::roundToInt(capMid), juce::Justification::left);
            x += vf.getStringWidthFloat(ch) + step;
        }
        g.restoreState();
    }
}
