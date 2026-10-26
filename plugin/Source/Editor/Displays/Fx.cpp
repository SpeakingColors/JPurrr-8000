// FX page displays: cv_qf, cv_qf2 (signal flow), cv_q (tone curve), cv_qd (drive curve), cv_ql (low end), cv_qm (M/S EQ),
// cv_qt (transient), cv_qr (tamer), cv_qg (glue), cv_qw (width), cv_qp (pan), cv_qo (output stage), cv_qa (opto dial),
// cv_qv (OTT), cv_qdm (multiband). Ported from the mockup's D3 object and LIVE overlays.
#include "Common.h"

namespace jp::disp {
using namespace jpurrr;

namespace {
juce::Colour rgba(int r, int g, int b, float a) { return juce::Colour::fromFloatRGBA(r / 255.0f, g / 255.0f, b / 255.0f, a); }
juce::Colour hex(juce::uint32 c) { return juce::Colour(0xff000000u | c); }
int rnd(float x) { return (int) std::floor(x + 0.5f); }
float bell(float f, float fc, float g, float Q) { return g * std::exp(-std::pow(std::log2(f / fc) / (1.2f / Q), 2.0f)); }
float shelf(float f, float fc, float g) { return g / (1 + std::pow(fc / f, 2.0f)); }
float log10f_(float x) { return std::log10(x); }

struct FxBase : CanvasDisplay {
    FxBase(DisplayEnv& e, int w, int h) : CanvasDisplay(e, w, h) {}
    bool voices() const { return env.live && env.live->activeVoices.load() > 0; }
    LiveCtx lv;
};

// ---------------------------------------------------------------- cv_qf, cv_qf2
class Flow : public FxBase {
public:
    Flow(DisplayEnv& e, int w, int h) : FxBase(e, w, h) {
        static const char* ids[13] = { "qdon", "qlon", "qeon", "qtn", "qrn", "qoon", "qvon", "qmon", "qcn", "qwn", "qpn", "qko", "qlk" };
        for (int i = 0; i < 13; ++i) idx[i] = pid(ids[i]);
    }
    void update() override {
        unsigned m = 0; for (int t : { kTapDrive, kTapEq, kTapOpto, kTapOtt, kTapMb, kTapGlue, kTapOut }) m |= LiveCtx::bit(t);
        lv.update(env, 0, m);
    }
    void draw(Ctx& x, float w, float h) override {
        static const char* names[13] = { "DRV", "LOW", "M/S", "TRN", "TAM", "OPT", "OTT", "MBL", "GLU", "WID", "PAN", "CLP", "LIM" };
        static const int KEY[13] = { kTapDrive, kTapEq, kTapEq, kTapEq, kTapEq, kTapOpto, kTapOtt, kTapMb, kTapGlue, kTapOut, kTapOut, kTapOut, kTapOut };
        const int rows = 2; const int per = (13 + rows - 1) / rows; const float gap = 5, bw = (w - (per - 1) * gap) / per, bh = (h - 12 - (rows - 1) * 8) / rows;
        x.mono(17); x.textAlign(0);
        for (int i = 0; i < 13; ++i) {
            const int r = i / per, k = i % per; const bool on = v(P_qon) && v(idx[i]);
            const float X = k * (bw + gap), Y = 12 + r * (bh + 8);
            if (on) { x.fillStyle(x.pal.fill); x.fillRect(X, Y, bw, bh); }
            x.strokeStyle(on ? x.pal.line : x.pal.grid); x.lineWidth = 1; x.strokeRect(X + 0.5f, Y + 0.5f, bw - 1, bh - 1);
            x.fillStyle(on ? x.pal.line : x.pal.txt); x.fillText(names[i], X + bw / 2, Y + bh / 2 + 4);
        }
        x.fillStyle(x.pal.txt); x.textAlign(-1); x.fillText(juce::String::fromUTF8("synth \xE2\x80\xBA gate \xE2\x80\xBA chain \xE2\x80\xBA out"), 0, 9);
        x.textAlign(1); x.fillText(v(P_qon) ? "ON" : "BYPASSED", w, 9);
        if (!lv.act) return;
        for (int i = 0; i < 13; ++i) {
            const int r = i / per, k = i % per; const float X = k * (bw + gap), Y = 12 + r * (bh + 8);
            if (!(v(P_qon) && v(idx[i]))) continue;
            const float u = clampv((lv.lvl[KEY[i]] + 50) / 50, 0.0f, 1.0f);
            x.fillStyle(x.pal.acc); x.globalAlpha = 0.9f; x.fillRect(X + 3, Y + bh - 6, (bw - 6) * u, 3); x.globalAlpha = 1;
        }
    }
private:
    int idx[13];
};

// ---------------------------------------------------------------- cv_q
class Tone : public FxBase {
public:
    explicit Tone(DisplayEnv& e) : FxBase(e, 410, 240) {}
    void update() override { lv.update(env, LiveCtx::bit(kTapOut), 0); }
    void draw(Ctx& x, float w, float h) override {
        auto X = [&](float f) { return std::log(f / 20) / std::log(1000.0f) * w; };
        auto Y = [&](float db) { return h / 2 - db / 12 * (h / 2 - 12); };
        x.strokeStyle(x.pal.grid); x.lineWidth = 1;
        for (float f : { 100.0f, 1000.0f, 10000.0f }) x.line(X(f) + 0.5f, 0, X(f) + 0.5f, h);
        for (float d : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f }) x.line(0, Y(d) + 0.5f, w, Y(d) + 0.5f);
        const bool on = v(P_qon);
        x.beginPath();
        for (int i = 0; i <= (int) w; ++i) {
            const float f = 20 * std::pow(1000.0f, i / w); float db = 0;
            if (on) {
                if (v(P_qlon) && v(P_qhp) >= 3) db -= 10 * log10f_(1 + std::pow(hzv((float) v(P_qhp), 20, 500) / f, 4.0f));
                if (v(P_qeon)) db += bell(f, 3000, v(P_qmp) / 2.0f, 0.8f) + shelf(f, 10000, v(P_qai) / 2.0f);
                if (v(P_qrn)) { const float d = v(P_qrd) / 127.0f * 6; if (v(P_qrf) != 1) db -= bell(f, 4000, d, 2.5f); if (v(P_qrf) != 0) db -= bell(f, 8000, d, 2.5f); }
            }
            const float py = clampv(Y(db), 2.0f, h - 2);
            if (i == 0) x.moveTo((float) i, py); else x.lineTo((float) i, py);
        }
        x.strokeStyle(x.pal.line); x.lineWidth = 1.8f; x.stroke();
        x.lineTo(w, h); x.lineTo(0, h); x.fillStyle(x.pal.fill); x.fill();
        x.mono(15.5f); x.lineWidth = 1; x.setLineDash({ 3, 3 });
        if (on && v(P_qlon)) {
            const float fm = hzv((float) v(P_qlm), 40, 250); x.strokeStyle(x.pal.acc); x.line(X(fm), 0, X(fm), h);
            x.fillStyle(x.pal.acc); x.textAlign(-1); x.fillText("MONO <" + juce::String(rnd(fm)), X(fm) + 3, h - 4);
        }
        if (on && v(P_qeon)) {
            const float fs = hzv((float) v(P_qsh), 80, 400); x.strokeStyle(x.pal.txt); x.line(X(fs), 0, X(fs), h);
            x.fillStyle(x.pal.txt); x.textAlign(-1); x.fillText("SIDE HPF " + juce::String(rnd(fs)), X(fs) + 3, 12);
        }
        x.clearDash(); x.fillStyle(x.pal.txt); x.textAlign(1); x.fillText(on ? "approx. curve" : "BYPASSED", w - 3, h - 4);
        drawSpec(x, w, lv, kTapOut, (float) env.sampleRate, 20, 20000, 12, h - 2);
    }
};

// ---------------------------------------------------------------- cv_qd
class DriveCurve : public FxBase {
public:
    explicit DriveCurve(DisplayEnv& e) : FxBase(e, 270, 190) {}
    void update() override { lv.update(env, 0, LiveCtx::bit(kTapIn)); }
    void draw(Ctx& x, float w, float h) override {
        const bool on = v(P_qon) && v(P_qdon);
        x.strokeStyle(x.pal.grid); x.lineWidth = 1; x.beginPath(); x.moveTo(0, h / 2 + 0.5f); x.lineTo(w, h / 2 + 0.5f); x.moveTo(w / 2 + 0.5f, 0); x.lineTo(w / 2 + 0.5f, h); x.stroke();
        x.setLineDash({ 2, 3 }); x.line(0, h, w, 0); x.clearDash();
        const auto cv = shapeCurve(v(P_qdv), v(P_qdx), v(P_qdf));
        x.beginPath();
        for (int i = 0; i <= (int) w; ++i) {
            const float xx = i / w * 2 - 1; const float y = cv[(size_t) rnd((xx + 1) / 2 * 2047)]; const float py = h / 2 - y * (h / 2 - 4);
            if (i == 0) x.moveTo((float) i, py); else x.lineTo((float) i, py);
        }
        x.globalAlpha = on ? 1.0f : 0.35f; x.strokeStyle(x.pal.line); x.lineWidth = 1.8f; x.stroke(); x.globalAlpha = 1;
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText(juce::String::fromUTF8("IN \xE2\x86\x92 OUT"), 3, 10);
        if (!lv.act) return;
        x.fillStyle(x.pal.acc); x.globalAlpha = 0.55f;
        const auto& a = lv.td[kTapIn];
        for (int i = 0; i < 1024; i += 5) { const float vv = clampv(a[(size_t) i], -1.0f, 1.0f); const float y = cv[(size_t) rnd((vv + 1) / 2 * 2047)]; x.fillCircle(w / 2 + vv * (w / 2), h / 2 - y * (h / 2 - 4), 2.2f); }
        x.globalAlpha = 1;
        const float p = clampv(lv.pk[kTapIn], 0.0f, 1.0f), yy = cv[(size_t) rnd((p + 1) / 2 * 2047)];
        drawDot(x, w / 2 + p * (w / 2), h / 2 - yy * (h / 2 - 4), 5);
    }
};

// ---------------------------------------------------------------- cv_ql
class LowEnd : public FxBase {
public:
    explicit LowEnd(DisplayEnv& e) : FxBase(e, 270, 190) {}
    void update() override { lv.update(env, LiveCtx::bit(kTapDrive), 0); }
    void draw(Ctx& x, float w, float h) override {
        auto X = [&](float f) { return std::log(f / 20) / std::log(100.0f) * w; };
        auto Y = [&](float db) { return h * 0.3f - db / 30 * h * 0.6f; };
        const bool on = v(P_qon) && v(P_qlon); const float fm = hzv((float) v(P_qlm), 40, 250); const float hp = v(P_qhp) >= 3 ? hzv((float) v(P_qhp), 20, 500) : 0.0f;
        x.strokeStyle(x.pal.grid); x.lineWidth = 1;
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f }) x.line(X(f) + 0.5f, 0, X(f) + 0.5f, h);
        if (on) { x.fillStyle(x.pal.fill); x.fillRect(0, 0, X(fm), h); }
        x.beginPath();
        for (int i = 0; i <= (int) w; ++i) {
            const float f = 20 * std::pow(100.0f, i / w); const float db = on && hp > 0 ? -10 * log10f_(1 + std::pow(hp / f, 4.0f)) : 0.0f;
            const float py = clampv(Y(db), 2.0f, h - 2); if (i == 0) x.moveTo((float) i, py); else x.lineTo((float) i, py);
        }
        x.strokeStyle(x.pal.line); x.lineWidth = 1.8f; x.stroke();
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText(on ? "MONO < " + juce::String(rnd(fm)) + " Hz" : juce::String("OFF"), 3, h - 4);
        x.textAlign(1); x.fillText(on && hp > 0 ? "HPF " + juce::String(rnd(hp)) : juce::String("HPF OFF"), w - 3, 10);
        drawSpec(x, w, lv, kTapDrive, (float) env.sampleRate, 20, 2000, 6, h - 2);
    }
};

// ---------------------------------------------------------------- cv_qm
class MSEq : public FxBase {
public:
    explicit MSEq(DisplayEnv& e) : FxBase(e, 270, 190) {}
    void update() override { lv.update(env, LiveCtx::bit(kTapEq), 0); }
    void draw(Ctx& x, float w, float h) override {
        auto X = [&](float f) { return std::log(f / 20) / std::log(1000.0f) * w; };
        auto Y = [&](float db) { return h / 2 - db / 9 * (h / 2 - 8); };
        const bool on = v(P_qon) && v(P_qeon);
        x.strokeStyle(x.pal.grid); x.lineWidth = 1;
        for (float f : { 100.0f, 1000.0f, 10000.0f }) x.line(X(f) + 0.5f, 0, X(f) + 0.5f, h);
        x.line(0, Y(0) + 0.5f, w, Y(0) + 0.5f);
        auto curve = [&](auto fn, juce::Colour col) {
            x.beginPath();
            for (int i = 0; i <= (int) w; ++i) { const float f = 20 * std::pow(1000.0f, i / w); const float py = clampv(Y(on ? fn(f) : 0.0f), 2.0f, h - 2); if (i == 0) x.moveTo((float) i, py); else x.lineTo((float) i, py); }
            x.strokeStyle(col); x.lineWidth = 1.8f; x.stroke();
        };
        const float sh = hzv((float) v(P_qsh), 80, 400); const float mp = v(P_qmp) / 2.0f, ai = v(P_qai) / 2.0f;
        curve([&](float f) { return bell(f, 3000, mp, 0.8f) + shelf(f, 10000, ai); }, x.pal.line);
        curve([&](float f) { return -10 * log10f_(1 + std::pow(sh / f, 4.0f)) + shelf(f, 10000, ai); }, x.pal.acc);
        x.mono(15.5f); x.textAlign(-1); x.fillStyle(x.pal.line); x.fillText("MID", 3, 10); x.fillStyle(x.pal.acc); x.fillText("SIDE", 30, 10);
        drawSpec(x, w, lv, kTapEq, (float) env.sampleRate, 20, 20000, 14, h - 2);
    }
};

// ---------------------------------------------------------------- cv_qt
class Trans : public FxBase {
public:
    explicit Trans(DisplayEnv& e) : FxBase(e, 270, 190) {}
    void update() override { lv.update(env, 0, LiveCtx::bit(kTapDrive)); lv.pushHist(true); }
    void draw(Ctx& x, float w, float h) override {
        const bool on = v(P_qon) && v(P_qtn);
        auto env_ = [&](float a, float s) {
            x.beginPath();
            for (int i = 0; i <= (int) w; ++i) {
                const float t = i / w; float y = t < 0.08f ? t / 0.08f * a : (s + (a - s) * std::exp(-(t - 0.08f) * 9));
                if (t > 0.78f) y *= std::max(0.0f, 1 - (t - 0.78f) / 0.2f);
                const float py = h - 6 - y * (h - 16); if (i == 0) x.moveTo((float) i, py); else x.lineTo((float) i, py);
            }
        };
        const float a = on ? 1 + v(P_qta) / 12.0f * 0.5f : 1.0f, s = on ? 0.45f * (1 + v(P_qts) / 12.0f * 0.6f) : 0.45f;
        x.setLineDash({ 2, 3 }); env_(1, 0.45f); x.strokeStyle(x.pal.grid); x.lineWidth = 1.2f; x.stroke(); x.clearDash();
        env_(clampv(a, 0.3f, 1.5f), clampv(s, 0.1f, 0.9f)); x.strokeStyle(x.pal.line); x.lineWidth = 1.8f; x.stroke();
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText("NOTE SHAPE", 3, 10);
        const int n = (int) lv.hist.size(); if (n < 2) return;
        x.beginPath();
        for (int i = 0; i < n; ++i) { const float px = (float) i / (n - 1) * w, py = h - 6 - clampv((lv.hist[(size_t) i] + 60) / 60, 0.0f, 1.0f) * (h - 16); if (i == 0) x.moveTo(px, py); else x.lineTo(px, py); }
        x.strokeStyle(x.pal.acc); x.lineWidth = 1.6f; x.stroke();
    }
};

// ---------------------------------------------------------------- cv_qr
class Tamer : public FxBase {
public:
    explicit Tamer(DisplayEnv& e) : FxBase(e, 270, 190) {}
    void update() override { lv.update(env, LiveCtx::bit(kTapEq), 0); }
    void draw(Ctx& x, float w, float h) override {
        auto X = [&](float f) { return std::log(f / 20) / std::log(1000.0f) * w; };
        const bool on = v(P_qon) && v(P_qrn); const float d = on ? v(P_qrd) / 127.0f * 0.7f : 0.0f; const int f = v(P_qrf);
        auto sp = [](float ff) { return 0.35f + 0.18f * std::exp(-std::pow(std::log2(ff / 300), 2.0f) / 3) + 0.28f * std::exp(-std::pow(std::log2(ff / 4000) * 3, 2.0f)) + 0.22f * std::exp(-std::pow(std::log2(ff / 8000) * 3, 2.0f)); };
        auto draw = [&](auto fn, juce::Colour col, bool dash) {
            x.beginPath();
            for (int i = 0; i <= (int) w; ++i) { const float ff = 20 * std::pow(1000.0f, i / w); const float py = h - 6 - fn(ff) * (h - 18); if (i == 0) x.moveTo((float) i, py); else x.lineTo((float) i, py); }
            if (dash) x.setLineDash({ 2, 3 }); else x.clearDash();
            x.strokeStyle(col); x.lineWidth = dash ? 1.2f : 1.8f; x.stroke(); x.clearDash();
        };
        x.strokeStyle(x.pal.grid); x.lineWidth = 1;
        for (float ff : { 100.0f, 1000.0f, 10000.0f }) x.line(X(ff) + 0.5f, 0, X(ff) + 0.5f, h);
        draw(sp, x.pal.grid, true);
        draw([&](float ff) { return sp(ff) * (1 - d * ((f != 1 ? std::exp(-std::pow(std::log2(ff / 4000) * 3, 2.0f)) : 0.0f) + (f != 0 ? std::exp(-std::pow(std::log2(ff / 8000) * 3, 2.0f)) : 0.0f)) * 0.6f); }, x.pal.line, false);
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText("HARSH PEAKS", 3, 10);
        drawSpec(x, w, lv, kTapEq, (float) env.sampleRate, 20, 20000, 12, h - 6);
    }
};

// ---------------------------------------------------------------- cv_qg
class Glue : public FxBase {
public:
    explicit Glue(DisplayEnv& e) : FxBase(e, 270, 190) {}
    void update() override { lv.update(env, 0, LiveCtx::bit(kTapMb) | LiveCtx::bit(kTapGlue)); }
    void draw(Ctx& x, float w, float h) override {
        static const float rt[4] = { 1.5f, 2, 3, 4 };
        const bool on = v(P_qon) && v(P_qcn); const float th = -30 * (1 - v(P_qct) / 127.0f); const float r = rt[juce::jlimit(0, 3, v(P_qcr))];
        auto X = [&](float db) { return (db + 40) / 40 * w; };
        auto Y = [&](float db) { return h - 6 - (db + 40) / 40 * (h - 14); };
        x.strokeStyle(x.pal.grid); x.lineWidth = 1; x.setLineDash({ 2, 3 }); x.line(X(-40), Y(-40), X(0), Y(0)); x.clearDash();
        x.beginPath();
        for (int i = 0; i <= (int) w; ++i) { const float d = -40 + i / w * 40; const float o = !on || d <= th ? d : th + (d - th) / r; const float py = Y(o); if (i == 0) x.moveTo((float) i, py); else x.lineTo((float) i, py); }
        x.strokeStyle(x.pal.line); x.lineWidth = 1.8f; x.stroke();
        if (on) { x.strokeStyle(x.pal.acc); x.setLineDash({ 3, 3 }); x.line(X(th), 0, X(th), h); x.clearDash(); }
        const float gr = on ? std::max(0.0f, (-14 - th) * (1 - 1 / r)) : 0.0f;
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText("GR ~" + toFixed(gr, 1) + " dB @ -14 in", 3, 10);
        if (!lv.act) return;
        auto Xc = [&](float db) { return (clampv(db, -40.0f, 0.0f) + 40) / 40 * w; };
        auto Yc = [&](float db) { return h - 6 - (clampv(db, -40.0f, 0.0f) + 40) / 40 * (h - 14); };
        drawDot(x, Xc(lv.lvl[kTapMb]), Yc(lv.lvl[kTapGlue]), 5);
        const float g = env.live ? std::max(0.0f, -env.live->gluGR.load()) : 0.0f;
        x.fillStyle(x.pal.acc); x.mono(15.5f); x.textAlign(1); x.fillText("live GR " + toFixed(g, 1) + " dB", w - 3, 10);
    }
};

// ---------------------------------------------------------------- cv_qw
class Width : public FxBase {
public:
    explicit Width(DisplayEnv& e) : FxBase(e, 270, 190) {}
    void update() override { lv.update(env, 0, 0, true); }
    void draw(Ctx& x, float w, float h) override {
        const bool on = v(P_qon) && v(P_qwn); const float a = on ? v(P_qwa) / 127.0f * 0.9f : 0.0f; const float R = h / 2 - 8;
        x.strokeStyle(x.pal.grid); x.lineWidth = 1; x.beginPath(); x.moveTo(w / 2 - R - 6, h / 2 + 0.5f); x.lineTo(w / 2 + R + 6, h / 2 + 0.5f); x.moveTo(w / 2 + 0.5f, 6); x.lineTo(w / 2 + 0.5f, h - 6); x.stroke();
        x.beginPath();
        for (int i = 0; i <= 120; ++i) {
            const float t = i / 120.0f * 6.2831853f; const float L = std::cos(t), Rr = (1 - a) * std::cos(t) + a * std::sin(t);
            const float px = w / 2 + (L - Rr) * R * 0.7f, py = h / 2 - (L + Rr) * R * 0.5f; if (i == 0) x.moveTo(px, py); else x.lineTo(px, py);
        }
        x.strokeStyle(x.pal.line); x.lineWidth = 1.8f; x.stroke();
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText("STEREO FIELD", 3, 10); x.textAlign(1); x.fillText("WIDTH " + juce::String(rnd(a / 0.9f * 100)) + "%", w - 3, 10);
        if (!lv.act) return;
        const float R0 = h / 2 - 8, pk = std::max(0.2f, lv.pk[kTapOut] * 1.1f), G = 0.9f / pk;
        x.fillStyle(x.pal.acc); x.globalAlpha = 0.55f;
        for (int i = 0; i < 1024; i += 3) { const float l = lv.preL[(size_t) i] * G, r = lv.preR[(size_t) i] * G; x.fillRect(w / 2 + (l - r) * R0 * 0.7f - 1, h / 2 - (l + r) * R0 * 0.5f - 1, 2, 2); }
        x.globalAlpha = 1;
    }
};

// ---------------------------------------------------------------- cv_qp
class Pan : public FxBase {
public:
    explicit Pan(DisplayEnv& e) : FxBase(e, 270, 190) {}
    void update() override { lv.update(env, 0, 0, false, true); }
    void draw(Ctx& x, float w, float h) override {
        const bool on = v(P_qon) && v(P_qpn); const float dp = on ? v(P_qpd) / 127.0f : 0.0f; const float hz = hzv((float) v(P_qpr), 0.05f, 4);
        x.strokeStyle(x.pal.grid); x.lineWidth = 1; x.line(0, h / 2 + 0.5f, w, h / 2 + 0.5f);
        x.beginPath();
        for (int i = 0; i <= (int) w; ++i) { const float t = i / w * std::min(3.0f, hz * 8); const float py = h / 2 - std::sin(t * 6.2831853f) * dp * (h / 2 - 12); if (i == 0) x.moveTo((float) i, py); else x.lineTo((float) i, py); }
        x.strokeStyle(x.pal.line); x.lineWidth = 1.8f; x.stroke();
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText("L", 3, 12); x.fillText("R", 3, h - 4); x.textAlign(1); x.fillText(on ? toFixed(hz, 2) + " Hz" : juce::String("OFF"), w - 3, 10);
        if (!lv.act) return;
        auto bar = [&](float px, float vv) { const float bh = clampv(vv, 0.0f, 1.0f) * (h - 24); x.fillStyle(x.pal.acc); x.fillRect(px, h - 8 - bh, 8, bh); };
        bar(w - 46, lv.oL); bar(w - 32, lv.oR);
        const float t = lv.oL + lv.oR, bal = t > 0.001f ? (lv.oR - lv.oL) / t : 0.0f;
        drawDot(x, w / 2, h / 2 - clampv(bal * 2, -1.0f, 1.0f) * (h / 2 - 12), 5);
    }
};

// ---------------------------------------------------------------- cv_qo
class OutStage : public FxBase {
public:
    explicit OutStage(DisplayEnv& e) : FxBase(e, 770, 150) {}
    void update() override { lv.update(env, 0, 0); }
    void draw(Ctx& x, float w, float h) override {
        const bool on = v(P_qon); const float ce = std::pow(10.0f, (-3 + v(P_qlc) / 10.0f) / 20);
        const float bst = on ? std::pow(10.0f, v(P_qll) / 127.0f * 12 / 20) : 1.0f; const float ck = 1 + v(P_qka) / 127.0f * 3; const bool clipOn = on && v(P_qko);
        auto Y = [&](float vv) { return h / 2 - vv * (h / 2 - 6); };
        x.strokeStyle(x.pal.acc); x.setLineDash({ 3, 3 }); x.lineWidth = 1;
        for (float vv : { ce, -ce }) x.line(0, Y(vv) + 0.5f, w, Y(vv) + 0.5f);
        x.clearDash();
        x.beginPath();
        for (int i = 0; i <= (int) w; ++i) {
            float vv = std::sin(i / w * 3.14159265f * 4) * 0.55f * bst;
            if (clipOn) vv = std::tanh(ck * vv) / std::tanh(ck);
            vv = clampv(vv, -ce, ce); const float py = Y(vv); if (i == 0) x.moveTo((float) i, py); else x.lineTo((float) i, py);
        }
        x.strokeStyle(x.pal.line); x.lineWidth = 1.8f; x.stroke();
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1); x.fillText("PEAKS", 3, 10); x.textAlign(1); x.fillText(toFixed(-3 + v(P_qlc) / 10.0, 1) + " dB ceiling", w - 3, 10);
        if (!lv.act) return;
        const auto& S = lv.td[kTapOut]; int st = 0;
        for (int i = 1; i < 512; ++i) if (S[(size_t) i - 1] < 0 && S[(size_t) i] >= 0) { st = i; break; }
        x.beginPath();
        for (int i = 0; i <= (int) w; ++i) { const float vv = clampv(S[(size_t) (st + (int) std::floor(i / w * 500))], -1.0f, 1.0f); if (i == 0) x.moveTo((float) i, Y(vv)); else x.lineTo((float) i, Y(vv)); }
        x.strokeStyle(x.pal.acc); x.lineWidth = 1.5f; x.globalAlpha = 0.9f; x.stroke(); x.globalAlpha = 1;
    }
};

// ---------------------------------------------------------------- cv_qa
class OptoDial : public FxBase {
public:
    explicit OptoDial(DisplayEnv& e) : FxBase(e, 490, 150) {}
    void update() override { lv.update(env, 0, 0); }
    void draw(Ctx& x, float w, float h) override {
        const bool on = v(P_qon) && v(P_qoon);
        const bool live = on && voices();
        const float gr = live ? clampv(-env.live->optoGR.load(), 0.0f, 16.0f) : (on ? v(P_qopr) / 100.0f * (v(P_qomd) ? 16 : 12) * (v(P_qomx) / 127.0f) : 0.0f);
        const float cx0 = w / 2, cy0 = h - 14, R = std::min(w / 2 - 20, h - 34);
        const float PI = 3.14159265f;
        x.strokeStyle(x.pal.grid); x.lineWidth = 1; x.beginPath(); x.arc(cx0, cy0, R, PI * 1.15f, PI * 1.85f); x.stroke();
        for (int i = 0; i <= 10; ++i) {
            const float a = PI * 1.15f + i / 10.0f * PI * 0.7f; const float r0 = R - (i % 5 == 0 ? 9 : 5);
            x.strokeStyle(i > 7 ? x.pal.acc : x.pal.txt); x.lineWidth = i % 5 == 0 ? 1.6f : 1.0f;
            x.line(cx0 + std::cos(a) * r0, cy0 + std::sin(a) * r0, cx0 + std::cos(a) * R, cy0 + std::sin(a) * R);
        }
        const float a = PI * 1.15f + clampv(gr / 14, 0.0f, 1.0f) * PI * 0.7f;
        x.strokeStyle(x.pal.line); x.lineWidth = 2; x.line(cx0, cy0, cx0 + std::cos(a) * (R - 4), cy0 + std::sin(a) * (R - 4));
        x.fillStyle(x.pal.line); x.fillCircle(cx0, cy0, 4);
        x.fillStyle(x.pal.txt); x.mono(17); x.textAlign(0); x.fillText("GAIN REDUCTION  ~" + toFixed(gr, 1) + " dB", cx0, h - 1);
    }
};

// ---------------------------------------------------------------- cv_qv (OTT)
class Ott : public FxBase {
public:
    explicit Ott(DisplayEnv& e) : FxBase(e, 490, 170) {
        static const char* d[3] = { "qvdl", "qvdm", "qvdh" }; static const char* u[3] = { "qvul", "qvum", "qvuh" };
        for (int i = 0; i < 3; ++i) { dn[i] = pid(d[i]); up[i] = pid(u[i]); }
    }
    void update() override { lv.update(env, 0, 0); }
    void draw(Ctx& x, float w, float h) override {
        const bool on = v(P_qon) && v(P_qvon); const bool live = on && voices();
        const auto G = ottGeo(w, h); static const char* names[3] = { "LOW", "MID", "HIGH" };
        x.mono(17); x.textAlign(0);
        for (int i = 0; i < 3; ++i) {
            const float X = G.x0 + i * (G.bw + G.gap), dnv = v(dn[i]) / 127.0f, upv = v(up[i]) / 127.0f, ty = G.yT + dnv * 0.45f * G.H, by = G.yB - upv * 0.45f * G.H;
            x.globalAlpha = on ? 1.0f : 0.4f;
            x.fillStyle(x.pal.grid); x.fillRect(X, G.yT, G.bw, G.H);
            juce::ColourGradient g1(rgba(255, 19, 131, 0.06f), 0, G.yT, rgba(255, 19, 131, 0.55f), 0, ty + 1, false); x.fillGradient(g1); x.fillRect(X, G.yT, G.bw, ty - G.yT);
            juce::ColourGradient g2(hex(0xa43dff), 0, ty, hex(0x6cc9f5), 0, by, false); x.fillGradient(g2); x.fillRect(X, ty, G.bw, std::max(2.0f, by - ty));
            juce::ColourGradient g3(rgba(108, 201, 245, 0.55f), 0, by, rgba(108, 201, 245, 0.06f), 0, G.yB, false); x.fillGradient(g3); x.fillRect(X, by, G.bw, G.yB - by);
            if (live) { const float gr = clampv(-env.live->ottGR[i].load(), 0.0f, 24.0f); x.fillStyle(hex(0xff1383)); x.fillRect(X + G.bw - 7, G.yT, 5, gr / 24 * G.H * 0.5f); }
            const float ys[2] = { ty, by }; const juce::Colour cs[2] = { hex(0xff1383), hex(0x6cc9f5) };
            for (int k = 0; k < 2; ++k) { x.fillStyle(cs[k]); x.fillRect(X - 3, ys[k] - 3, G.bw + 6, 6); x.fillStyle(rgba(255, 255, 255, 0.85f)); x.fillRect(X + G.bw / 2 - 10, ys[k] - 1, 20, 2); }
            x.fillStyle(x.pal.txt);
            x.fillText(juce::String::fromUTF8("\xE2\x96\xBC ") + juce::String(rnd(dnv * 100)) + "%", X + G.bw / 2 - 4, G.yT + 13);
            x.fillText(juce::String::fromUTF8("\xE2\x96\xB2 ") + juce::String(rnd(upv * 100)) + "%", X + G.bw / 2, G.yB - 5);
            x.fillText(names[i], X + G.bw / 2, h - 6);
            x.globalAlpha = 1;
        }
    }
    // drag the top or bottom edge of a band; double click resets the band
    void mouseDown(const juce::MouseEvent& e) override {
        const auto p = toCanvas(e.position); const auto G = ottGeo((float) W, (float) H); const int i = bandAt(p.x, G);
        const float dnv = v(dn[i]) / 127.0f, upv = v(up[i]) / 127.0f, ty = G.yT + dnv * 0.45f * G.H, by = G.yB - upv * 0.45f * G.H;
        dragBand = i; dragWhich = std::abs(p.y - ty) <= std::abs(p.y - by) ? 0 : 1;
        dragIdx = dragWhich == 0 ? dn[i] : up[i]; if (env.beginGesture) env.beginGesture(dragIdx);
        moveTo(p);
    }
    void mouseDrag(const juce::MouseEvent& e) override { if (dragBand >= 0) moveTo(toCanvas(e.position)); }
    void mouseUp(const juce::MouseEvent&) override { endDrag(); }
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        endDrag(); const auto G = ottGeo((float) W, (float) H); const int i = bandAt(toCanvas(e.position).x, G);
        for (int idx : { dn[i], up[i] }) { if (env.beginGesture) env.beginGesture(idx); env.set(idx, kParams[idx].def); if (env.endGesture) env.endGesture(idx); }
        repaint();
    }
private:
    int dn[3], up[3]; int dragBand = -1, dragWhich = 0, dragIdx = -1;
    static int bandAt(float px, const OttGeo& G) { return clampv((int) std::floor((px - G.x0 + G.gap / 2) / (G.bw + G.gap)), 0, 2); }
    void moveTo(juce::Point<float> p) {
        const auto G = ottGeo((float) W, (float) H);
        const float vv = dragWhich == 0 ? (p.y - G.yT) / (0.45f * G.H) : (G.yB - p.y) / (0.45f * G.H);
        env.set(dragIdx, rnd(clampv(vv, 0.0f, 1.0f) * 127)); repaint();
    }
    void endDrag() { if (dragBand >= 0) { if (env.endGesture) env.endGesture(dragIdx); } dragBand = -1; }
};

// ---------------------------------------------------------------- cv_qdm (multiband)
class Multiband : public FxBase {
public:
    explicit Multiband(DisplayEnv& e) : FxBase(e, 2320, 236) {
        for (int b = 1; b <= 4; ++b) { const juce::String p = "qm" + juce::String(b); mo[b - 1] = pid(p + "o"); mu[b - 1] = pid(p + "u"); ms[b - 1] = pid(p + "s"); mt[b - 1] = pid(p + "t"); mr[b - 1] = pid(p + "r"); }
        fx[0] = pid("qmx1"); fx[1] = pid("qmx2"); fx[2] = pid("qmx3");
    }
    void update() override { lv.update(env, 0, 0); }
    juce::String infoText(int slot) const override {
        const auto fs = mbFr(env.get); if (slot < 0 || slot > 3) return {};
        if (slot == 0) return "20 Hz to " + fmtHzK(fs[0]);
        if (slot == 3) return fmtHzK(fs[2]) + " up";
        return fmtHzK(fs[(size_t) slot - 1]) + " to " + fmtHzK(fs[(size_t) slot]);
    }
    void draw(Ctx& x, float w, float h) override {
        const bool e = v(P_qon) && v(P_qmon); const bool live = e && voices();
        auto X = [&](float f) { return std::log(f / 20) / std::log(1000.0f) * w; };
        const auto fsA = mbFr(env.get); const float ed[5] = { 20, fsA[0], fsA[1], fsA[2], 20000 };
        static const int cols[4][3] = { { 60, 110, 255 }, { 140, 70, 255 }, { 200, 60, 235 }, { 255, 40, 140 } };
        static const char* nm[4] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
        bool solo = false; for (int b = 0; b < 4; ++b) if (v(ms[b])) solo = true;
        auto yth = [&](float th) { return 14 + (-th / 48) * (h - 62); };
        for (int i = 0; i < 4; ++i) {
            const float xa = X(ed[i]), xb = X(ed[i + 1]);
            const bool bon = e && v(mo[i]) && !(v(mu[i]) || (solo && !v(ms[i])));
            x.globalAlpha = bon ? 1.0f : 0.32f;
            juce::ColourGradient gg(rgba(cols[i][0], cols[i][1], cols[i][2], 0.42f), 0, 0, rgba(cols[i][0], cols[i][1], cols[i][2], 0.08f), 0, h, false);
            x.fillGradient(gg); x.fillRect(xa, 0, xb - xa, h);
            const float th = mbThr((float) v(mt[i])), r = mbRat((float) v(mr[i]));
            x.strokeStyle(x.pal.acc); x.lineWidth = 1.2f; x.setLineDash({ 5, 4 }); x.line(xa + 4, yth(th), xb - 4, yth(th)); x.clearDash();
            float gc = bon ? std::max(0.0f, (-14 - th) * (1 - 1 / r)) : 0.0f, gl = 0;
            if (live && bon) { gc = clampv(-env.live->mbCompGR[i].load(), 0.0f, 30.0f); gl = clampv(-env.live->mbLimGR[i].load(), 0.0f, 30.0f); }
            const float bw = std::min(22.0f, (xb - xa) * 0.12f), cxm = (xa + xb) / 2;
            x.fillStyle(hex(0xff1383)); x.fillRect(cxm - bw - 3, 0, bw, gc / 24 * (h - 50));
            x.fillStyle(hex(0x6cc9f5)); x.fillRect(cxm + 3, 0, bw, gl / 24 * (h - 50));
            x.fillStyle(x.pal.txt); x.mono(17); x.textAlign(0);
            x.fillText(juce::String(nm[i]) + (bon ? "" : (v(mo[i]) ? "  MUTED" : "  OFF")), cxm, h - 22);
            if (bon) x.fillText("GR " + toFixed(gc, 1) + " / " + toFixed(gl, 1) + " dB", cxm, h - 6);
            x.globalAlpha = 1;
        }
        x.strokeStyle(x.pal.grid); x.lineWidth = 1;
        for (float f : { 100.0f, 1000.0f, 10000.0f }) x.line(X(f) + 0.5f, 0, X(f) + 0.5f, h);
        x.fillStyle(x.pal.txt); x.mono(15.5f); x.textAlign(-1);
        x.fillText("100", X(100) + 4, h - 40); x.fillText("1k", X(1000) + 4, h - 40); x.fillText("10k", X(10000) + 4, h - 40);
        x.lineWidth = 2;
        for (float f : fsA) {
            x.strokeStyle(x.pal.line); x.line(X(f), 0, X(f), h);
            x.fillStyle(x.pal.line); x.beginPath(); x.moveTo(X(f) - 6, 0); x.lineTo(X(f) + 6, 0); x.lineTo(X(f), 9); x.fill();
            x.fillStyle(x.pal.txt); x.textAlign(-1); x.fillText(fmtHz(f), X(f) + 8, 14);
        }
    }
    void mouseMove(const juce::MouseEvent& e) override {
        setMouseCursor(nearest(toCanvas(e.position).x) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        drag = nearest(toCanvas(e.position).x);
        if (drag >= 0 && env.beginGesture) env.beginGesture(fx[drag]);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (drag < 0) return;
        static const float lo[3] = { 40, 300, 4000 }, hi[3] = { 400, 3000, 16000 };
        const float f = 20 * std::pow(1000.0f, clampv(toCanvas(e.position).x / (float) W, 0.0f, 1.0f));
        env.set(fx[drag], clampv(rnd(127 * std::log(f / lo[drag]) / std::log(hi[drag] / lo[drag])), 0, 127)); repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { if (drag >= 0 && env.endGesture) env.endGesture(fx[drag]); drag = -1; }
private:
    int mo[4], mu[4], ms[4], mt[4], mr[4], fx[3]; int drag = -1;
    int nearest(float x) const {
        const auto fs = mbFr(env.get); int best = -1; float bd = 14;
        for (int i = 0; i < 3; ++i) { const float d = std::abs(std::log(fs[(size_t) i] / 20) / std::log(1000.0f) * (float) W - x); if (d < bd) { bd = d; best = i; } }
        return best;
    }
};

} // namespace

std::unique_ptr<Display> makeFxDisplay(const juce::String& id, DisplayEnv& e) {
    if (id == "cv_qf") return std::make_unique<Flow>(e, 496, 110);
    if (id == "cv_qf2") return std::make_unique<Flow>(e, 610, 110);
    if (id == "cv_q") return std::make_unique<Tone>(e);
    if (id == "cv_qd") return std::make_unique<DriveCurve>(e);
    if (id == "cv_ql") return std::make_unique<LowEnd>(e);
    if (id == "cv_qm") return std::make_unique<MSEq>(e);
    if (id == "cv_qt") return std::make_unique<Trans>(e);
    if (id == "cv_qr") return std::make_unique<Tamer>(e);
    if (id == "cv_qg") return std::make_unique<Glue>(e);
    if (id == "cv_qw") return std::make_unique<Width>(e);
    if (id == "cv_qp") return std::make_unique<Pan>(e);
    if (id == "cv_qo") return std::make_unique<OutStage>(e);
    if (id == "cv_qa") return std::make_unique<OptoDial>(e);
    if (id == "cv_qv") return std::make_unique<Ott>(e);
    if (id == "cv_qdm") return std::make_unique<Multiband>(e);
    return nullptr;
}

} // namespace jp::disp
