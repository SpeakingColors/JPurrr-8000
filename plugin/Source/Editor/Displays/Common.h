// Shared helpers of the display canvases: a small canvas-2D style drawing context on top of juce::Graphics,
// the mockup's helper maths (hzv, tm, curveD, shapeCurve, mbFr, ottGeo...), an AnalyserNode style spectrum
// and the CanvasDisplay base class (chrome, canvas coordinates).
#pragma once
#include "../Display.h"
#include <array>
#include <cmath>
#include <vector>

namespace jp::disp {

using jpurrr::kNumParams;

template <class T> inline T clampv(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }

// ---------------------------------------------------------------- canvas style drawing context
// Mirrors the CanvasRenderingContext2D calls the mockup uses, so the draw code can be ported line by line.
class Ctx {
public:
    Ctx(juce::Graphics& gr, const DisplayEnv& e) : g(gr), env(e), pal(e.pal) {}

    juce::Graphics& g;
    const DisplayEnv& env;
    const Palette& pal;
    float lineWidth = 1.0f, globalAlpha = 1.0f;

    void fillStyle(juce::Colour c) { fillCol = c; grad = false; }
    void fillGradient(const juce::ColourGradient& cg) { fillGrad = cg; grad = true; }
    void strokeStyle(juce::Colour c) { strokeCol = c; }
    void setLineDash(std::initializer_list<float> d) { dash.assign(d.begin(), d.end()); }
    void clearDash() { dash.clear(); }

    // fonts: px is the mockup's CSS font size
    void mono(float px) { font = env.mono(px); fontPx = px; }
    void textAlign(int a) { align = a; }  // -1 left, 0 center, 1 right

    void beginPath() { path.clear(); }
    void moveTo(float x, float y) { path.startNewSubPath(x, y); }
    void lineTo(float x, float y) { if (path.isEmpty()) path.startNewSubPath(x, y); else path.lineTo(x, y); }
    void closePath() { path.closeSubPath(); }
    void rect(float x, float y, float w, float h) { path.addRectangle(x, y, w, h); }
    void arc(float cx, float cy, float r, float a0, float a1);   // canvas angles (0 = 3 o'clock, clockwise)

    void stroke() {
        g.setColour(strokeCol.withMultipliedAlpha(globalAlpha));
        if (dash.empty()) { g.strokePath(path, juce::PathStrokeType(lineWidth)); return; }
        juce::Path d; juce::PathStrokeType(lineWidth).createDashedStroke(d, path, dash.data(), (int) dash.size());
        g.fillPath(d);
    }
    void fill() { applyFill(); g.fillPath(path); }
    void fillRect(float x, float y, float w, float h) { applyFill(); g.fillRect(x, y, w, h); }
    void strokeRect(float x, float y, float w, float h) {
        juce::Path p; p.addRectangle(x, y, w, h); g.setColour(strokeCol.withMultipliedAlpha(globalAlpha));
        if (dash.empty()) g.strokePath(p, juce::PathStrokeType(lineWidth));
        else { juce::Path d; juce::PathStrokeType(lineWidth).createDashedStroke(d, p, dash.data(), (int) dash.size()); g.fillPath(d); }
    }
    void fillCircle(float cx, float cy, float r) { applyFill(); g.fillEllipse(cx - r, cy - r, 2 * r, 2 * r); }
    // fillText with the mockup's fixCtx rule: the baseline is never above 0.8 * font size + 1
    void fillText(const juce::String& s, float x, float y);

    // helpers used by several displays
    void line(float x0, float y0, float x1, float y1) { beginPath(); moveTo(x0, y0); lineTo(x1, y1); stroke(); }

    juce::Path path;
private:
    void applyFill() {
        if (grad) { auto cg = fillGrad; for (int i = 0; i < cg.getNumColours(); ++i) cg.setColour(i, cg.getColour(i).withMultipliedAlpha(globalAlpha)); g.setGradientFill(cg); }
        else g.setColour(fillCol.withMultipliedAlpha(globalAlpha));
    }
    juce::Colour fillCol { 0xff000000 }, strokeCol { 0xff000000 };
    juce::ColourGradient fillGrad; bool grad = false;
    std::vector<float> dash;
    juce::Font font; float fontPx = 12.0f; int align = -1;
};

// ---------------------------------------------------------------- mockup helper maths
inline float hzv(float v, float lo, float hi) { return lo * std::pow(hi / lo, v / 127.0f); }
inline float tm(float v, float lo, float hi) { return lo * std::pow(hi / lo, v / 127.0f); }
inline float mbThr(float v) { return -48.0f * (1.0f - v / 127.0f); }
inline float mbRat(float v) { return 1.0f + 19.0f * std::pow(v / 127.0f, 1.6f); }
float curveD(float x);
std::vector<float> shapeCurve(int dv, int dx, int df);   // 2048 points, as the mockup's drive curve
struct OttGeo { float gap, x0, yT, yB, H, bw; };
inline OttGeo ottGeo(float w, float h) { OttGeo g; g.gap = 14; g.x0 = 10; g.yT = 8; g.yB = h - 24; g.H = g.yB - g.yT; g.bw = (w - 2 * g.x0 - 2 * g.gap) / 3; return g; }
std::array<float, 3> mbFr(const std::function<int(int)>& get);  // crossover frequencies from qmx1..3
juce::String fmtHz(float f);                // "123 Hz" / "1.2 k"  (mockup canvas label)
juce::String fmtHzK(float f);               // "123 Hz" / "1.2 kHz" (mockup DOM label)
juce::String toFixed(double v, int digits); // JS toFixed

// param index of "qm<b><suffix>" etc. ("qm1o"), cached
int pid(const juce::String& id);
inline float fr_(float p) { return p - std::floor(p); }

// ---------------------------------------------------------------- AnalyserNode style spectrum
// fftSize 1024, Blackman window, smoothingTimeConstant 0.6, output in dB like getFloatFrequencyData (512 bins).
class Analyser {
public:
    Analyser();
    void process(const float* td1024, float tau = 0.6f);
    const float* db() const { return out; }
private:
    float smooth[512], out[512];
};

// ---------------------------------------------------------------- live data (taps of env.live), one per display
class LiveCtx {
public:
    // taps in specMask get a spectrum, taps in lvlMask get a peak / level (the out tap is always measured)
    void update(const DisplayEnv& env, unsigned specMask, unsigned lvlMask, bool wantPre = false, bool wantOutLR = false);
    bool act = false, was = false;                    // LV.act / LV.was: output level above -72 dB
    float lvl[jpurrr::kNumTaps] = {}, pk[jpurrr::kNumTaps] = {};
    std::array<std::vector<float>, jpurrr::kNumTaps> td;   // time domain, only for taps requested
    std::array<Analyser, jpurrr::kNumTaps> an;
    std::vector<float> preL, preR;                   // stereo chain input (wantPre)
    float oL = 0, oR = 0;                             // output peaks L and R (wantOutLR)
    std::vector<float> hist;                          // drive level history (cv_qt), 160 entries
    void pushHist(bool wantDrv);
    bool built = false;
    static unsigned bit(int tap) { return 1u << tap; }
};
// spectrum fill under the curve, like the mockup's lvSpec
void drawSpec(Ctx& x, float w, const LiveCtx& lv, int tap, float sr, float fLo, float fHi, float yTop, float yBot);
void drawDot(Ctx& x, float px, float py, float r = 5.0f);

// ---------------------------------------------------------------- base class
class CanvasDisplay : public Display {
public:
    CanvasDisplay(DisplayEnv& e, int canvasW, int canvasH);
    void paint(juce::Graphics& g) override;
    void tick() override;
    static void setChrome(bool on);                   // test hook: paint without background, shadow and border
    int cw() const { return W; }
    int ch() const { return H; }
protected:
    virtual void draw(Ctx& x, float w, float h) = 0;  // canvas coordinates, 0..w, 0..h
    virtual void update() {}                          // per tick state (live analysis, animation clocks)
    juce::Point<float> toCanvas(juce::Point<float> p) const { return p - origin(); }
    juce::Point<float> origin() const { return { (float) ((getWidth() - W) / 2), (float) ((getHeight() - H) / 2) }; }
    int W, H;
    double lastTick = -1; double dtTick = 0.033;
    float dt() const { return (float) dtTick; }
    int touchIdx = -1;
};

std::unique_ptr<Display> makeOscDisplay(const juce::String& id, DisplayEnv& env);
std::unique_ptr<Display> makeFxDisplay(const juce::String& id, DisplayEnv& env);
std::unique_ptr<Display> makeControlDisplay(const juce::String& id, DisplayEnv& env);

} // namespace jp::disp
