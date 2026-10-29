// Headless check of the keyboard strip with the real processor and editor (no window needed). Not built by default:
//   cmake --build <dir> --target jpurrr_stripcheck && <dir>/plugin/jpurrr_stripcheck
#include "PluginEditor.h"
#include "Editor/Fonts.h"
#include "Editor/KeyStrip.h"
#include "jpurrr/patch.h"
#include <juce_events/juce_events.h>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace jpurrr;

static int fails = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; }

// renders n blocks, returns the peak of the last `tail` blocks and the zero crossing rate of the whole run
struct Run { double peak = 0, zcr = 0, f0 = 0; };
static Run render(JPurrr8000AudioProcessor& p, int blocks, int tailBlocks = 0) {
    juce::AudioBuffer<float> buf(2, 256); juce::MidiBuffer midi; Run r; long cross = 0, total = 0; float last = 0; std::vector<float> mono;
    for (int b = 0; b < blocks; ++b) {
        buf.clear(); midi.clear(); p.processBlock(buf, midi);
        const float* l = buf.getReadPointer(0);
        for (int i = 0; i < 256; ++i) { if ((l[i] > 0) != (last > 0)) ++cross; last = l[i]; ++total; }
        if (b >= blocks - 32) mono.insert(mono.end(), l, l + 256);
        if (b >= blocks - juce::jmax(1, tailBlocks)) for (int i = 0; i < 256; ++i) r.peak = std::max(r.peak, (double) std::fabs(l[i]));
    }
    r.zcr = (double) cross / (double) total;
    // pitch of the last 32 blocks by autocorrelation (lags for 60 Hz to 1.6 kHz)
    std::vector<double> ac(800, 0.0); double mx = 0; const int n = (int) mono.size() - 800;
    if (n > 1000) for (int lag = 30; lag < 800; ++lag) { double s = 0, e = 1e-9; for (int i = 0; i < n; ++i) { s += (double) mono[(size_t) i] * mono[(size_t) (i + lag)]; e += (double) mono[(size_t) i] * mono[(size_t) i]; } ac[(size_t) lag] = s / e; mx = std::max(mx, s / e); }
    int bestLag = 0;                                          // first local maximum that is nearly as strong as the strongest (avoids octave errors)
    for (int lag = 31; lag < 799 && !bestLag; ++lag) if (ac[(size_t) lag] >= 0.85 * mx && mx > 0.3 && ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1]) bestLag = lag;
    r.f0 = bestLag ? 48000.0 / bestLag : 0; return r;
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    JPurrr8000AudioProcessor proc; proc.setPlayConfigDetails(0, 2, 48000, 256); proc.prepareToPlay(48000, 256);
    proc.setAll(factoryPatch(0).v);
    proc.setInt(P_pch, 5);                                   // both parts listen to channel 5 only: strip notes must still play
    check(render(proc, 20, 4).peak < 1e-4, "silent before any note");

    proc.stripNoteOn(60);
    check(render(proc, 40, 4).peak > 0.01, "stripNoteOn makes sound (part channel set to 5)");
    proc.stripNoteOff(60);
    const double tail = render(proc, 48000 * 6 / 256, 8).peak;
    check(tail < 1e-3, "stripNoteOff lets it die away");

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* editor = dynamic_cast<JPurrrEditor*>(ed.get()); editor->setScaleWidth(2920);
    auto& root = editor->content(); root.tick(0.0);
    auto* strip = root.keyStrip();
    check(strip != nullptr && strip->isVisible(), "strip is shown (PREVIEW on)");

    strip->noteOn(64);
    check(strip->isLit(64) && render(proc, 40, 4).peak > 0.01, "key lights up and sounds");
    strip->noteOff(64); check(!strip->isLit(64), "key goes dark on release");
    render(proc, 48000 * 6 / 256);

    // computer keys (Z = C3 = 48); in this headless run no key is physically down, so keyStateChanged() sees the key as released
    check(root.keyPressed(juce::KeyPress('z', juce::ModifierKeys(), 'z')) && strip->isLit(48), "computer key Z plays note 48");
    check(root.keyPressed(juce::KeyPress('z', juce::ModifierKeys(), 'z')) && strip->isLit(48), "auto repeat does not retrigger");
    check(!root.keyPressed(juce::KeyPress('a', juce::ModifierKeys(), 'a')), "unmapped key is left alone");
    check(!root.keyPressed(juce::KeyPress('x', juce::ModifierKeys::ctrlModifier, 'x')), "ctrl+key is left alone");
    render(proc, 20); root.keyStateChanged(true);
    check(!strip->isLit(48), "key up releases the note");
    render(proc, 48000 * 6 / 256);
    {   // typing in a text field must not play
        juce::TextEditor te; root.addAndMakeVisible(te); te.grabKeyboardFocus();
        const bool hasFocus = te.hasKeyboardFocus(false);
        const bool played = root.keyPressed(juce::KeyPress('z', juce::ModifierKeys(), 'z'));
        check(!hasFocus || (!played && !strip->isLit(48)), hasFocus ? "typing in a text field does not play" : "typing in a text field does not play (no window focus here, not testable)");
        strip->releaseAll(); root.removeChildComponent(&te);
    }

    // mouse on the keys: press, glissando while dragging, release (points are local to the keys component, which starts at strip x 183)
    {
        auto* keys = strip->findChildWithID("keys"); check(keys != nullptr, "keys component exists");
        auto ev = [&](juce::Component* c, juce::Point<float> p, int clicks = 1) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys(), 1.0f, juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation, 0.0f, 0.0f, c, c, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), clicks, false); };
        keys->mouseDown(ev(keys, { 20, 100 }));
        check(strip->isLit(36), "mouse press on C2 lights it");
        check(render(proc, 40, 4).peak > 0.01, "mouse press sounds");
        keys->mouseDrag(ev(keys, { 115, 132 }));
        check(!strip->isLit(36) && strip->isLit(38), "dragging to D2 moves the note (glissando)");
        keys->mouseDrag(ev(keys, { 5000, 132 }));
        check(strip->isLit(38), "dragging off the keys keeps the last note");
        keys->mouseDrag(ev(keys, { 65, 42 }));
        check(strip->isLit(37) && !strip->isLit(38), "black key C#2 wins where it overlaps the white keys");
        keys->mouseUp(ev(keys, { 65, 42 }));
        check(!strip->isLit(37), "mouse release ends the note");
        render(proc, 48000 * 6 / 256);
    }

    // wheels (arrow keys on the wheel, as the mockup)
    auto* wb = strip->findChildWithID("wBend"); auto* wm = strip->findChildWithID("wMod");
    check(wb != nullptr && wm != nullptr, "BEND and MOD wheels exist");
    check(proc.getInt(P_bend) == 0 && proc.getInt(P_mod) == 0, "wheels rest at BEND 0, MOD 0");
    wm->keyPressed(juce::KeyPress(juce::KeyPress::upKey)); wm->keyPressed(juce::KeyPress(juce::KeyPress::upKey));
    check(proc.getInt(P_mod) == 8, "MOD wheel up twice: 8 of 127 (4 per step)");
    wm->mouseDoubleClick(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), juce::Point<float>(), juce::ModifierKeys(), 1.0f, juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation, 0.0f, 0.0f, wm, wm, juce::Time::getCurrentTime(), juce::Point<float>(), juce::Time::getCurrentTime(), 2, false));
    check(proc.getInt(P_mod) == 0, "MOD wheel double click resets to 0");
    wm->keyPressed(juce::KeyPress(juce::KeyPress::pageUpKey)); check(proc.getInt(P_mod) == 16, "MOD wheel page up: 16");
    wm->keyPressed(juce::KeyPress(juce::KeyPress::homeKey)); check(proc.getInt(P_mod) == 0, "MOD wheel Home: 0");

    // BEND by mouse: drag up a quarter of the travel, release, it springs back
    {
        auto ev = [&](juce::Component* c, juce::Point<float> p) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys(), 1.0f, juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation, 0.0f, 0.0f, c, c, juce::Time::getCurrentTime(), { 20, 64 }, juce::Time::getCurrentTime(), 1, true); };
        wb->mouseDown(ev(wb, { 20, 64 }));
        wb->mouseDrag(ev(wb, { 20, 64 - 25.6f }));
        check(proc.getInt(P_bend) == 32, "BEND drag up 25.6 px (a quarter of the drag range): 32");
        wb->mouseUp(ev(wb, { 20, 38 }));
        strip->animateForTest(300);
        check(proc.getInt(P_bend) == 0, "BEND springs back after the mouse is released");
        wm->mouseDown(ev(wm, { 20, 64 })); wm->mouseDrag(ev(wm, { 20, 64 - 52.0f })); wm->mouseUp(ev(wm, { 20, 12.0f }));
        check(proc.getInt(P_mod) == 64, "MOD drag up 52 px: 64 (0.5 of 127, stays there)");
        strip->animateForTest(200);
        check(proc.getInt(P_mod) == 64, "MOD stays where it was left");
        wm->keyPressed(juce::KeyPress(juce::KeyPress::homeKey));
    }

    // BEND: wheel up to the top, with BEND UP = 12 the note goes up an octave; then it springs back to 0
    proc.setInt(P_bu, 12); proc.setInt(P_bd, -12);
    strip->noteOn(60);
    const Run base = render(proc, 60);
    for (int i = 0; i < 8; ++i) wb->keyPressed(juce::KeyPress(juce::KeyPress::upKey));
    check(proc.getInt(P_bend) == 63, "BEND wheel up: 63");
    const Run up = render(proc, 60);
    std::printf("     pitch at rest %.1f Hz, bend up %.1f Hz, ratio %.2f\n", base.f0, up.f0, up.f0 / base.f0);
    check(base.f0 > 100 && std::fabs(up.f0 / base.f0 - 2.0) < 0.15, "BEND up 12 semitones: one octave higher");
    wb->keyStateChanged(false);                              // no arrow key is down in this headless run: spring back starts
    strip->animateForTest(400);
    check(proc.getInt(P_bend) == 0, "BEND springs back to 0 after release");
    proc.setInt(P_bend, -64);
    const Run dn = render(proc, 60);
    std::printf("     bend down %.1f Hz, ratio %.2f\n", dn.f0, dn.f0 / base.f0);
    check(std::fabs(dn.f0 / base.f0 - 0.5) < 0.08, "BEND down 12 semitones: one octave lower");
    proc.setInt(P_bend, 0);
    strip->noteOff(60); render(proc, 48000 * 6 / 256);

    // EDIT PARTS (mockup v3.5): the PARTS band, the window heights, the key tints and the split marker on the strip
    {
        proc.setInt(P_km, 0); proc.setInt(P_sp, 60); root.tick(0.25);
        check(root.designHeight() == 1105 && !root.partsShown() && !strip->tint.on, "parts: band closed by default, SOUND page with strip 1105 px, plain keys");
        root.setPartsOpen(true); root.tick(0.3);
        check(root.partsShown() && root.designHeight() == 1280, "parts: EDIT PARTS on grows the window by the band (175 px): 1280");
        proc.setInt(P_prev, 0); root.tick(0.31);
        check(root.designHeight() == 1106, "parts: strip off + band open = 1106 on SOUND");
        proc.setInt(P_prev, 1); root.tick(0.32);
        root.showPage(1);
        check(!root.partsShown() && root.designHeight() == 1135 && !strip->tint.on, "parts: FX page shows no band and keeps its own height (1135)");
        root.showPage(0); root.tick(0.33);
        check(root.partsShown() && root.designHeight() == 1280, "parts: the band comes back with the SOUND page (state kept)");
        check(strip->splitHandle().isEmpty() && strip->tint.on && strip->tint.km == 0, "parts: SINGLE has no split marker");
        proc.setInt(P_km, 2); root.tick(0.34);
        check(strip->tint.on && strip->tint.km == 2 && !strip->splitHandle().isEmpty(), "parts: SPLIT shows the split marker");
        auto* sp = strip->findChildWithID("split"); check(sp != nullptr, "parts: split marker component exists");
        if (sp) {
            sp->keyPressed(juce::KeyPress(juce::KeyPress::rightKey)); check(proc.getInt(P_sp) == 61, "parts: arrow right moves the split point one key");
            sp->keyPressed(juce::KeyPress(juce::KeyPress::leftKey)); sp->keyPressed(juce::KeyPress(juce::KeyPress::leftKey)); check(proc.getInt(P_sp) == 59, "parts: arrow left moves it down");
            sp->keyPressed(juce::KeyPress(juce::KeyPress::pageUpKey)); check(proc.getInt(P_sp) == 71, "parts: page up moves it an octave");
            sp->keyPressed(juce::KeyPress(juce::KeyPress::rightKey, juce::ModifierKeys::shiftModifier, 0)); check(proc.getInt(P_sp) == 83, "parts: shift + arrow moves it an octave");
            for (int i = 0; i < 3; ++i) sp->keyPressed(juce::KeyPress(juce::KeyPress::pageUpKey)); check(proc.getInt(P_sp) == 96, "parts: the split point stops at C7 (96)");
            proc.setInt(P_sp, 60);
        }
        proc.setInt(P_km, 1); root.tick(0.35);
        check(strip->tint.km == 1 && strip->splitHandle().isEmpty(), "parts: DUAL has the tint and no handle (the label UPPER + LOWER on every key)");
        root.setPartsOpen(false); root.tick(0.36);
        check(!root.partsShown() && root.designHeight() == 1105 && !strip->tint.on, "parts: closing the band gives the plain strip and 1105 px back");
        proc.setInt(P_km, 0); root.tick(0.37);
    }

    // PREVIEW off while keys are held: everything is released
    strip->noteOn(67); strip->noteOn(71);
    render(proc, 20, 4);
    proc.setInt(P_prev, 0); root.tick(0.1);
    check(!strip->isLit(67) && !strip->isLit(71) && !strip->isVisible(), "PREVIEW off releases held keys and hides the strip");
    check(render(proc, 48000 * 6 / 256, 8).peak < 1e-3, "no stuck notes after PREVIEW off");
    proc.setInt(P_prev, 1); root.tick(0.2);

    // editor closed while keys are held
    strip = root.keyStrip(); strip->noteOn(55); strip->noteOn(59);
    render(proc, 20, 4);
    ed.reset();
    check(render(proc, 48000 * 6 / 256, 8).peak < 1e-3, "no stuck notes after the editor closes");
    ed.reset(); jp::fonts::clearCache();
    std::printf(fails ? "STRIPCHECK FAILED (%d)\n" : "STRIPCHECK OK\n", fails);
    return fails ? 1 : 0;
}
