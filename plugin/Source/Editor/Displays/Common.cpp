#include "Common.h"
#include <unordered_map>

namespace jp::disp {

// ---------------------------------------------------------------- Ctx
void Ctx::arc(float cx, float cy, float r, float a0, float a1) {
    const int n = juce::jmax(8, (int) std::ceil(std::abs(a1 - a0) / juce::MathConstants<float>::twoPi * juce::jmax(24.0f, r * 3.0f)));
    for (int i = 0; i <= n; ++i) {
        const float a = a0 + (a1 - a0) * (float) i / (float) n;
        lineTo(cx + std::cos(a) * r, cy + std::sin(a) * r);
    }
}

void Ctx::fillText(const juce::String& s, float x, float y) {
    if (s.isEmpty()) return;
    if (y < fontPx * 0.8f + 1.0f) y = fontPx * 0.8f + 1.0f;   // mockup fixCtx
    juce::GlyphArrangement ga;
    ga.addLineOfText(font, s, 0.0f, 0.0f);
    const float tw = ga.getBoundingBox(0, -1, true).getWidth();
    const float ox = align < 0 ? x : (align == 0 ? x - tw * 0.5f : x - tw);
    juce::GlyphArrangement gb;
    gb.addLineOfText(font, s, ox, y);
    g.setColour(fillCol.withMultipliedAlpha(globalAlpha));
    gb.draw(g);
}

// ---------------------------------------------------------------- helper maths
double curveD(double x) {
    static const double co[12] = { 10028.7312891634, -50818.8652045924, 111363.4808729368, -138150.6761080548, 106649.6679158292, -53046.9642751875,
                                   17019.9518580080, -3425.0836591318, 404.2703938388, -24.1878824391, 0.6717417634, 0.0030115596 };
    double r = 0;
    for (double k : co) r = r * x + k;
    return r;
}

std::vector<float> shapeCurve(int dv, int dx, int df) {
    const int n = 2048;
    std::vector<float> c((size_t) n);
    const float wd = std::min(1.0f, dv / 127.0f * 1.5f), wt = dx / 127.0f, wf = df / 127.0f;
    const float k1 = 1 + dv / 127.0f * 8, k2 = 1 + dx / 127.0f * 6, k3 = 2 + df / 127.0f * 14;
    auto mixf = [](float a, float b, float w) { return a + (b - a) * w; };
    for (int i = 0; i < n; ++i) {
        const float x = (float) i / (n - 1) * 2 - 1;
        const float y1 = mixf(x, std::tanh(k1 * x) / std::tanh(k1), wd);
        const float y2 = mixf(y1, std::tanh(k2 * (y1 + 0.12f * y1 * y1)) / std::tanh(k2), wt);
        c[(size_t) i] = mixf(y2, std::tanh(k3 * y2) / std::tanh(k3), wf);
    }
    return c;
}

std::array<float, 3> mbFr(const std::function<int(int)>& get) {
    const float f1 = hzv((float) get(pid("qmx1")), 40, 400);
    const float f2 = std::max(hzv((float) get(pid("qmx2")), 300, 3000), f1 * 1.5f);
    const float f3 = std::max(hzv((float) get(pid("qmx3")), 4000, 16000), f2 * 1.5f);
    return { f1, f2, f3 };
}

juce::String toFixed(double v, int digits) { return juce::String(v, digits); }
juce::String fmtHz(float f) { return f < 1000 ? juce::String((int) std::floor(f + 0.5f)) + " Hz" : toFixed(f / 1000.0, 1) + " k"; }
juce::String fmtHzK(float f) { return f < 1000 ? juce::String((int) std::floor(f + 0.5f)) + " Hz" : toFixed(f / 1000.0, 1) + " kHz"; }

int pid(const juce::String& id) {
    static std::unordered_map<std::string, int> cache;
    const std::string k = id.toStdString();
    auto it = cache.find(k);
    if (it != cache.end()) return it->second;
    const int i = jpurrr::paramIndexById(k.c_str());
    jassert(i >= 0);
    cache[k] = i;
    return i;
}

// ---------------------------------------------------------------- Analyser
namespace {
struct FftTables {
    float win[1024]; float cs[512], sn[512]; int rev[1024];
    FftTables() {
        const double a = 0.16;
        for (int n = 0; n < 1024; ++n)
            win[n] = (float) ((1 - a) / 2 - 0.5 * std::cos(2 * juce::MathConstants<double>::pi * n / 1024) + a / 2 * std::cos(4 * juce::MathConstants<double>::pi * n / 1024));
        for (int k = 0; k < 512; ++k) { cs[k] = (float) std::cos(2 * juce::MathConstants<double>::pi * k / 1024); sn[k] = (float) -std::sin(2 * juce::MathConstants<double>::pi * k / 1024); }
        for (int i = 0; i < 1024; ++i) { int r = 0; for (int b = 0; b < 10; ++b) if (i & (1 << b)) r |= 1 << (9 - b); rev[i] = r; }
    }
};
const FftTables& tables() { static FftTables t; return t; }
}

Analyser::Analyser() { for (int i = 0; i < 512; ++i) { smooth[i] = 0; out[i] = -140; } }

void Analyser::process(const float* td, float tau) {
    const auto& T = tables();
    float re[1024], im[1024];
    for (int i = 0; i < 1024; ++i) { re[T.rev[i]] = std::isfinite(td[i]) ? td[i] * T.win[i] : 0.0f; im[T.rev[i]] = 0; }
    for (int len = 2; len <= 1024; len <<= 1) {
        const int half = len / 2, step = 1024 / len;
        for (int i = 0; i < 1024; i += len)
            for (int j = 0; j < half; ++j) {
                const float wr = T.cs[j * step], wi = T.sn[j * step];
                const float xr = re[i + j + half] * wr - im[i + j + half] * wi, xi = re[i + j + half] * wi + im[i + j + half] * wr;
                re[i + j + half] = re[i + j] - xr; im[i + j + half] = im[i + j] - xi;
                re[i + j] += xr; im[i + j] += xi;
            }
    }
    for (int k = 0; k < 512; ++k) {
        const float mag = std::sqrt(re[k] * re[k] + im[k] * im[k]) / 1024.0f;
        smooth[k] = tau * smooth[k] + (1 - tau) * mag;
        out[k] = smooth[k] > 1e-7f ? 20.0f * std::log10(smooth[k]) : -140.0f;
    }
}

// ---------------------------------------------------------------- LiveCtx
void LiveCtx::update(const DisplayEnv& env, unsigned specMask, unsigned lvlMask, bool wantPre, bool wantOutLR) {
    const auto* L = env.live;
    was = act;
    if (L == nullptr) { act = false; return; }
    built = true;
    const unsigned all = specMask | lvlMask | bit(jpurrr::kTapOut);
    for (int t = 0; t < jpurrr::kNumTaps; ++t) {
        if (!(all & bit(t))) continue;
        auto& b = td[(size_t) t]; b.resize(1024);
        L->tap[t].snapshot(b.data());
        float p = 0;
        for (float& s : b) { if (!std::isfinite(s)) s = 0; p = std::max(p, std::abs(s)); }
        pk[t] = p; lvl[t] = p > 1e-5f ? 20.0f * std::log10(p) : -90.0f;
        if (specMask & bit(t)) an[(size_t) t].process(b.data());
    }
    act = lvl[jpurrr::kTapOut] > -72.0f;
    if (wantPre) {
        preL.resize(1024); preR.resize(1024);
        L->preL.snapshot(preL.data()); L->preR.snapshot(preR.data());
        for (int i = 0; i < 1024; ++i) { if (!std::isfinite(preL[(size_t) i])) preL[(size_t) i] = 0; if (!std::isfinite(preR[(size_t) i])) preR[(size_t) i] = 0; }
    }
    if (wantOutLR) {
        std::vector<float> a(1024), b(1024);
        L->outL.snapshot(a.data()); L->outR.snapshot(b.data());
        oL = oR = 0;
        for (int i = 0; i < 1024; ++i) { if (std::isfinite(a[(size_t) i])) oL = std::max(oL, std::abs(a[(size_t) i])); if (std::isfinite(b[(size_t) i])) oR = std::max(oR, std::abs(b[(size_t) i])); }
    }
}

void LiveCtx::pushHist(bool) {
    if (!built) return;
    hist.push_back(act ? lvl[jpurrr::kTapDrive] : -90.0f);
    if (hist.size() > 160) hist.erase(hist.begin());
}

void drawSpec(Ctx& x, float w, const LiveCtx& lv, int tap, float sr, float fLo, float fHi, float yTop, float yBot) {
    if (!lv.act) return;
    const float* s = lv.an[(size_t) tap].db();
    const int nb = 512;
    x.beginPath(); x.moveTo(0, yBot);
    for (int i = 0; i <= (int) w; i += 2) {
        const float f = fLo * std::pow(fHi / fLo, i / w), f2 = fLo * std::pow(fHi / fLo, std::min(w, (float) i + 2) / w);
        const int b0 = std::max(0, (int) std::floor(f / (sr / 2) * nb));
        const int b1 = std::min(nb - 1, std::max(b0, (int) std::floor(f2 / (sr / 2) * nb)));
        float m = -140;
        for (int b = b0; b <= b1; ++b) m = std::max(m, s[b]);
        x.lineTo((float) i, yBot - clampv((m + 100) / 70, 0.0f, 1.0f) * (yBot - yTop));
    }
    x.lineTo(w, yBot); x.closePath();
    x.globalAlpha = 0.38f; x.fillStyle(x.pal.acc); x.fill(); x.globalAlpha = 1;
}

void drawDot(Ctx& x, float px, float py, float r) {
    x.fillStyle(x.pal.acc); x.fillCircle(px, py, r);
    x.strokeStyle(x.pal.bg); x.lineWidth = 1; x.beginPath(); x.arc(px, py, r, 0, 6.2832f); x.stroke();
}

// ---------------------------------------------------------------- CanvasDisplay
static bool gChrome = true;
void CanvasDisplay::setChrome(bool on) { gChrome = on; }

CanvasDisplay::CanvasDisplay(DisplayEnv& e, int cwid, int chei) : Display(e), W(cwid), H(chei) {
    setSize(cwid + 2, chei + 2);   // the mockup canvas plus its 1 px border
    setOpaque(false);
}

void CanvasDisplay::tick() {
    const double now = env.nowSeconds;
    dtTick = lastTick < 0 ? 0.033 : clampv(now - lastTick, 0.0, 0.1);
    lastTick = now;
    update();
    repaint();
}

void CanvasDisplay::paint(juce::Graphics& g) {
    const auto o = origin();
    const auto box = juce::Rectangle<float>(o.x, o.y, (float) W, (float) H);
    if (gChrome) {
        g.setColour(env.pal.bg);
        g.fillRoundedRectangle(box.expanded(1.0f), 2.0f);
        // inset 0 0 6px rgba(0,0,0,.6)
        const float a[6] = { 0.26f, 0.19f, 0.12f, 0.07f, 0.04f, 0.02f };
        for (int d = 0; d < 6; ++d) {
            g.setColour(juce::Colours::black.withAlpha(a[d]));
            g.drawRect(box.reduced((float) d), 1.0f);
        }
        g.setColour(env.pal.edge);
        g.drawRoundedRectangle(box.expanded(0.5f), 2.0f, 1.0f);
    }
    juce::Graphics::ScopedSaveState ss(g);
    g.setOrigin(o.toInt());
    g.reduceClipRegion(0, 0, W, H);
    Ctx x(g, env);
    draw(x, (float) W, (float) H);
}

} // namespace jp::disp
