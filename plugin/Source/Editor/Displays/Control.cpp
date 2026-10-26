// Performance displays: cv_pad (bend / mod pad), cv_rib (ribbon), cv_gate (trance gate steps), cv_m (output meter).
#include "Common.h"

namespace jp::disp {
using namespace jpurrr;

namespace {
juce::Colour hex(juce::uint32 c) { return juce::Colour(0xff000000u | c); }
int rnd(float x) { return (int) std::floor(x + 0.5f); }

// ---------------------------------------------------------------- cv_pad
class Pad : public CanvasDisplay {
public:
    explicit Pad(DisplayEnv& e) : CanvasDisplay(e, 112, 112) {}
    void draw(Ctx& x, float w, float h) override {
        x.strokeStyle(x.pal.grid); x.lineWidth = 1; x.beginPath(); x.moveTo(w / 2 + 0.5f, 0); x.lineTo(w / 2 + 0.5f, h); x.moveTo(0, h - 0.5f); x.lineTo(w, h - 0.5f); x.stroke();
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText("BEND", 3, 10); x.textAlign(1); x.fillText("MOD", w - 3, 10);
        const float px = (v(P_bend) + 64) / 127.0f * w, py = (1 - v(P_mod) / 127.0f) * h;
        x.fillStyle(x.pal.acc); x.fillCircle(clampv(px, 5.0f, w - 5), clampv(py, 5.0f, h - 5), 5);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        on = true; if (env.beginGesture) { env.beginGesture(P_bend); env.beginGesture(P_mod); }
        setFrom(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override { if (on) setFrom(e); }
    void mouseUp(const juce::MouseEvent&) override {
        if (!on) return; on = false;       // springs back like the mockup
        env.set(P_bend, 0); env.set(P_mod, 0);
        if (env.endGesture) { env.endGesture(P_bend); env.endGesture(P_mod); }
        repaint();
    }
private:
    bool on = false;
    void setFrom(const juce::MouseEvent& e) {
        const auto p = toCanvas(e.position);
        const float fx = clampv(p.x / (float) W, 0.0f, 1.0f), fy = clampv(p.y / (float) H, 0.0f, 1.0f);
        env.set(P_bend, rnd(fx * 127 - 64)); env.set(P_mod, rnd((1 - fy) * 127)); repaint();
    }
};

// ---------------------------------------------------------------- cv_rib
class Ribbon : public CanvasDisplay {
public:
    explicit Ribbon(DisplayEnv& e) : CanvasDisplay(e, 250, 64) {}
    int target() const { return env.ribbonTarget ? env.ribbonTarget() : -1; }
    void draw(Ctx& x, float w, float h) override {
        const float n = v(P_rib) / 127.0f;
        x.fillStyle(x.pal.fill); x.fillRect(0, h / 2 - 5, w * n, 10);
        x.strokeStyle(x.pal.grid); x.lineWidth = 1; x.strokeRect(0.5f, h / 2 - 5.5f, w - 1, 11);
        if (n > 0 || v(P_rhold)) { x.fillStyle(x.pal.acc); x.fillRect(clampv(w * n - 2, 0.0f, w - 4), h / 2 - 9, 4, 18); }
        const int t = target();
        juce::String lab = "no target: RIBBON ASSIGN";
        if (t >= 0 && t < kNumParams) {
            juce::StringArray words; words.addTokens(juce::String(kParams[t].label), " \t\r\n", ""); words.removeEmptyStrings();
            lab = "moves: " + words.joinIntoString(" ").toLowerCase();
        }
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText(lab, 3, 10);
        x.textAlign(1); x.fillText(v(P_rrel) ? "relative" : "absolute", w - 3, h - 3);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        on = true; tgt = target(); base = tgt >= 0 ? v(tgt) : 0;
        if (env.beginGesture) { env.beginGesture(P_rib); if (tgt >= 0) env.beginGesture(tgt); }
        setFrom(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override { if (on) setFrom(e); }
    void mouseUp(const juce::MouseEvent&) override {
        if (!on) return; on = false;
        if (!v(P_rhold)) { env.set(P_rib, 0); if (tgt >= 0) env.set(tgt, base); }
        if (env.endGesture) { env.endGesture(P_rib); if (tgt >= 0) env.endGesture(tgt); }
        repaint();
    }
private:
    bool on = false; int tgt = -1, base = 0;
    void setFrom(const juce::MouseEvent& e) {
        const float fx = clampv(toCanvas(e.position).x / (float) W, 0.0f, 1.0f);
        env.set(P_rib, rnd(fx * 127));
        if (tgt >= 0) {   // ribApply
            const auto& d = kParams[tgt]; const float n = v(P_rib) / 127.0f;
            const float val = v(P_rrel) ? base + (n - 0.5f) * (float) (d.max - d.min) : d.min + n * (float) (d.max - d.min);
            env.set(tgt, juce::jlimit(d.min, d.max, rnd(val)));
        }
        repaint();
    }
};

// ---------------------------------------------------------------- cv_gate
class Gate : public CanvasDisplay {
public:
    explicit Gate(DisplayEnv& e) : CanvasDisplay(e, 1100, 240) {}
    void draw(Ctx& x, float w, float h) override {
        const float sw = w / 16; const int pat = v(P_gpat);
        const int ph = (env.live && v(P_gon)) ? env.live->gatePhase.load() : -1;
        const float fl = floorLevel(); const float dc = v(P_gdc) / 127.0f, swg = v(P_gsw) / 127.0f;
        auto gLevel = [&](int on, float u, int nextOn) -> float {
            if (on) { if (dc < 0.01f) return 1.0f; const float tau = 1.2f * (1 - dc) + 0.05f; return fl + (1 - fl) * std::exp(-u / tau); }
            if (swg > 0.01f && nextOn) { const float a = 1 - swg; if (u > a) return fl + (1 - fl) * ((u - a) / swg); }
            return fl;
        };
        for (int i = 0; i < 16; ++i) {
            const int on = (pat >> i) & 1, nx = (pat >> ((i + 1) % 16)) & 1; const int N = 20;
            x.beginPath(); x.moveTo(i * sw + 1.5f, h - 3);
            for (int k = 0; k <= N; ++k) { const float u = (float) k / N, lv = gLevel(on, u, nx); x.lineTo(i * sw + 1.5f + u * (sw - 3), h - 3 - std::max(3.0f, lv * (h - 6))); }
            x.lineTo(i * sw + sw - 1.5f, h - 3); x.closePath();
            x.fillStyle(on ? x.pal.line : x.pal.grid); x.globalAlpha = on ? 0.85f : 1.0f; x.fill(); x.globalAlpha = 1;
            if (v(P_gon) == 0) { x.fillStyle(juce::Colour::fromFloatRGBA(0, 0, 0, 0.35f)); x.fillRect(i * sw + 1.5f, 0, sw - 3, h); }
            if (ph == i) { x.strokeStyle(x.pal.acc); x.lineWidth = 1.5f; x.strokeRect(i * sw + 1, 1, sw - 2, h - 2); }
        }
        if (h > 100) {
            x.fillStyle(x.pal.txt); x.mono(17); x.textAlign(0);
            static const char* lab[4] = { "1", "e", "&", "a" };
            for (int i = 0; i < 16; ++i) x.fillText(lab[i % 4], i * sw + sw / 2, 12);
        }
        x.strokeStyle(x.pal.txt); x.globalAlpha = 0.4f; x.lineWidth = 1;
        for (int k : { 4, 8, 12 }) x.line(k * sw, 0, k * sw, h);
        x.globalAlpha = 1;
    }
    void mouseDown(const juce::MouseEvent& e) override {
        on = true; if (env.beginGesture) { env.beginGesture(P_gpat); env.beginGesture(P_gpt); }
        const int i = stepAt(e); val = ((v(P_gpat) >> i) & 1) ? 0 : 1; last = i; apply(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override { if (on) apply(e); }
    void mouseUp(const juce::MouseEvent&) override { if (!on) return; on = false; if (env.endGesture) { env.endGesture(P_gpat); env.endGesture(P_gpt); } }
private:
    bool on = false; int val = 1, last = 0;
    float floorLevel() const { const int d = v(P_gdep); return d >= 127 ? 0.0f : std::pow(10.0f, -d / 127.0f * 60 / 20); }
    int stepAt(const juce::MouseEvent& e) const { return clampv((int) std::floor(toCanvas(e.position).x / (float) W * 16), 0, 15); }
    void apply(const juce::MouseEvent& e) {
        // fill every step between the last and the current one, so a fast drag leaves no gaps (the mockup only sees the sampled steps)
        const int now = stepAt(e); int bits = 0;
        for (int k = std::min(last, now); k <= std::max(last, now); ++k) bits |= 1 << k;
        last = now;
        const int cur = v(P_gpat); const int nv = val ? (cur | bits) : (cur & ~bits);
        if (nv != cur) { env.set(P_gpat, nv); env.set(P_gpt, 0); repaint(); }
    }
};

// ---------------------------------------------------------------- cv_m (output meter)
class Meter : public CanvasDisplay {
public:
    explicit Meter(DisplayEnv& e) : CanvasDisplay(e, 92, 200) {}
    void update() override {
        float lvL = -90, lvR = -90;
        if (const auto* L = env.live) {
            float a[1024], b[1024]; L->outL.snapshot(a); L->outR.snapshot(b);
            float pl = L->outPeakL.load(), pr = L->outPeakR.load();
            for (int i = 0; i < 1024; ++i) { if (std::isfinite(a[i])) pl = std::max(pl, std::abs(a[i])); if (std::isfinite(b[i])) pr = std::max(pr, std::abs(b[i])); }
            lvL = pl > 0 ? 20 * std::log10(pl) : -90.0f; lvR = pr > 0 ? 20 * std::log10(pr) : -90.0f;
        }
        const float d = dt();   // the mockup falls 1.1 dB per 60 Hz frame and holds peaks 0.15 dB per frame
        auto fall = [&](float c, float n) { return n > c ? n : c - 1.1f * 60 * d; };
        l = fall(l, lvL); r = fall(r, lvR);
        pl = std::max(pl - 0.15f * 60 * d, l); pr = std::max(pr - 0.15f * 60 * d, r);
        if (std::max(lvL, lvR) >= 0) clipUntil = env.nowSeconds + 2.0;
    }
    void draw(Ctx& x, float w, float h) override {
        auto& g = x.g;
        const float top = 4, bot = h - 4;
        auto yDb = [&](float db) { return top + (3 - db) / 63 * (bot - top); };
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(1);
        for (int db : { 0, -12, -24, -36, -48, -60 }) {
            const float y = yDb((float) db); x.fillText(juce::String(db), 26, y + 3);
            x.fillStyle(x.pal.grid); x.fillRect(28, y, 4, 1); x.fillStyle(x.pal.txt);
        }
        const float span = yDb(-60) - yDb(3); auto at = [&](float db) { return (yDb(db) - yDb(3)) / span; };
        juce::ColourGradient grad(hex(0xff1383), 0, yDb(3), hex(0x4aa9e0), 0, yDb(-60), false);
        grad.addColour(at(-1), hex(0xff3a9a)); grad.addColour(at(-8), hex(0xa43dff)); grad.addColour(at(-22), hex(0x6f7bf8)); grad.addColour(at(-34), hex(0x6cc9f5));
        auto col = [](float db) { return db >= -1 ? hex(0xff3a9a) : db > -12 ? hex(0xb46bff) : hex(0x6cc9f5); };
        const float lv[2] = { l, r }, pk[2] = { pl, pr }, bxs[2] = { 36, 62 };
        for (int k = 0; k < 2; ++k) {
            const float bx = bxs[k];
            juce::Path rr; rr.addRoundedRectangle(bx, top, 18.0f, bot - top, 3.0f);
            juce::ColourGradient bg(juce::Colours::white.withAlpha(0.04f), bx, 0, juce::Colours::white.withAlpha(0.04f), bx + 18, 0, false); bg.addColour(0.5, juce::Colours::white.withAlpha(0.09f));
            g.setGradientFill(bg); g.fillPath(rr);
            const float y = yDb(clampv(lv[k], -60.0f, 3.0f));
            {
                juce::Graphics::ScopedSaveState ss(g);
                g.reduceClipRegion(rr, juce::AffineTransform()); g.reduceClipRegion(juce::Rectangle<int>((int) std::floor(bx), (int) std::floor(y), 18, (int) std::ceil(bot - y) + 1));
                g.setGradientFill(grad); g.fillRect(bx, top, 18.0f, bot - top);
                juce::ColourGradient sh(juce::Colours::white.withAlpha(0.28f), bx, 0, juce::Colours::black.withAlpha(0.25f), bx + 18, 0, false); sh.addColour(0.35, juce::Colours::white.withAlpha(0.0f));
                g.setGradientFill(sh); g.fillRect(bx, top, 18.0f, bot - top);
            }
            const float yp = yDb(clampv(pk[k], -60.0f, 3.0f)); const auto c = col(pk[k]);
            for (int s = 3; s >= 1; --s) { g.setColour(c.withAlpha(0.10f)); g.fillRoundedRectangle(bx - s, yp - 1 - s, 18.0f + 2 * s, 2.5f + 2 * s, 1.0f + s); }
            g.setColour(c); g.fillRoundedRectangle(bx, yp - 1, 18.0f, 2.5f, 1.0f);
        }
        const bool clip = env.nowSeconds < clipUntil;
        if (clip) for (int s = 4; s >= 1; --s) { g.setColour(hex(0xff1383).withAlpha(0.10f)); g.fillRoundedRectangle(36.0f - s, -s, 44.0f + 2 * s, 3.0f + 2 * s, 1.5f + s); }
        g.setColour(clip ? hex(0xff1383) : x.pal.grid); g.fillRoundedRectangle(36.0f, 0.0f, 44.0f, 3.0f, 1.5f);
    }
private:
    float l = -90, r = -90, pl = -90, pr = -90; double clipUntil = -1;
};

} // namespace

std::unique_ptr<Display> makeControlDisplay(const juce::String& id, DisplayEnv& e) {
    if (id == "cv_pad") return std::make_unique<Pad>(e);
    if (id == "cv_rib") return std::make_unique<Ribbon>(e);
    if (id == "cv_gate") return std::make_unique<Gate>(e);
    if (id == "cv_m") return std::make_unique<Meter>(e);
    return nullptr;
}

} // namespace jp::disp
