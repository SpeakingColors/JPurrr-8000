// Tests for the part logic of mockup v3.5 (core/src/parts.cpp) and the engine fed with composed part vectors.
#include "test_util.h"
#include "jpurrr/parts.h"
#include "jpurrr/patch.h"
#include "jpurrr/synth.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
using namespace jpurrr;

TEST(parts_classes) {
    auto cls = [](const char* id) { return partClassOf(paramIndexById(id)); };
    CHECK(cls("km") == PartClass::Perf && cls("ps") == PartClass::Perf && cls("sp") == PartClass::Perf && cls("pdet") == PartClass::Perf);
    CHECK(cls("vc") == PartClass::Glob && cls("vol") == PartClass::Glob && cls("qdv") == PartClass::Glob && cls("gpt") == PartClass::Glob && cls("aon") == PartClass::Glob);
    CHECK(cls("o1w") == PartClass::Part && cls("pch") == PartClass::Part && cls("m2dp") == PartClass::Part);
    CHECK(cls("bend") == PartClass::None && cls("prev") == PartClass::None && cls("m2d") == PartClass::None);
    int n[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < kNumParams; ++i) ++n[(int) partClassOf(i)];
    CHECK(n[(int) PartClass::Perf] == 4 && n[(int) PartClass::Part] > 50 && n[(int) PartClass::Glob] > 50);
    CHECK(n[0] + n[1] + n[2] + n[3] == kNumParams);
    CHECK(partClassOf(-1) == PartClass::None && partClassOf(kNumParams) == PartClass::None);
}

TEST(parts_edit_part) {
    // KEY MODE, PANEL SELECT (LOWER 0, UPPER 1, BOTH 2, VOICE MOD 3)
    CHECK(effectiveEditPart(0, 0, kLower) == kUpper);          // SINGLE: UPPER whatever PANEL SELECT says
    CHECK(effectiveEditPart(1, 0, kUpper) == kLower);
    CHECK(effectiveEditPart(2, 1, kLower) == kUpper);
    CHECK(effectiveEditPart(2, 2, kLower) == kUpper);          // BOTH shows UPPER
    CHECK(effectiveEditPart(2, 3, kLower) == kLower && effectiveEditPart(2, 3, kUpper) == kUpper);   // VOICE MOD keeps the part
    int o[2];
    CHECK(editedParts(0, 2, 0, o) == 1 && o[0] == kUpper);
    CHECK(editedParts(1, 2, 0, o) == 2 && o[0] == kUpper && o[1] == kLower);
    CHECK(editedParts(2, 3, 0, o) == 0);
    CHECK(editedParts(2, 0, 0, o) == 1 && o[0] == kLower);
    CHECK(editedParts(2, 1, 1, o) == 1 && o[0] == kUpper);
}

TEST(parts_compose) {
    int panel[kNumParams]; static int store[2][kNumParams]; static int out[2][kNumParams];
    for (int i = 0; i < kNumParams; ++i) { panel[i] = kParams[i].def; store[0][i] = kParams[i].min; store[1][i] = kParams[i].max; }
    const int o1w = paramIndexById("o1w"), vc = paramIndexById("vc"), km = paramIndexById("km");
    panel[o1w] = 2; panel[vc] = 9; panel[km] = 2;
    composePartVectors(panel, kUpper, store, out);
    CHECK(out[0][o1w] == 2 && out[1][o1w] == kParams[o1w].max);           // the panel part is live, the other part comes from its store
    CHECK(out[0][vc] == 9 && out[1][vc] == 9 && out[0][km] == 2 && out[1][km] == 2);   // GLOB and PERF values always come from the panel
    composePartVectors(panel, kLower, store, out);
    CHECK(out[1][o1w] == 2 && out[0][o1w] == kParams[o1w].min);
    int copy[kNumParams] = {}; copyPartValues(panel, copy);
    CHECK(copy[o1w] == 2 && copy[vc] == 0 && copy[km] == 0);
}

namespace {
float renderPeak(Synth& s, int n) {
    std::vector<float> l(512), r(512); float pk = 0.f; int done = 0;
    while (done < n) { const int m = std::min(512, n - done); s.process(l.data(), r.data(), m); for (int i = 0; i < m; ++i) pk = std::max(pk, std::max(std::fabs(l[(size_t) i]), std::fabs(r[(size_t) i]))); done += m; }
    return pk;
}
}

TEST(parts_engine_split_dual) {
    // The engine fed the way the plugin does: UPPER loud, LOWER silent (amp level 0), distinct sounds per part.
    static int panel[kNumParams], store[2][kNumParams], pv[2][kNumParams];
    std::memcpy(panel, factoryPatch(0).v, sizeof panel);
    std::memcpy(store[0], panel, sizeof panel); std::memcpy(store[1], panel, sizeof panel);
    store[1][P_av] = 0;
    panel[P_km] = 2; panel[P_sp] = 60;
    Synth s; s.prepare(48000.0, 512);
    auto feed = [&](int editPart) { composePartVectors(panel, editPart, store, pv); s.setParams(pv[0]); s.setPartParams(0, pv[0]); s.setPartParams(1, pv[1]); };
    feed(kUpper);
    s.noteOn(0, 48, 100); const float low = renderPeak(s, 9600);        // below the split: LOWER, silent
    s.noteOn(0, 72, 100); const float high = renderPeak(s, 9600);       // from the split up: UPPER
    CHECK(low < 1e-4f && high > 0.02f);
    s.allNotesOff(); renderPeak(s, 48000);
    // the panel edits LOWER now (its values are live on the panel, UPPER comes from the store): the same result
    std::memcpy(store[0], factoryPatch(0).v, sizeof panel);
    std::memcpy(panel, store[1], sizeof panel); panel[P_km] = 2; panel[P_sp] = 60; panel[P_ps] = 0;
    feed(kLower);
    s.noteOn(0, 48, 100); const float low2 = renderPeak(s, 9600);
    s.noteOn(0, 72, 100); const float high2 = renderPeak(s, 9600);
    CHECK(low2 < 1e-4f && high2 > 0.02f);
    // DUAL: both parts sound on every key
    s.allNotesOff(); renderPeak(s, 48000);
    panel[P_km] = 1; feed(kLower);
    s.noteOn(0, 48, 100); renderPeak(s, 2400);
    CHECK(s.heldVoices() == 2);
    // moving the split point moves which part answers
    s.allNotesOff(); renderPeak(s, 48000);
    panel[P_km] = 2; panel[P_sp] = 40; feed(kLower);
    s.noteOn(0, 48, 100); const float above = renderPeak(s, 9600);      // now above the split: UPPER
    CHECK(above > 0.02f);
}
