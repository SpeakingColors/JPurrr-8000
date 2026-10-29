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
void drawLed(juce::Graphics& g, juce::Rectangle<float> r, bool on, const Look& L);
void drawHeaderButton(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, const Look& L, bool on, bool hover, bool square);
juce::Path catPath();                                    // the cat head of the logo (viewBox 0 0 40 40)
void drawCat(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill);
void drawWood(juce::Graphics& g, juce::Rectangle<float> r, bool left);
// v3.3 keyboard strip (#kbd), drawn in pieces so the live strip can paint keys and wheels in their own components.
// y0 = Page::stripY; all x are window x. lit = 128 flags (pressed keys) or nullptr.
void drawKeyStripBox(juce::Graphics& g, const Look& L, float y0);
void drawKeyStripKeys(juce::Graphics& g, float y0, const bool* lit);
void drawKeyStripWheel(juce::Graphics& g, const Look& L, float y0, int idx, float wn, float handleWn, bool dragging, bool focus);
void drawKeyStripCaptions(juce::Graphics& g, const Look& L, float y0);

} // namespace jp
