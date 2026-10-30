#include "Theme.h"
#include "Fonts.h"
using namespace jp;
using namespace jp::layout;

const Look& jp::lookByIndex(int i) {
    static Look looks[kNumLooks]; static bool init = false;
    if (!init) {
        for (int l = 0; l < kNumLooks; ++l)
            for (int t = 0; t < kNumToks; ++t) looks[l].c[t] = juce::Colour((juce::uint32) kLooks[l][t]);
        init = true;
    }
    return looks[juce::jlimit(0, kNumLooks - 1, i)];
}
int jp::lookIndexOf(const juce::String& id) {
    for (int i = 0; i < kNumLooks; ++i) if (id == kLookIds[i]) return i;
    return 0;
}
Palette jp::paletteOf(const Look& l) {
    Palette p;
    p.bg = l[dsp]; p.line = l[dspline]; p.fill = l[dspfill]; p.grid = l[dspgrid]; p.acc = l[dspaccent]; p.txt = l[lbl2]; p.edge = l[edge];
    return p;
}

juce::Font jp::fontOf(layout::Fam fam, float px, int weight, float ls) {
    juce::Font f = fam == kBody ? fonts::body(px, weight) : fam == kCond ? fonts::cond(px, weight) : fonts::mono(px, weight);
    if (ls != 0.0f) f = f.withExtraKerningFactor(ls * px / f.getHeight());   // JUCE tracking is relative to the font height, CSS to the em size
    return f;
}
juce::Font jp::fontOf(const layout::Font& f) { return fontOf(f.fam, f.size, f.weight, f.ls); }

float jp::textWidth(const juce::Font& f, const juce::String& s) {
    juce::GlyphArrangement ga; ga.addLineOfText(f, s, 0, 0);
    return ga.getBoundingBox(0, -1, true).getWidth();
}

void jp::drawTextAt(juce::Graphics& g, const juce::String& s, float x, float base, char al, bool italic) {
    if (italic) {   // the mockup fonts have no italic: the browser slants them by 0.25 (synthetic oblique)
        g.saveState();
        g.addTransform(juce::AffineTransform::translation(-x, -base).sheared(-0.25f, 0.0f).translated(x, base));
        drawTextAt(g, s, x, base, al, false);
        g.restoreState();
        return;
    }
    g.drawSingleLineText(s, juce::roundToInt(x), juce::roundToInt(base),
        al == 'c' ? juce::Justification::horizontallyCentred : al == 'r' ? juce::Justification::right : juce::Justification::left);
}

void jp::drawSection(juce::Graphics& g, const Look& L, const layout::Sec& s) {
        const float sx0 = std::round(s.r.x), sy0 = std::round(s.r.y), sx1 = std::round(s.r.x + s.r.w), sy1 = std::round(s.r.y + s.r.h);
        juce::Rectangle<float> r(sx0, sy0, sx1 - sx0, sy1 - sy0);   // browsers snap box edges to device pixels
        g.setColour(juce::Colours::black.withAlpha(0.25f)); g.fillRoundedRectangle(r.translated(0, 1), 3.0f);
        g.setColour(L[pnl2]); g.fillRoundedRectangle(r, 3.0f);
        g.setColour(L[hi]); g.fillRect(juce::Rectangle<float>(r.getX() + 1, r.getY() + 1, r.getWidth() - 2, 1.0f));
        g.setColour(L[sbd]); g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
        // title rules: the h3 is a flex row with a 1px line before and after the text
        const float ly = std::round(s.ttl.y + s.ttl.h / 2.0f);
        g.setColour(L[ttl].withAlpha(0.45f));
        g.fillRect(s.ttl.x + 8.0f, ly, juce::jmax(0.0f, s.span.x - 8.0f - (s.ttl.x + 8.0f)), 1.0f);
        g.fillRect(s.span.x + s.span.w + 8.0f, ly, juce::jmax(0.0f, s.ttl.x + s.ttl.w - 8.0f - (s.span.x + s.span.w + 8.0f)), 1.0f);
}

void jp::drawLed(juce::Graphics& g, juce::Rectangle<float> r, bool on, const Look& L) {
    if (on) {
        // box-shadow: 0 0 6px var(--led)
        juce::ColourGradient glow(L[led].withAlpha(0.55f), r.getCentre(), L[led].withAlpha(0.0f), r.getCentre().translated(r.getWidth() / 2 + 6.0f, 0), true);
        g.setGradientFill(glow); g.fillEllipse(r.expanded(6.0f));
        g.setColour(L[led]); g.fillEllipse(r);
    } else {
        g.setColour(L[ledoff]); g.fillEllipse(r);
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.drawEllipse(r.reduced(0.5f), 1.0f);
    }
}

void jp::drawHeaderButton(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, const Look& L, bool on, bool hover, bool square) {
    (void) square;
    auto bg = on ? L[acc] : L[btnbg];
    if (hover && !on) bg = bg.brighter(0.25f);
    g.setColour(bg); g.fillRoundedRectangle(r, 4.0f);
    g.setColour(L[hi]); g.fillRect(juce::Rectangle<float>(r.getX() + 1, r.getY() + 1, r.getWidth() - 2, 1.0f));   // inset 0 1px 0 var(--hi)
    g.setColour(on ? L[acc] : L[btnbd]); g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
    (void) text;
}

juce::Path jp::catPath() {
    static juce::Path p = juce::Drawable::parseSVGPath("M5 5l10 8c3-1 7-1 10 0l10-8 1 19c0 8-7 13-16 13S4 32 4 24z");
    return p;
}
void jp::drawCat(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill) {
    const float s = r.getWidth() / 40.0f;
    auto t = juce::AffineTransform::scale(s).translated(r.getX(), r.getY());
    g.setColour(fill); g.fillPath(catPath(), t);
    const auto dark = juce::Colour(0xff0b1018);
    g.setColour(dark);
    g.fillEllipse(juce::Rectangle<float>(11.9f, 20.4f, 5.2f, 5.2f).transformedBy(t));
    g.fillEllipse(juce::Rectangle<float>(22.9f, 20.4f, 5.2f, 5.2f).transformedBy(t));
    juce::Path nose; nose.addTriangle(18.3f, 27.0f, 21.7f, 27.0f, 20.0f, 29.2f); g.fillPath(nose, t);
    juce::Path w;
    w.startNewSubPath(3, 26); w.lineTo(12, 25); w.startNewSubPath(3, 30); w.lineTo(12, 28);
    w.startNewSubPath(37, 26); w.lineTo(28, 25); w.startNewSubPath(37, 30); w.lineTo(28, 28);
    g.strokePath(w, juce::PathStrokeType(1.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
}

void jp::drawWood(juce::Graphics& g, juce::Rectangle<float> r, bool left) {
    // base: linear-gradient(90deg,#3d2210,#5a3419 35%,#5a3419 65%,#3d2210)
    juce::ColourGradient base(juce::Colour(0xff3d2210), r.getX(), 0, juce::Colour(0xff3d2210), r.getRight(), 0, false);
    base.addColour(0.35, juce::Colour(0xff5a3419)); base.addColour(0.65, juce::Colour(0xff5a3419));
    g.setGradientFill(base); g.fillRect(r);
    // two repeating stripe layers (periods 14 and 19 px)
    auto stripes = [&](int period, std::initializer_list<std::tuple<int, int, juce::Colour>> bands) {
        for (float x = r.getX(); x < r.getRight(); x += (float) period) {
            for (auto& b : bands) {
                const float a = x + (float) std::get<0>(b), w = (float) std::get<1>(b) - (float) std::get<0>(b);
                auto cl = juce::Rectangle<float>(a, r.getY(), w, r.getHeight()).getIntersection(r);
                if (!cl.isEmpty()) { g.setColour(std::get<2>(b)); g.fillRect(cl); }
            }
        }
    };
    stripes(14, { { 0, 1, juce::Colour::fromFloatRGBA(61 / 255.f, 34 / 255.f, 16 / 255.f, .55f) }, { 7, 9, juce::Colour::fromFloatRGBA(123 / 255.f, 74 / 255.f, 38 / 255.f, .40f) } });
    stripes(19, { { 0, 1, juce::Colour::fromFloatRGBA(0, 0, 0, .18f) }, { 11, 12, juce::Colour::fromFloatRGBA(123 / 255.f, 74 / 255.f, 38 / 255.f, .25f) } });
    // inner edge shadow: inset -6px 0 8px -4px (left cheek), inset 6px 0 8px -4px (right cheek)
    const float ex = left ? r.getRight() : r.getX();
    juce::ColourGradient sh(juce::Colours::black.withAlpha(0.45f), ex, 0, juce::Colours::transparentBlack, left ? ex - 6.0f : ex + 6.0f, 0, false);
    g.setGradientFill(sh); g.fillRect(juce::Rectangle<float>(left ? r.getRight() - 8 : r.getX(), r.getY(), 8, r.getHeight()));
    g.setColour(juce::Colours::black); g.drawRect(r, 1.0f);
}

// ------------------------------------------------------------------------------ keyboard strip (#kbd)
namespace {
juce::Rectangle<float> snapBox(const layout::Rect& r, float y0) {         // browsers snap box edges to device pixels
    const float x0 = std::round(r.x), y1 = std::round(y0 + r.y), x1 = std::round(r.x + r.w), y2 = std::round(y0 + r.y + r.h);
    return juce::Rectangle<float>(x0, y1, x1 - x0, y2 - y1);
}
}

void jp::drawKeyStripBox(juce::Graphics& g, const Look& L, float y0) {
    auto sb = snapBox(kStrip.r, y0);
    g.setColour(L[pnl3]); g.fillRoundedRectangle(sb, 6.0f);
    g.setColour(L[hi]); g.fillRect(juce::Rectangle<float>(sb.getX() + 6, sb.getY() + 1, sb.getWidth() - 12, 1.0f));
    g.setColour(L[sbd]); g.drawRoundedRectangle(sb.reduced(0.5f), 6.0f, 1.0f);
}

// ---- key tints of the PARTS band (mockup v3.5): LOWER = cyan (acc2), UPPER = amber (acc), DUAL = both. color-mix(in srgb, A p%, B) = A * p + B * (1 - p).
static juce::Colour mixc(juce::Colour a, float p, juce::Colour b) {
    return juce::Colour::fromFloatRGBA(a.getFloatRed() * p + b.getFloatRed() * (1 - p), a.getFloatGreen() * p + b.getFloatGreen() * (1 - p),
                                       a.getFloatBlue() * p + b.getFloatBlue() * (1 - p), 1.0f);
}
enum { kTintNone = 0, kTintLower = 1, kTintUpper = 2, kTintBoth = 3 };
static int tintOf(const KeyTint& t, int n) { return !t.on ? kTintNone : t.km == 2 ? (n < t.sp ? kTintLower : kTintUpper) : t.km == 1 ? kTintBoth : kTintNone; }

static void paintKeyBox(juce::Graphics& g, const Look& L, juce::Rectangle<float> r, bool black, bool on, int tint, bool sk, juce::Colour whiteBorder, bool ck = false) {
    const juce::Colour pu = L[acc], pl = L[acc2];
    if (!black) {
        juce::ColourGradient gr; const float y0 = r.getY(), y1 = r.getBottom();
        if (on && tint != kTintNone) { gr = juce::ColourGradient(juce::Colour(0xff2f7fd6), 0, y0, juce::Colour(0xff7db8f0), 0, y1, false); }
        else if (on) { gr = juce::ColourGradient(juce::Colour(0xff9fd2ff), 0, y0, juce::Colour(0xffd4ebff), 0, y1, false); }
        else if (tint == kTintBoth) {
            gr = juce::ColourGradient(mixc(pu, .42f, juce::Colour(0xffeef0f3)), 0, y0, mixc(pl, .42f, juce::Colour(0xffd9dce1)), 0, y1, false);
            gr.addColour(.36, mixc(pu, .20f, juce::Colour(0xfffbfbfc))); gr.addColour(.64, mixc(pl, .20f, juce::Colour(0xfffbfbfc)));
        } else if (tint != kTintNone) {
            const juce::Colour tc = tint == kTintLower ? pl : pu;
            gr = juce::ColourGradient(mixc(tc, .44f, juce::Colour(0xffeef0f3)), 0, y0, mixc(tc, .42f, juce::Colour(0xffd9dce1)), 0, y1, false);
            gr.addColour(.6, mixc(tc, .28f, juce::Colour(0xfffbfbfc)));
        } else { gr = juce::ColourGradient(juce::Colour(0xffeef0f3), 0, y0, juce::Colour(0xffd9dce1), 0, y1, false); gr.addColour(.6, juce::Colour(0xfffbfbfc)); }
        g.setGradientFill(gr); g.fillRect(r);
        if (sk) { g.setColour(pu); g.fillRect(r.getX() + 1, r.getBottom() - 1 - 6, r.getWidth() - 2, 6.0f); }   // inset 0 -6px 0 var(--pu)
        else if (ck) { g.setColour(juce::Colour(0xff4a82b8)); g.fillRect(r.getX() + 1, r.getBottom() - 1 - 4, r.getWidth() - 2, 4.0f); }   // v3.7 inset 0 -4px 0 #4a82b8
        g.setColour(whiteBorder);                              // 1 px borders left, right, bottom (no top)
        g.fillRect(r.getX(), r.getY(), 1.0f, r.getHeight()); g.fillRect(r.getRight() - 1, r.getY(), 1.0f, r.getHeight()); g.fillRect(r.getX(), r.getBottom() - 1, r.getWidth(), 1.0f);
    } else {
        juce::Path p; p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), 3.0f, 3.0f, false, false, true, true);
        juce::ColourGradient gr(juce::Colour(0xff22252a), 0, r.getY(), juce::Colour(0xff050607), 0, r.getBottom(), false);
        if (on) gr = juce::ColourGradient(juce::Colour(0xff4a82b8), 0, r.getY(), juce::Colour(0xff1a3550), 0, r.getBottom(), false);
        else if (tint == kTintBoth) gr = juce::ColourGradient(mixc(pu, .58f, juce::Colour(0xff22252a)), 0, r.getY(), mixc(pl, .34f, juce::Colour(0xff050607)), 0, r.getBottom(), false);
        else if (tint != kTintNone) { const juce::Colour tc = tint == kTintLower ? pl : pu; gr = juce::ColourGradient(mixc(tc, .60f, juce::Colour(0xff22252a)), 0, r.getY(), mixc(tc, .30f, juce::Colour(0xff050607)), 0, r.getBottom(), false); }
        g.setGradientFill(gr); g.fillPath(p);
        if (sk) { g.setColour(pu); g.fillRect(r.getX() + 1, r.getBottom() - 1 - 5, r.getWidth() - 2, 5.0f); }     // inset 0 -5px 0 var(--pu)
        else if (ck) { g.setColour(juce::Colour(0xff7db8f0)); g.fillRect(r.getX() + 1, r.getBottom() - 1 - 3, r.getWidth() - 2, 3.0f); }   // v3.7 inset 0 -3px 0 #7db8f0
        juce::Path q; q.addRoundedRectangle(r.getX() + 0.5f, r.getY() - 0.5f, r.getWidth() - 1.0f, r.getHeight(), 2.5f, 2.5f, false, false, true, true);
        g.setColour(juce::Colours::black); g.strokePath(q, juce::PathStrokeType(1.0f));
    }
}

void jp::drawKeyStripKeys(juce::Graphics& g, const Look& L, float y0, const bool* lit, const KeyTint& tint, int ckBase) {
    const KeyStrip& S = kStrip;
    auto kb = snapBox(S.keysRect, y0);
    g.setColour(juce::Colour(0xff0b0e13)); g.fillRoundedRectangle(kb, 3.0f);
    g.saveState();
    juce::Path clip; clip.addRoundedRectangle(kb.reduced(1.0f), 2.0f); g.reduceClipRegion(clip, {});
    for (auto& k : S.keys) {
        auto r = snapBox(k.r, y0);
        const bool on = lit != nullptr && k.note >= 0 && k.note < 128 && lit[k.note];
        paintKeyBox(g, L, r, k.black, on, tintOf(tint, k.note), tint.on && tint.km == 2 && k.note == tint.sp, juce::Colour(0xff888888), ckBase >= 0 && k.note >= ckBase && k.note < ckBase + 18);
    }
    g.setColour(juce::Colour((juce::uint32) S.labelCol)); g.setFont(fontOf(S.labelFont));
    for (auto& l : S.labels) drawTextAt(g, l.t, l.x + l.w / 2.0f, y0 + l.base, 'c');
    g.restoreState();
}

// The split line is stepped: the top part (60 percent of the height) at the edge of the first UPPER key as you see it on top, the bottom part at the white key edge.
static constexpr int kKbLo = 36, kKbHi = 96;
static bool isBlackKey(int n) { const int m = ((n % 12) + 12) % 12; return m == 1 || m == 3 || m == 6 || m == 8 || m == 10; }
static int whiteIndex(int n) { int c = 0; for (int k = kKbLo; k < n; ++k) if (!isBlackKey(k)) ++c; return c; }
static constexpr int kNumWhite = 36;
static constexpr float kKw = 100.0f / kNumWhite;
jp::SplitX jp::splitX(int sp) {
    if (sp <= kKbLo) return { 0.0f, 0.0f };
    if (sp > kKbHi) return { 100.0f, 100.0f };
    int idx = 0; for (int n = kKbLo; n < sp; ++n) if (!isBlackKey(n)) ++idx;           // first white key at or above sp
    if (isBlackKey(sp)) { const float xb = (float) (idx) * kKw; return { xb - .3f * kKw, xb }; }
    const float xb = (float) idx * kKw;
    return { isBlackKey(sp - 1) ? xb + .3f * kKw : xb, xb };
}
juce::String jp::noteNameOf(int n) {
    static const char* const nm[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String(nm[((n % 12) + 12) % 12]) + juce::String(n / 12 - 1);
}
void jp::drawSplitLines(juce::Graphics& g, const Look& L, juce::Rectangle<float> box, int sp) {
    const auto s = splitX(sp); const float W = box.getWidth(), H = box.getHeight();
    auto bar = [&](juce::Rectangle<float> r) {
        g.setColour(L[lbl]); g.fillRect(r.expanded(1.5f));          // box-shadow 0 0 0 1.5px var(--lbl)
        g.setColour(L[hdr]); g.fillRect(r);
    };
    const float xt = box.getX() + s.xt * W / 100.0f, xb = box.getX() + s.xb * W / 100.0f;
    bar({ xt - 2.0f, box.getY(), 4.0f, H * 0.6f });
    bar({ xb - 2.0f, box.getY() + H * 0.6f, 4.0f, H * 0.4f });
    if (s.xt != s.xb) bar({ std::min(xt, xb), box.getY() + H * 0.6f - 2.0f, std::abs(xt - xb), 4.0f });
}
void jp::drawMiniKeys(juce::Graphics& g, const Look& L, juce::Rectangle<float> box, const KeyTint& tint) {
    const float W = box.getWidth(), H = box.getHeight(), kw = W * kKw / 100.0f;
    g.saveState(); g.reduceClipRegion(box.toNearestInt());
    for (int n = kKbLo; n <= kKbHi; ++n) if (!isBlackKey(n)) {
        const float x = box.getX() + (float) whiteIndex(n) * kw;
        const float x0 = std::round(x), x1 = std::round(x + kw);
        paintKeyBox(g, L, { x0, box.getY(), x1 - x0, H }, false, false, tintOf(tint, n), tint.on && tint.km == 2 && n == tint.sp, juce::Colour(0xff777777));
    }
    for (int n = kKbLo; n <= kKbHi; ++n) if (isBlackKey(n)) {
        const float x = box.getX() + (float) whiteIndex(n) * kw - kw * 0.3f, bw = kw * 0.6f;
        const float x0 = std::round(x), x1 = std::round(x + bw);
        paintKeyBox(g, L, { x0, box.getY(), x1 - x0, std::round(H * 0.6f) }, true, false, tintOf(tint, n), tint.on && tint.km == 2 && n == tint.sp, juce::Colours::black);
    }
    g.restoreState();
}

// A wheel: stripes (period 7 px: 2 px edge, 5 px sbd) at 75 percent, dark top and bottom, handle. wn drives the stripes, handleWn the
// handle (it slides with a transition while the stripes jump). dragging: accent border; focus: 2 px accent outline, offset 2 px.
void jp::drawKeyStripWheel(juce::Graphics& g, const Look& L, float y0, int idx, float wn, float handleWn, bool dragging, bool focus) {
    const Wheel& w = kStrip.wheels[(size_t) idx];
    auto r = snapBox(w.r, y0);
    g.setColour(L[hi]); g.fillRoundedRectangle(r.translated(0, 1), 8.0f);
    g.setColour(L[pnl2]); g.fillRoundedRectangle(r, 8.0f);
    g.saveState();
    juce::Path pc; pc.addRoundedRectangle(r.reduced(1.0f), 7.0f); g.reduceClipRegion(pc, {});
    const float top = r.getY() + 1.0f, bot = r.getBottom() - 1.0f;
    const float shift = -std::fmod(wn * 210.0f, 7.0f);   // background-position-y = wn * -210 px
    for (float yy = top + shift - 7.0f; yy < bot; yy += 7.0f) {
        g.setColour(L[edge].withAlpha(0.75f)); g.fillRect(juce::Rectangle<float>(r.getX(), yy, r.getWidth(), 2.0f).getIntersection(juce::Rectangle<float>(r.getX(), top, r.getWidth(), bot - top)));
        g.setColour(L[sbd].withAlpha(0.75f)); g.fillRect(juce::Rectangle<float>(r.getX(), yy + 2, r.getWidth(), 5.0f).getIntersection(juce::Rectangle<float>(r.getX(), top, r.getWidth(), bot - top)));
    }
    juce::ColourGradient dk(juce::Colours::black.withAlpha(0.72f), 0, top, juce::Colours::black.withAlpha(0.72f), 0, bot, false);
    dk.addColour(0.32, juce::Colours::transparentBlack); dk.addColour(0.68, juce::Colours::transparentBlack);
    g.setGradientFill(dk); g.fillRect(juce::Rectangle<float>(r.getX(), top, r.getWidth(), bot - top));
    g.restoreState();
    if (w.line.w > 0) { g.setColour(L[lbl2].withAlpha(0.7f)); g.fillRect(w.line.x, y0 + w.line.y, w.line.w, 1.0f); }
    const float hy = y0 + w.r.y + 1.0f + 6.0f + (1.0f - handleWn) * w.travel;
    const juce::Rectangle<float> hr(w.handle.x, hy, w.handle.w, w.handle.h);
    g.setColour(L[edge]); g.fillRoundedRectangle(hr.expanded(1.0f), 3.0f);
    g.setColour(L[acc]); g.fillRoundedRectangle(hr, 2.0f);
    g.setColour(dragging ? L[acc] : L[edge]); g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
    if (focus) { g.setColour(L[acc]); g.drawRoundedRectangle(r.expanded(3.0f), 11.0f, 2.0f); }
}

void jp::drawKeyStripCaptions(juce::Graphics& g, const Look& L, float y0) {
    for (auto& t : kStrip.texts) {
        g.setColour(t.tok >= 0 ? L.c[t.tok] : juce::Colour((juce::uint32) t.col)); g.setFont(fontOf(t.f));
        for (auto& l : t.lines) drawTextAt(g, l.t, t.al == 'c' ? l.x + l.w / 2.0f : l.x, y0 + l.base, t.al == 'c' ? 'c' : 'l', t.f.italic);
    }
}
