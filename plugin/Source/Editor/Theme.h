// Colour looks of the mockup (data-look) and the small drawing helpers shared by all widgets.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Layout.h"
#include "Display.h"

namespace jp {

struct Look {
    juce::Colour c[layout::kNumToks];
    const juce::Colour& operator[](layout::Tok t) const { return c[t]; }
};
const Look& lookByIndex(int i);
int lookIndexOf(const juce::String& id);                 // "jp" "navy" "soda" "purr", unknown = 0
Palette paletteOf(const Look& l);                        // for the displays (CC in the mockup)

// A layout font as a juce::Font (size = CSS px at design scale 1).
juce::Font fontOf(const layout::Font& f);
juce::Font fontOf(layout::Fam fam, float px, int weight, float ls = 0.0f);

float textWidth(const juce::Font& f, const juce::String& s);

// Text at an exact baseline. al: 'l' x = left edge, 'c' x = centre, 'r' x = right edge.
void drawTextAt(juce::Graphics& g, const juce::String& s, float x, float base, char al, bool italic = false);
inline void drawTextAt(juce::Graphics& g, const char* utf8, float x, float base, char al, bool italic = false) { drawTextAt(g, juce::String::fromUTF8(utf8), x, base, al, italic); }

// Mockup widgets painted in code
void drawSection(juce::Graphics& g, const Look& L, const layout::Sec& s);   // a section frame with its title rules (page sections and the PARTS band)
void drawLed(juce::Graphics& g, juce::Rectangle<float> r, bool on, const Look& L);
void drawHeaderButton(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, const Look& L, bool on, bool hover, bool square);
juce::Path catPath();                                    // the cat head of the logo (viewBox 0 0 40 40)
void drawCat(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill);
void drawWood(juce::Graphics& g, juce::Rectangle<float> r, bool left);
// v3.3 keyboard strip (#kbd), drawn in pieces so the live strip can paint keys and wheels in their own components.
// y0 = Page::stripY; all x are window x. lit = 128 flags (pressed keys) or nullptr.
void drawKeyStripBox(juce::Graphics& g, const Look& L, float y0);
// v3.5 key tints while the PARTS band is shown: km = KEY MODE (1 DUAL: both tints, 2 SPLIT: LOWER below sp, UPPER from sp, the split key gets an amber bar).
struct KeyTint { bool on = false; int km = 0, sp = 60; };
struct SplitX { float xt, xb; };                         // split line: top part (the key edge you see) and bottom part (the white key edge), in percent of the key area
SplitX splitX(int sp);
juce::String noteNameOf(int note);                       // 60 = "C4"
void drawSplitLines(juce::Graphics& g, const Look& L, juce::Rectangle<float> box, int sp);       // box = the key area (padding box)
void drawMiniKeys(juce::Graphics& g, const Look& L, juce::Rectangle<float> box, const KeyTint& tint);   // the 61 keys C2..C7 in box (the PARTS map)
// ckBase >= 0 (mockup v3.7): the 18 keys the computer keys play from that note get a blue bar along the bottom (the split key's amber bar wins).
void drawKeyStripKeys(juce::Graphics& g, const Look& L, float y0, const bool* lit, const KeyTint& tint, int ckBase = -1);
void drawKeyStripWheel(juce::Graphics& g, const Look& L, float y0, int idx, float wn, float handleWn, bool dragging, bool focus);
void drawKeyStripCaptions(juce::Graphics& g, const Look& L, float y0);

} // namespace jp
