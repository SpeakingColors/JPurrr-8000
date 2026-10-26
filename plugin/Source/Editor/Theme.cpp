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
    if (ls != 0.0f) f = f.withExtraKerningFactor(ls);
    return f;
}
juce::Font jp::fontOf(const layout::Font& f) { return fontOf(f.fam, f.size, f.weight, f.ls); }

void jp::drawTextAt(juce::Graphics& g, const juce::String& s, float x, float base, char al) {
    g.drawSingleLineText(s, juce::roundToInt(x), juce::roundToInt(base),
        al == 'c' ? juce::Justification::horizontallyCentred : al == 'r' ? juce::Justification::right : juce::Justification::left);
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
    g.strokePath(w, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
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
