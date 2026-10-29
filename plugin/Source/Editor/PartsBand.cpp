#include "PartsBand.h"
using namespace jp;
using namespace jp::layout;
using namespace jpurrr;

namespace {
const char* const kPN[2] = { "UPPER", "LOWER" };

juce::Colour mixc(juce::Colour a, float p, juce::Colour b) {
    return juce::Colour::fromFloatRGBA(a.getFloatRed() * p + b.getFloatRed() * (1 - p), a.getFloatGreen() * p + b.getFloatGreen() * (1 - p), a.getFloatBlue() * p + b.getFloatBlue() * (1 - p), 1.0f);
}
juce::String noteOfSp(int sp) { return noteNameOf(sp); }

// fonts of the band (CSS of mockup v3.5: .pt-h b, .pt-role, .pt-ed, .pt-name, .pt-chip, .pm-h, .pm-oct)
juce::Font fTitle() { return fontOf(kCond, 23.6f, 700, 0.14f); }
juce::Font fRole() { return fontOf(kCond, 18.4f, 600, 0.04f); }
juce::Font fEd() { return fontOf(kCond, 16.7f, 700, 0.14f); }
juce::Font fName() { return fontOf(kMono, 21.3f, 500); }
juce::Font fChip() { return fontOf(kCond, 18.4f, 600, 0.06f); }
juce::Font fChipId() { return fontOf(kMono, 18.4f, 600); }
juce::Font fPill() { return fontOf(kCond, 18.4f, 700, 0.12f); }
juce::Font fOct() { return fontOf(kCond, 16.7f, 600, 0.06f); }

juce::Rectangle<float> R(const Rect& r) { return { r.x, r.y, r.w, r.h }; }
}

// ============================================================================================== one part card
namespace jp {
class PartCardW : public juce::Component {
public:
    PartCardW(PartsBand& b, int p) : band(b), part(p), geo(p == kUpper ? kParts.upper : kParts.lower) {
        setComponentID(p == kUpper ? "partU" : "partL");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }
    int part;
    bool off() const { return band.offCard(part); }
    bool edited() const {
        int o[2]; const int n = editedParts(band.ctl.get(P_km), band.ctl.get(P_ps), band.proc.editPart(), o);
        for (int i = 0; i < n; ++i) if (o[i] == part) return true;
        return false;
    }
    juce::Colour pc(const Look& L) const { return part == kUpper ? L[acc] : L[acc2]; }
    juce::Rectangle<float> boxAt(const PBox& b) const { return R(b.r); }          // inner boxes are relative to the card
    juce::Rectangle<float> nameRect() const { return R(geo.name.r); }

    juce::String roleText() const {
        const int km = band.ctl.get(P_km), sp = band.ctl.get(P_sp);
        return (km == 0 ? (part == kUpper ? "whole keyboard" : "off in SINGLE") : km == 1 ? "whole keyboard" : (part == kLower ? juce::String("below ") + noteOfSp(sp) : noteOfSp(sp) + " and above")).toUpperCase();
    }
    void paint(juce::Graphics& g) override {
        const Look& L = band.x.L(); const bool ed = edited(), isOff = off();
        const juce::Colour pcol = pc(L); auto r = getLocalBounds().toFloat();
        // background and the 2 px border
        if (ed) { g.setGradientFill(juce::ColourGradient(mixc(pcol, .15f, L[pnl3]), 0, 0, L[pnl3], 0, r.getHeight(), false)); }
        else g.setColour(L[pnl3]);
        g.fillRoundedRectangle(r, 4.0f);
        juce::Colour border = L[edge];
        if (ed) border = pcol; else if (hover && !isOff) border = mixc(pcol, .45f, L[edge]);
        g.setColour(border); g.drawRoundedRectangle(r.reduced(1.0f), 3.0f, 2.0f);
        if (ed) { g.setColour(pcol.withAlpha(0.35f)); g.drawRoundedRectangle(r.reduced(2.5f), 2.0f, 1.0f); }      // inset 0 0 0 1px
        // header: LED, name of the part, role, PANEL EDITS tag
        const auto led = boxAt(geo.led);
        if (ed) {
            juce::ColourGradient glow(pcol.withAlpha(0.55f), led.getCentre(), pcol.withAlpha(0.0f), led.getCentre().translated(led.getWidth() / 2 + 7.0f, 0), true);
            g.setGradientFill(glow); g.fillEllipse(led.expanded(7.0f)); g.setColour(pcol); g.fillEllipse(led);
        } else drawLed(g, led, false, L);
        g.setColour(pcol); g.setFont(fTitle()); drawTextAt(g, kPN[part], geo.title.r.x, geo.title.base, 'l');
        g.setColour(L[lbl2]); g.setFont(fRole()); drawTextAt(g, roleText(), geo.role.r.x, geo.role.base, 'l');
        if (ed) {
            const auto t = boxAt(geo.ed);
            g.setColour(pcol); g.fillRoundedRectangle(t, 9.0f);
            g.setColour(juce::Colour(0xff10141a)); g.setFont(fEd()); drawTextAt(g, "PANEL EDITS", t.getX() + 9.0f, geo.ed.base, 'l');
        }
        // sound name field
        const auto m = band.proc.partMeta(part);
        const auto nr = nameRect();
        g.setColour(L[lcdbg]); g.fillRoundedRectangle(nr, 3.0f);
        g.setColour(nameHover && !isOff ? L[acc] : L[btnbd]); g.drawRoundedRectangle(nr.reduced(0.5f), 3.0f, 1.0f);
        g.saveState(); g.reduceClipRegion(nr.reduced(1.0f).toNearestInt());
        g.setColour(L[lcdfg]); g.setFont(fName()); drawTextAt(g, m.name + (m.dirty ? " *" : ""), nr.getX() + 13.0f, geo.name.base, 'l');
        g.restoreState();
        // type and genre chips, then the place of the sound (inline, no gap)
        const auto row = boxAt(geo.chipsRow);
        g.saveState(); g.reduceClipRegion(row.toNearestInt());
        float cx = row.getX();
        auto chip = [&](const juce::String& s) {
            const float w = textWidth(fChip(), s) + 22.0f;
            const juce::Rectangle<float> cr(cx, geo.chip.r.y, w, geo.chip.r.h);
            g.setColour(L[sel]); g.fillRoundedRectangle(cr, 11.0f);
            g.setColour(L[btnbd]); g.drawRoundedRectangle(cr.reduced(0.5f), 11.0f, 1.0f);
            g.setColour(L[lbl]); g.setFont(fChip()); drawTextAt(g, s, cx + 11.0f, geo.chip.base, 'l');
            cx += w;
        };
        chip((m.type.isEmpty() ? juce::String("Sound") : m.type).toUpperCase());
        for (int i = 0; i < juce::jmin(2, m.genres.size()); ++i) chip(m.genres[i].toUpperCase());
        const juce::String id = m.bank + juce::String(m.slot).paddedLeft('0', 3) + juce::String::fromUTF8(" \xc2\xb7 ") + (m.factory ? "factory" : "mine");
        g.setColour(L[lbl2]); g.setFont(fChipId()); drawTextAt(g, id, cx + 11.0f, geo.chipId.base, 'l');
        g.restoreState();
        if (hasKeyboardFocus(false) && ring) { g.setColour(L[acc]); g.drawRoundedRectangle(r.expanded(4.0f), 6.0f, 2.0f); }   // :focus-visible outline 2px, offset 2px
    }
    void mouseEnter(const juce::MouseEvent& e) override { hover = true; mouseMove(e); repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; nameHover = false; repaint(); }
    void mouseMove(const juce::MouseEvent& e) override {
        const bool nh = nameRect().contains(e.position);
        if (nh != nameHover) { nameHover = nh; repaint(); }
    }
    void mouseDown(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu() && band.hideTip) band.hideTip(); }
    void mouseUp(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu() || !getLocalBounds().contains(e.getPosition())) return;
        if (!off() && nameRect().contains(e.position)) { if (band.openBrowser) band.openBrowser(part); return; }
        band.clickCard(part);
    }
    bool keyPressed(const juce::KeyPress& k) override {
        if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) { band.clickCard(part); return true; }
        return false;
    }
    void focusGained(FocusChangeType t) override { ring = t == focusChangedByTabKey; repaint(); }
    void focusLost(FocusChangeType) override { ring = false; repaint(); }
private:
    PartsBand& band; const PCard& geo; bool hover = false, nameHover = false, ring = false;
};
}

// ============================================================================================== the band
PartsBand::PartsBand(JPurrr8000AudioProcessor& p, Ctx& c) : proc(p), x(c), ctl(c.c) {
    setComponentID("parts");
    setInterceptsMouseClicks(false, true);
    for (int part : { kUpper, kLower }) {                 // cards first: the buttons sit above them
        auto* cw = new PartCardW(*this, part);
        const Rect& r = (part == kUpper ? kParts.upper : kParts.lower).r;
        const int x0 = juce::roundToInt(r.x), y0 = juce::roundToInt(r.y);
        cards.add(cw); addAndMakeVisible(cw);
        cw->setBounds(x0, y0, juce::roundToInt(r.x + r.w) - x0, juce::roundToInt(r.y + r.h) - y0);
    }
    for (int i = 0; i < kParts.btns.n; ++i) {
        auto* w = new HBtnW(x, kParts.btns[i], &kParts.btnTexts[i], 0.0f);
        const int part = i < 3 ? kLower : kUpper, kind = i % 3;       // generated order: lower (prev, next, browse), then upper
        w->onClick = [this, part, kind] {
            if (offCard(part)) return;
            if (kind == 0) ctl.stepPart(part, -1); else if (kind == 1) ctl.stepPart(part, 1); else if (openBrowser) openBrowser(part);
        };
        btns.add(w); addAndMakeVisible(w);
        const Rect& r = kParts.btns[i].r;
        const int x0 = juce::roundToInt(r.x), y0 = juce::roundToInt(r.y);
        w->setBounds(x0, y0, juce::roundToInt(r.x + r.w) - x0, juce::roundToInt(r.y + r.h) - y0);
    }
}
PartsBand::~PartsBand() = default;

void PartsBand::place() { setBounds(0, juce::roundToInt(kParts.top), kDesignW, juce::roundToInt(kParts.sec.r.h) + 1); }      // +1: the 1 px shadow under the frame
bool PartsBand::offCard(int part) const { return part == kLower && ctl.get(P_km) == 0; }

void PartsBand::clickCard(int part) {
    if (offCard(part)) { ctl.say("LOWER is only used in DUAL and SPLIT. Set KEY MODE first."); return; }
    if (ctl.get(P_km) != 0) ctl.user(P_ps, part == kLower ? 0 : 1);          // the same as PANEL SELECT
}

void PartsBand::lookChanged() { for (auto* c : cards) c->repaint(); for (auto* b : btns) b->lookChanged(); repaint(); }

void PartsBand::refresh() {
    for (auto* b : btns) b->refresh();
    juce::String s = juce::String(ctl.get(P_km)) + "|" + juce::String(ctl.get(P_ps)) + "|" + juce::String(ctl.get(P_sp)) + "|" + juce::String(proc.editPart());
    for (int p = 0; p < 2; ++p) { const auto m = proc.partMeta(p); s << "|" << m.name << m.bank << m.slot << m.type << m.genres.joinIntoString(",") << (m.dirty ? "*" : "") << (m.factory ? "f" : "m"); }
    if (s != sig) {
        sig = s; repaint();
        for (int i = 0; i < 2; ++i) {                      // LOWER is dimmed and its buttons dead in SINGLE (pointer-events:none, opacity .38)
            const bool off = offCard(i);
            cards[i]->setAlpha(off ? 0.38f : 1.0f);
            for (int k = 0; k < 3; ++k) { auto* b = btns[(i == kLower ? 0 : 3) + k]; b->setAlpha(off ? 0.38f : 1.0f); b->setInterceptsMouseClicks(!off, false); }
            cards[i]->setWantsKeyboardFocus(!off);
        }
    }
}

void PartsBand::paint(juce::Graphics& g) {
    const Look& L = x.L();
    drawSection(g, L, kParts.sec);
    g.setColour(L[ttl]); g.setFont(fTitle());
    drawTextAt(g, kParts.sec.title, kParts.sec.span.x + kParts.sec.span.w / 2.0f, kParts.sec.span.y + 24.0f, 'c');
    g.saveState(); g.reduceClipRegion(juce::Rectangle<float>(kParts.sec.r.x, kParts.sec.r.y, kParts.sec.r.w, kParts.sec.r.h).toNearestInt());
    for (int i = 0; i < 2; ++i) {                          // glow of the edited cards (0 0 10px, 22 percent)
        if (!cards[i]->isVisible()) continue;
        int o[2]; const int n = editedParts(ctl.get(P_km), ctl.get(P_ps), proc.editPart(), o); bool ed = false; for (int k = 0; k < n; ++k) ed |= o[k] == i;
        if (!ed) continue;
        const juce::Colour pcol = i == kUpper ? L[acc] : L[acc2];
        juce::DropShadow(pcol.withAlpha(0.22f), 10, { 0, 0 }).drawForRectangle(g, cards[i]->getBounds());
    }
    g.restoreState();
    // the KEYS map
    const int km = ctl.get(P_km), sp = ctl.get(P_sp), ps = ctl.get(P_ps); (void) ps;
    const auto mr = R(kParts.map.r); const auto& M = kParts.map;
    g.setColour(L[pnl3]); g.fillRoundedRectangle(mr, 4.0f);
    g.setColour(L[edge]); g.drawRoundedRectangle(mr.reduced(1.0f), 3.0f, 2.0f);
    g.saveState(); g.addTransform(juce::AffineTransform::translation(mr.getX(), mr.getY()));
    g.setColour(L[ttl]); g.setFont(fTitle()); drawTextAt(g, "KEYS", M.title.r.x, M.title.base, 'l');
    const juce::String mode = km == 0 ? "SINGLE" : km == 1 ? "DUAL" : "SPLIT";
    const float pw = textWidth(fPill(), mode) + 20.0f;
    const juce::Rectangle<float> pill(M.pill.r.x, M.pill.r.y, pw, M.pill.r.h);
    g.setColour(L[acc]); g.fillRoundedRectangle(pill, 9.0f);
    g.setColour(juce::Colour(0xff10141a)); g.setFont(fPill()); drawTextAt(g, mode, pill.getX() + 10.0f, M.pill.base, 'l');
    const juce::String txt = km == 0 ? "UPPER plays the whole keyboard" : km == 1 ? "UPPER and LOWER play every key" : "LOWER below " + noteOfSp(sp) + ", UPPER from " + noteOfSp(sp);
    g.setColour(L[lbl2]); g.setFont(fRole()); drawTextAt(g, txt.toUpperCase(), pill.getRight() + 10.0f, M.text.base, 'l');
    if (km == 2) drawTextAt(g, (juce::String("split point ") + noteOfSp(sp)).toUpperCase(), M.sp.r.x + M.sp.r.w, M.sp.base, 'r');
    const auto mini = R(M.mini.r);
    g.setColour(juce::Colour(0xff0b0e13)); g.fillRoundedRectangle(mini, 3.0f);
    const juce::Rectangle<float> pad = mini.reduced(1.0f);
    KeyTint t { true, km, sp };
    drawMiniKeys(g, L, pad, t);
    if (km == 2) drawSplitLines(g, L, pad, sp);
    g.setColour(L[edge]); g.drawRoundedRectangle(mini.reduced(0.5f), 3.0f, 1.0f);
    g.setColour(L[lbl2]); g.setFont(fOct());
    const auto oct = R(M.oct.r);
    const float kw = 100.0f / 36.0f;
    for (int n : { 36, 48, 60, 72, 84, 96 }) {
        int wi = 0; for (int k = 36; k < n; ++k) { const int mm = k % 12; if (!(mm == 1 || mm == 3 || mm == 6 || mm == 8 || mm == 10)) ++wi; }
        drawTextAt(g, noteNameOf(n), oct.getX() + ((float) wi * kw + kw / 2.0f) / 100.0f * oct.getWidth(), M.octI.base, 'c');
    }
    g.restoreState();
}

const Tip* PartsBand::tipAt(juce::Point<float> p) {
    const juce::Point<float> q(p.x, p.y - kParts.top);
    if (q.y < 0 || q.y >= kParts.sec.r.h) return nullptr;
    int n = 0; auto add = [&](juce::Rectangle<float> r, const juce::String& t) {
        tipText[n] = t.toStdString(); tipBuf[n] = { { r.getX(), r.getY() + kParts.top, r.getWidth(), r.getHeight() }, tipText[n].c_str(), "" }; ++n; };
    for (int part : { kLower, kUpper }) {
        const auto cr = cards[part]->getBounds().toFloat();
        if (!cr.contains(q)) continue;
        const juce::String pn = kPN[part];
        const bool off = offCard(part);
        const auto& geo = part == kUpper ? kParts.upper : kParts.lower;
        if (!off) {
            const char* names[3] = { "Previous sound for the ", "Next sound for the ", "Open the preset browser for the " };
            for (int k = 0; k < 3; ++k) {
                const auto br = btns[(part == kLower ? 0 : 3) + k]->getBounds().toFloat();
                if (br.contains(q)) { add(br, juce::String(names[k]) + pn + (k == 2 ? " part." : " part.")); return &tipBuf[0]; }
            }
            const auto nr = R(geo.name.r).translated(cr.getX(), cr.getY());
            if (nr.contains(q)) { add(nr, "Sound of the " + pn + " part. Click to open the preset browser for this part."); return &tipBuf[0]; }
        }
        add(cr, off ? "LOWER is only used in DUAL and SPLIT. Set KEY MODE to DUAL or SPLIT to use it."
                    : pn + " part. Click the card to let the panel edit this part (the same as PANEL SELECT). Use the arrows or BROWSE to load another sound into this part.");
        return &tipBuf[0];
    }
    if (R(kParts.map.r).contains(q)) { add(R(kParts.map.r), "Which part plays which keys. KEY MODE and the split point are set on the panel (KEY & PANEL) and on the keyboard strip."); return &tipBuf[0]; }
    return nullptr;
}
