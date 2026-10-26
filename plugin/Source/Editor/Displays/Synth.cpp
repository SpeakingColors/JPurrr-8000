// Synth section displays: cv_l1 (LFO 1 shape), cv_o1, cv_o2 (oscillators), cv_pe (pitch envelope),
// cv_f (filter response), cv_fe, cv_ae (envelopes). Ported from the mockup's D3 object and LIVE overlays.
#include "Common.h"

namespace jp::disp {
using namespace jpurrr;

namespace {
const float K7[7] = { -0.11002313f, -0.06288439f, -0.01952356f, 0.0f, 0.01991221f, 0.06216538f, 0.10745242f };
const char* const O1W[7] = { "SUPER SAW", "TRIANGLE MOD", "NOISE", "FEEDBACK OSC", "SQUARE (PWM)", "SAW", "TRIANGLE" };
const char* const O1L[7][2] = { { "DETUNE", "MIX" }, { "OFFSET", "LFO1 DEPTH" }, { "CUTOFF FREQ", "RESONANCE" }, { "HARMONICS", "FEEDBACK AMT" },
                                { "PULSE WIDTH", "PWM DEPTH" }, { "SHAPE", "LFO1 DEPTH" }, { "SHAPE", "LFO1 DEPTH" } };
const char* const O2L[4][2] = { { "PULSE WIDTH", "PWM DEPTH" }, { "SHAPE", "LFO1 DEPTH" }, { "SHAPE", "LFO1 DEPTH" }, { "CUTOFF FREQ", "RESONANCE" } };

template <class T> T sawW(T p) { return 1 - 2 * fr_(p); }
template <class T> T triW(T p) { p = fr_(p); return p < T(0.5) ? p * 4 - 1 : 3 - p * 4; }
const std::array<float, 64>& nzTab() {
    static std::array<float, 64> a = [] { std::array<float, 64> t{}; for (int i = 0; i < 64; ++i) t[(size_t) i] = (float) (std::sin(i * 91.7) * std::cos(i * 13.3)); return t; }();
    return a;
}
float noiseAt(double ph, double t) { return nzTab()[(size_t) std::floor(fr_(ph * 1.7 + t * 3) * 63)]; }
int rnd(float x) { return (int) std::floor(x + 0.5f); }

// Last note, like the mockup's LV.last, rebuilt from the note counters of env.live.
struct LastNote {
    bool have = false, off = false; double t0 = 0, tr = 0;
    double rel = 1, aa = 1, ad = 1, fa = 1, fd = 1, pa = 1, pdc = 1;
    uint32_t onC = 0, offC = 0; bool seen = false;
    void update(const DisplayEnv& env) {
        const auto* L = env.live; if (!L) return;
        const uint32_t on = L->noteOnCount.load(), offc = L->noteOffAllCount.load();
        if (!seen) { seen = true; onC = on; offC = offc; return; }
        if (on != onC) {
            onC = on; have = true; off = false; t0 = env.nowSeconds;
            auto g = [&](const char* id) { return (float) env.get(pid(id)); };
            rel = tm(g("aer"), 0.01f, 5); aa = tm(g("aea"), 0.002f, 3); ad = tm(g("aed"), 0.01f, 4);
            fa = tm(g("fea"), 0.002f, 3); fd = tm(g("fed"), 0.01f, 4); pa = tm(g("pa"), 0.001f, 2); pdc = tm(g("pdd"), 0.01f, 3);
        }
        if (offc != offC) { offC = offc; if (have && !off) { off = true; tr = env.nowSeconds; } }
    }
};

// ---------------------------------------------------------------- cv_l1
class L1 : public CanvasDisplay {
public:
    explicit L1(DisplayEnv& e) : CanvasDisplay(e, 130, 44) {}
    void draw(Ctx& x, float w, float h) override {
        x.strokeStyle(x.pal.line); x.lineWidth = 1.6f; x.beginPath();
        const int t = v(P_l1w); const int N = 100; float ly = 0;
        for (int i = 0; i <= N; ++i) {
            const float ph = std::fmod((float) i / N * 2, 1.0f); float y;
            if (t == 0) y = ph < 0.5f ? ph * 4 - 1 : 3 - ph * 4;
            else if (t == 1) y = 1 - ph * 2;
            else if (t == 2) y = ph < 0.5f ? 1.0f : -1.0f;
            else y = (float) std::fmod(std::sin(std::floor(i / 12.5) * 127.1) * 43758.5453, 1.0);
            const float py = h / 2 - y * (h / 2 - 5);
            if (i == 0) x.moveTo(0, py);
            else { if (t == 2 || t == 3) x.lineTo(i * w / N, ly); x.lineTo(i * w / N, py); }
            ly = py;
        }
        x.stroke();
    }
};

// ---------------------------------------------------------------- oscillator base (scroll clock, parameter view)
class OscBase : public CanvasDisplay {
public:
    using CanvasDisplay::CanvasDisplay;
protected:
    double viewUntil = -1; double scr = 0;
    bool paramView() const { return env.nowSeconds < viewUntil; }
    void bump() { viewUntil = env.nowSeconds + 1.7; repaint(); }
    void update() override {
        if (env.animPhase >= 0) { scr = env.animPhase; return; }
        const bool voices = env.live && env.live->activeVoices.load() > 0;
        scr = std::fmod(scr + dt() * (voices ? 2.2 : 0.9), 1000.0);
    }
    double clockT() const { return env.nowSeconds / 1.0 * 0.35; }
    template <class F> void waveView(Ctx& x, float w, float h, F fn) const {
        const double t = clockT(); const float mid = (h + 14) / 2, amp = (h - 14) / 2 - 5;
        x.strokeStyle(x.pal.grid); x.lineWidth = 1; x.line(0, mid + 0.5f, w, mid + 0.5f);
        x.strokeStyle(x.pal.line); x.lineWidth = 1.6f; x.beginPath();
        const int N = (int) std::floor(w);
        for (int i = 0; i <= N; ++i) {
            const double ph = (double) i / w * 2.5 + scr; const float y = (float) fn(ph, i, t);
            const float py = mid - clampv(y, -1.2f, 1.2f) * amp;
            if (i == 0) x.moveTo((float) i, py); else x.lineTo((float) i, py);
        }
        x.stroke();
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(1); x.fillText("live", w - 3, 10);
    }
    // pulse >= 0: pulse width view, else the one period outline
    template <class F> void shapeView(Ctx& x, float w, float h, F fn, float pulse, float pwm) const {
        const float mid = (h - 16) / 2 + 4, amp = (h - 26) / 2;
        x.strokeStyle(x.pal.line); x.fillStyle(x.pal.fill); x.lineWidth = 1.6f;
        if (pulse >= 0) {
            const float d = 0.1f + pulse * 0.8f; const float lo = clampv(d - pwm * 0.3f, 0.05f, 0.95f), hi = clampv(d + pwm * 0.3f, 0.05f, 0.95f);
            auto px = [&](float a) { return 4 + a * (w - 8) / 2; };
            x.globalAlpha = 0.35f; x.fillStyle(x.pal.acc); x.fillRect(px(lo), mid - amp, px(hi) - px(lo), amp * 2); x.globalAlpha = 1;
            x.beginPath(); x.moveTo(4, mid + amp); x.lineTo(4, mid - amp); x.lineTo(px(d), mid - amp); x.lineTo(px(d), mid + amp); x.lineTo(px(2), mid + amp); x.lineTo(px(2), mid - amp); x.stroke();
            x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1);
            x.fillText("width " + juce::String(rnd(d * 100)) + "%" + (pwm > 0 ? juce::String::fromUTF8(" \xC2\xB1 ") + juce::String(rnd(pwm * 30)) + "%" : juce::String()), 3, h - 3);
            return;
        }
        x.beginPath(); const int N = 120;
        for (int i = 0; i <= N; ++i) { const float ph = (float) i / N * 2; const float py = mid - fn(ph) * amp; if (i == 0) x.moveTo((float) i * w / N, py); else x.lineTo((float) i * w / N, py); }
        x.stroke();
    }
};

// ---------------------------------------------------------------- cv_o1
class O1 : public OscBase {
public:
    explicit O1(DisplayEnv& e) : OscBase(e, 132, 140) {}
    void touched(int idx) override { if (idx == P_o1c1 || idx == P_o1c2) bump(); }
    juce::String infoText(int) const override {
        const int wv = juce::jlimit(0, 6, v(P_o1w));
        if (!paramView()) return O1W[wv];
        const double cr = curveD((double) v(P_o1c1) / 127.0);
        if (wv == 0) {
            const int lo = rnd(1200 * std::log2(1 + K7[0] * cr)), hi = rnd(1200 * std::log2(1 + K7[6] * cr));
            return juce::String::fromUTF8("7 saws \xC2\xB7 ") + juce::String(lo) + juce::String::fromUTF8(" \xE2\x80\xA6 +") + juce::String(hi) + " ct";
        }
        return juce::String(O1L[wv][0]) + " " + juce::String(v(P_o1c1)) + juce::String::fromUTF8(" \xC2\xB7 ") + O1L[wv][1] + " " + juce::String(v(P_o1c2));
    }
    void draw(Ctx& x, float w, float h) override {
        const int wv = juce::jlimit(0, 6, v(P_o1w)); const bool param = paramView();
        const float c1 = (float) v(P_o1c1), c2 = (float) v(P_o1c2);
        const double crd = curveD((double) v(P_o1c1) / 127.0); const float cr = (float) crd, m = c2 / 127, n = c1 / 127;
        const float gc = -0.55366f * m + 0.99785f, gs = -0.73764f * m * m + 1.2841f * m + 0.044372f;
        auto wave = [&](double ph, double t) -> double {
            if (wv == 0) {
                double sum = 0, tot = 0;   // double: the saw edges sit on pixel borders, float shifts them visibly
                for (int i = 0; i < 7; ++i) { const double g = i == 3 ? gc : gs; const double k = 1 + (double) K7[i] * crd * 3; const double p = (double) ph * k + (double) t * k; sum += g * (1 - 2 * (p - std::floor(p))); tot += g; }
                return sum / std::max(0.5, tot * 0.62);
            }
            if (wv == 1) return triW(ph) * (1 - n * 0.5) + std::sin(ph * 6.283185307179586 * 3) * n * 0.4;
            if (wv == 2) return noiseAt(ph, t);
            if (wv == 3) return std::sin(ph * 6.283185307179586 + std::sin(ph * 12.566370614359172) * m * 3) * (0.4 + n * 0.6);
            if (wv == 4) { const double d = 0.1 + n * 0.8 + std::sin(t * 6) * m * 0.3; return fr_(ph) < clampv(d, 0.05, 0.95) ? 1.0 : -1.0; }
            if (wv == 5) return sawW(ph);
            return triW(ph);
        };
        if (!param) { waveView(x, w, h, [&](double ph, int, double t) { return wave(ph, t); }); return; }
        x.strokeStyle(x.pal.grid); x.lineWidth = 1; x.line(0, h - 14.5f, w, h - 14.5f);
        if (wv == 0) {
            const float R = 215;
            for (int i = 0; i < 7; ++i) {
                const float ct = 1200 * std::log2(1 + K7[i] * cr); const float px = w / 2 + ct / R * (w / 2 - 8);
                const float g = i == 3 ? gc : gs; const float bh = std::max(1.0f, g * (h - 30));
                x.fillStyle(i == 3 ? x.pal.acc : x.pal.line); x.fillRect(px - 1, h - 14 - bh, 2, bh);
            }
            x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(0);
            x.fillText("-250", 14, h - 3); x.fillText("0", w / 2, h - 3); x.fillText("+250 ct", w - 20, h - 3);
        } else {
            shapeView(x, w, h, [&](double ph) { return wave(ph, 0.0); }, wv == 4 ? n : -1.0f, wv == 4 ? m : 0.0f);
        }
    }
};

// ---------------------------------------------------------------- cv_o2
class O2 : public OscBase {
public:
    explicit O2(DisplayEnv& e) : OscBase(e, 104, 140) {}
    void touched(int idx) override { if (idx == P_o2c1 || idx == P_o2c2) bump(); }
    void draw(Ctx& x, float w, float h) override {
        const int wv = juce::jlimit(0, 3, v(P_o2w)); const bool param = paramView();
        const float c1 = v(P_o2c1) / 127.0f, c2 = v(P_o2c2) / 127.0f;
        auto wave = [&](double ph, double t) -> double {
            if (wv == 0) { const double d = 0.1 + c1 * 0.8 + std::sin(t * 6) * c2 * 0.3; return fr_(ph) < clampv(d, 0.05, 0.95) ? 1.0 : -1.0; }
            if (wv == 1) return sawW(ph);
            if (wv == 2) return triW(ph);
            return noiseAt(ph, t);
        };
        if (!param) { waveView(x, w, h, [&](double ph, int, double t) { return wave(ph, t); }); return; }
        shapeView(x, w, h, [&](double ph) { return wave(ph, 0.0); }, wv == 0 ? c1 : -1.0f, wv == 0 ? c2 : 0.0f);
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1);
        x.fillText(juce::String(O2L[wv][0]) + " " + juce::String(v(P_o2c1)) + juce::String::fromUTF8(" \xC2\xB7 ") + O2L[wv][1] + " " + juce::String(v(P_o2c2)), 3, h - 3);
    }
};

// ---------------------------------------------------------------- cv_pe
class PE : public CanvasDisplay {
public:
    explicit PE(DisplayEnv& e) : CanvasDisplay(e, 112, 78) {}
    void update() override { note.update(env); }
    void draw(Ctx& x, float w, float h) override {
        const float a = v(P_pa) / 127.0f, d = v(P_pdd) / 127.0f, dep = v(P_pd) / 64.0f;
        x.beginPath();
        const float aw = 6 + a * w * 0.3f, dw = 10 + d * w * 0.45f; x.moveTo(4, h / 2);
        const float top = h / 2 - dep * (h / 2 - 8);
        x.lineTo(4 + aw, top);
        for (int i = 1; i <= 30; ++i) { const float t = i / 30.0f; x.lineTo(4 + aw + t * dw, h / 2 + (top - h / 2) * std::exp(-4 * t)); }
        x.lineTo(w - 4, h / 2);
        x.strokeStyle(x.pal.line); x.lineWidth = 1.8f; x.stroke();
        x.strokeStyle(x.pal.grid); x.lineWidth = 1; x.line(0, h / 2 + 0.5f, w, h / 2 + 0.5f);
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText("pitch", 4, 10);
        // live dot (mockup LIVE.cv_pe). The mockup used fixed 40 / 60 px widths here; the curve uses w * .3 / w * .45, which is used now.
        if (!note.have || !v(P_pd)) return;
        const double now = env.nowSeconds, t = now - note.t0;
        if (note.off && now > note.tr + note.rel * 1.1) return;
        float px, py;
        if (t < note.pa) { const float k = (float) (t / note.pa); px = 4 + aw * k; py = h / 2 + (top - h / 2) * k; }
        else { const float k = (float) std::min(1.0, (t - note.pa) / note.pdc); px = 4 + aw + dw * k; py = h / 2 + (top - h / 2) * std::exp(-4 * k); }
        drawDot(x, px, py, 5);
    }
private:
    LastNote note;
};

// ---------------------------------------------------------------- cv_f
class Filt : public CanvasDisplay {
public:
    explicit Filt(DisplayEnv& e) : CanvasDisplay(e, 200, 168) {}
    void update() override { lv.update(env, LiveCtx::bit(kTapIn), 0); }
    void draw(Ctx& x, float w, float h) override {
        const int type = v(P_ft), steep = v(P_f24);
        const float fc = 20 * std::pow(1000.0f, v(P_fc) / 127.0f);
        const float fr = (float) v(P_fr);
        const float Q0 = 0.6f + fr / 127.0f * (fr > 110 ? 14.0f : 7.0f);
        x.strokeStyle(x.pal.grid); x.lineWidth = 1;
        for (float f : { 100.0f, 1000.0f, 10000.0f }) { const float px = std::log(f / 20) / std::log(1000.0f) * w; x.line(px + 0.5f, 0, px + 0.5f, h); }
        for (float db : { -12.0f, -24.0f }) { const float py = h * 0.35f - db / 24 * h * 0.55f; x.line(0, py + 0.5f, w, py + 0.5f); }
        auto H1 = [&](float f, float Q) { const float r = f / fc; const float den = std::hypot(1 - r * r, r / Q); return type == 2 ? 1 / den : type == 0 ? r * r / den : (r / Q) / den; };
        x.beginPath();
        for (int i = 0; i <= (int) w; ++i) {
            const float f = 20 * std::pow(1000.0f, i / w); float m = H1(f, Q0);
            if (steep) m *= H1(f, 0.707f + (Q0 - 0.707f) * 0.5f);
            const float db = 20 * std::log10(std::max(m, 1e-4f));
            const float py = clampv(h * 0.35f - db / 24 * h * 0.55f, 2.0f, h - 2);
            if (i == 0) x.moveTo((float) i, py); else x.lineTo((float) i, py);
        }
        x.strokeStyle(x.pal.line); x.lineWidth = 1.8f; x.stroke();
        x.lineTo(w, h); x.lineTo(0, h); x.fillStyle(x.pal.fill); x.fill();
        const float cxp = std::log(fc / 20) / std::log(1000.0f) * w;
        x.strokeStyle(x.pal.acc); x.setLineDash({ 3, 3 }); x.line(cxp, 0, cxp, h); x.clearDash();
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1);
        x.fillText(fc < 1000 ? juce::String(rnd(fc)) + " Hz" : toFixed(fc / 1000, 1) + " k", 3, 10);
        x.textAlign(1); x.fillText("approx. curve", w - 3, h - 3);
        drawSpec(x, w, lv, kTapIn, (float) env.sampleRate, 20, 20000, 4, h - 2);
    }
private:
    LiveCtx lv;
};

// ---------------------------------------------------------------- cv_fe, cv_ae
class EnvD : public CanvasDisplay {
public:
    EnvD(DisplayEnv& e, int w, int h, bool filter) : CanvasDisplay(e, w, h), isFilter(filter) {
        ids[0] = pid(filter ? "fea" : "aea"); ids[1] = pid(filter ? "fed" : "aed"); ids[2] = pid(filter ? "fes" : "aes"); ids[3] = pid(filter ? "fer" : "aer");
    }
    void update() override { note.update(env); }
    void draw(Ctx& x, float w, float h) override {
        const float A = v(ids[0]) / 127.0f, D = v(ids[1]) / 127.0f, S = v(ids[2]) / 127.0f, R = v(ids[3]) / 127.0f;
        const float aw = 6 + A * w * 0.22f, dw = 6 + D * w * 0.26f, sw = w * 0.12f, rw = 6 + R * w * 0.26f;
        const float base = h - 5, top = 7, sy = base - S * (base - top);
        float px = 4; x.beginPath(); x.moveTo(px, base);
        px += aw; x.lineTo(px, top); px += dw; x.lineTo(px, sy); px += sw; x.lineTo(px, sy); px += rw; x.lineTo(std::min(px, w - 3), base);
        x.lineTo(4, base); x.fillStyle(x.pal.fill); x.fill(); x.strokeStyle(x.pal.line); x.lineWidth = 1.7f; x.stroke();
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText("A", 6 + aw / 2 - 2, h - 8 > top + 8 ? top + 9 : h - 8);
        // live dot (mockup lvEnv)
        if (!note.have) return;
        const double now = env.nowSeconds;
        const double ta = isFilter ? note.fa : note.aa, td = isFilter ? note.fd : note.ad;
        const double rl = isFilter ? tm((float) v(ids[3]), 0.01f, 5) : note.rel;
        const float x0 = 4, x1 = x0 + aw, x2 = x1 + dw, x3 = x2 + sw, x4 = std::min(x3 + rw, w - 3);
        auto held = [&](double t) -> juce::Point<float> {
            if (t < ta) return { (float) (x0 + aw * t / ta), (float) (base - (base - top) * t / ta) };
            if (t < ta + td) return { (float) (x1 + dw * (t - ta) / td), (float) (top + (sy - top) * (t - ta) / td) };
            return { (float) (x2 + sw * std::min(1.0, (t - ta - td) / 3)), sy };
        };
        juce::Point<float> p;
        if (note.off) {
            const double f = (now - note.tr) / std::max(0.05, rl); if (f > 1.1) return;
            const auto s0 = held(note.tr - note.t0); const float k = (float) std::min(1.0, f);
            p = { s0.x + (x4 - s0.x) * k, s0.y + (base - s0.y) * k };
        } else p = held(now - note.t0);
        drawDot(x, p.x, p.y, 5);
    }
private:
    bool isFilter; int ids[4]; LastNote note;
};

} // namespace

std::unique_ptr<Display> makeOscDisplay(const juce::String& id, DisplayEnv& e) {
    if (id == "cv_l1") return std::make_unique<L1>(e);
    if (id == "cv_o1") return std::make_unique<O1>(e);
    if (id == "cv_o2") return std::make_unique<O2>(e);
    if (id == "cv_pe") return std::make_unique<PE>(e);
    if (id == "cv_f") return std::make_unique<Filt>(e);
    if (id == "cv_fe") return std::make_unique<EnvD>(e, 150, 138, true);
    if (id == "cv_ae") return std::make_unique<EnvD>(e, 220, 84, false);
    return nullptr;
}

} // namespace jp::disp
