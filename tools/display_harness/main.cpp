// Display harness: renders the cv_* displays to PNG (2x pixel density) for given parameter values and synthetic live data.
// usage: jpurrr_displays states.json outdir [--chrome]
//   states.json: [{ "name": "default", "params": {"o1w": 3}, "t": 1.0, "scr": 0.37, "touch": ["o1c1"],
//                   "live": {"kind": "sine|saw|noise|silence|stereo", "freq": 440, "amp": 0.5, "voices": 1, "gatePhase": 3,
//                            "glu": -3, "opto": -6, "ott": [-4,-8,-2], "mbc": [-3,-6,-9,-12], "mbl": [-1,-2,-3,-4], "noteAgo": 0.3, "offAgo": 0.1},
//                   "ribbonTarget": "fc", "only": ["cv_o1"] }]
// Default output is the canvas content only, composited on the display background (pixel for pixel comparable with tools/display_harness/ref.js).
// --chrome also writes <id>_chrome.png with the border and inset shadow, and a grid sheet _sheet.png per state.
#include <juce_gui_basics/juce_gui_basics.h>
#include "Displays/Common.h"
#include "jpurrr/patch.h"

using namespace jpurrr;

static const char* kIds[] = { "cv_l1", "cv_o1", "cv_o2", "cv_pe", "cv_f", "cv_fe", "cv_ae", "cv_pad", "cv_rib", "cv_gate", "cv_m",
                              "cv_qf", "cv_qf2", "cv_q", "cv_qd", "cv_ql", "cv_qm", "cv_qt", "cv_qr", "cv_qg", "cv_qw", "cv_qp", "cv_qo", "cv_qa", "cv_qv", "cv_qdm" };

static void fillLive(Live& L, const juce::var& lv, double sr) {
    const auto kind = lv.getProperty("kind", "silence").toString();
    const double f = (double) lv.getProperty("freq", 440), amp = (double) lv.getProperty("amp", 0.5);
    auto gain = [&](int tap) { static const double g[kNumTaps] = { 1, 1.0, 0.9, 0.85, 0.8, 0.8, 0.75, 0.7 }; return g[tap]; };
    juce::Random rnd(1234);
    auto sample = [&](int i, double ph, double g) -> float {
        const double t = i / sr;
        if (kind == "sine" || kind == "stereo") return (float) (amp * g * std::sin(2 * juce::MathConstants<double>::pi * f * t + ph));
        if (kind == "saw") { double s = 0; for (int h = 1; h <= 20; ++h) s += std::sin(2 * juce::MathConstants<double>::pi * f * h * t + ph) / h; return (float) (amp * g * s * 0.6); }
        if (kind == "noise") return (float) (amp * g * (rnd.nextFloat() * 2 - 1));
        return 0.0f;
    };
    for (int k = 0; k < kNumTaps; ++k) for (int i = 0; i < TapBuf::kSize; ++i) L.tap[k].s[i] = sample(i, 0, gain(k));
    for (int i = 0; i < TapBuf::kSize; ++i) {
        const double ph2 = kind == "stereo" ? 1.2 : 0.0;
        L.preL.s[i] = sample(i, 0, 1); L.preR.s[i] = sample(i, ph2, kind == "stereo" ? 0.8 : 1);
        L.outL.s[i] = sample(i, 0, 0.7); L.outR.s[i] = sample(i, ph2, kind == "stereo" ? 0.5 : 0.7);
    }
    float pl = 0, pr = 0; for (int i = 0; i < TapBuf::kSize; ++i) { pl = std::max(pl, std::abs(L.outL.s[i])); pr = std::max(pr, std::abs(L.outR.s[i])); }
    L.outPeakL = pl; L.outPeakR = pr;
    L.activeVoices = (int) lv.getProperty("voices", kind == "silence" ? 0 : 1);
    L.gatePhase = (int) lv.getProperty("gatePhase", -1);
    L.gluGR = (float) (double) lv.getProperty("glu", 0); L.optoGR = (float) (double) lv.getProperty("opto", 0);
    for (int i = 0; i < 3; ++i) L.ottGR[i] = (float) (double) lv.getProperty("ott", juce::var()).getArray()->getUnchecked(0) * 0 + (lv.hasProperty("ott") ? (float) (double) (*lv["ott"].getArray())[i] : 0.0f);
    for (int i = 0; i < 4; ++i) {
        L.mbCompGR[i] = lv.hasProperty("mbc") ? (float) (double) (*lv["mbc"].getArray())[i] : 0.0f;
        L.mbLimGR[i] = lv.hasProperty("mbl") ? (float) (double) (*lv["mbl"].getArray())[i] : 0.0f;
    }
}

static juce::Image renderComp(juce::Component& c, juce::Colour bg, bool chrome, int w, int h) {
    juce::Image img(juce::Image::ARGB, (c.getWidth()) * 2, (c.getHeight()) * 2, true);
    juce::Graphics g(img);
    if (!chrome) g.fillAll(bg);
    g.addTransform(juce::AffineTransform::scale(2.0f));
    c.paintEntireComponent(g, false);
    if (chrome) return img;
    const int ox = (c.getWidth() - w) / 2, oy = (c.getHeight() - h) / 2;
    return img.getClippedImage({ ox * 2, oy * 2, w * 2, h * 2 }).createCopy();
}

static void savePng(const juce::Image& img, const juce::File& f) {
    f.deleteFile(); juce::FileOutputStream os(f);
    if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) { std::fprintf(stderr, "usage: jpurrr_displays states.json outdir [--chrome]\n"); return 1; }
    bool chrome = false; for (int i = 3; i < argc; ++i) if (juce::String(argv[i]) == "--chrome") chrome = true;
    const auto states = juce::JSON::parse(juce::File(argv[1]));
    const juce::File outDir(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]));
    if (!states.isArray()) { std::fprintf(stderr, "bad states file\n"); return 1; }

    for (const auto& st : *states.getArray()) {
        const auto name = st.getProperty("name", "state").toString();
        const auto dir = outDir.getChildFile(name); dir.createDirectory();
        int vals[kNumParams]; const Patch& base = factoryPatch(0); for (int i = 0; i < kNumParams; ++i) vals[i] = base.v[i];
        if (auto* o = st["params"].getDynamicObject())
            for (auto& p : o->getProperties()) { const int i = paramIndexById(p.name.toString().toRawUTF8()); if (i < 0) std::fprintf(stderr, "unknown param %s\n", p.name.toString().toRawUTF8()); else vals[i] = (int) p.value; }
        auto live = std::make_unique<Live>();
        const bool hasLive = st.hasProperty("live");
        const double sr = 44100;
        if (hasLive) fillLive(*live, st["live"], sr);
        jp::DisplayEnv env;
        env.get = [&](int i) { return vals[i]; };
        env.set = [&](int i, int v) { vals[i] = juce::jlimit(kParams[i].min, kParams[i].max, v); };
        env.beginGesture = [](int) {}; env.endGesture = [](int) {};
        env.live = hasLive ? live.get() : nullptr; env.sampleRate = sr;
        env.animPhase = (double) st.getProperty("scr", -1.0);
        int ribT = st.hasProperty("ribbonTarget") ? paramIndexById(st["ribbonTarget"].toString().toRawUTF8()) : -1;
        env.ribbonTarget = [ribT] { return ribT; };
        const double T = (double) st.getProperty("t", 1.0);

        std::vector<std::pair<juce::String, std::unique_ptr<jp::Display>>> ds;
        std::vector<juce::String> only; if (auto* a = st["only"].getArray()) for (auto& v : *a) only.push_back(v.toString());
        for (const char* id : kIds) {
            if (!only.empty() && std::find(only.begin(), only.end(), juce::String(id)) == only.end()) continue;
            env.nowSeconds = T - 1.0;
            auto d = jp::makeDisplay(id, env);
            if (!d) { std::fprintf(stderr, "no display for %s\n", id); continue; }
            ds.emplace_back(id, std::move(d));
        }
        const double noteAgo = (double) st.getProperty("live", juce::var()).getProperty("noteAgo", -1.0);
        const double offAgo = (double) st.getProperty("live", juce::var()).getProperty("offAgo", -1.0);
        bool noted = false, offed = false, touched = false;
        for (double tn = T - 0.9; tn <= T + 1e-6; tn += 1.0 / 30) {
            env.nowSeconds = tn;
            if (!touched && tn >= T - 0.4) { touched = true; if (auto* a = st["touch"].getArray()) for (auto& v : *a) jp::noteTouch(paramIndexById(v.toString().toRawUTF8())); }
            if (hasLive && noteAgo >= 0 && !noted && tn >= T - noteAgo) { noted = true; live->noteOnCount++; }
            if (hasLive && offAgo >= 0 && !offed && tn >= T - offAgo) { offed = true; live->noteOffAllCount++; }
            for (auto& d : ds) d.second->tick();
        }
        env.nowSeconds = T;
        std::vector<juce::Image> sheetImgs;
        for (auto& d : ds) {
            auto* cd = dynamic_cast<jp::disp::CanvasDisplay*>(d.second.get());
            jp::disp::CanvasDisplay::setChrome(false);
            savePng(renderComp(*d.second, env.pal.bg, false, cd->cw(), cd->ch()), dir.getChildFile(d.first + ".png"));
            if (chrome) {
                jp::disp::CanvasDisplay::setChrome(true);
                auto im = renderComp(*d.second, env.pal.bg, true, cd->cw(), cd->ch());
                savePng(im, dir.getChildFile(d.first + "_chrome.png")); sheetImgs.push_back(im);
            }
            const auto info = d.second->infoText(0);
            if (info.isNotEmpty()) std::printf("%s/%s info: %s\n", name.toRawUTF8(), d.first.toRawUTF8(), info.toRawUTF8());
        }
        if (chrome && !sheetImgs.empty()) {
            const int maxW = 2400; int x = 10, y = 10, rowH = 0, totW = maxW;
            struct P { int x, y; int i; }; std::vector<P> pos;
            for (int i = 0; i < (int) sheetImgs.size(); ++i) {
                const int w = sheetImgs[(size_t) i].getWidth(), h = sheetImgs[(size_t) i].getHeight();
                if (x + w + 10 > maxW && x > 10) { x = 10; y += rowH + 10; rowH = 0; }
                pos.push_back({ x, y, i }); x += w + 10; rowH = std::max(rowH, h); totW = std::max(totW, x);
            }
            juce::Image sheet(juce::Image::RGB, std::max(totW, 100), y + rowH + 10, true);
            juce::Graphics g(sheet); g.fillAll(juce::Colour(0xff232b58));
            for (auto& p : pos) g.drawImageAt(sheetImgs[(size_t) p.i], p.x, p.y);
            savePng(sheet, dir.getChildFile("_sheet.png"));
        }
    }
    return 0;
}
