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

// Text at an exact baseline. al: 'l' x = left edge, 'c' x = centre, 'r' x = right edge.
void drawTextAt(juce::Graphics& g, const juce::String& s, float x, float base, char al);

// Mockup widgets painted in code
void drawLed(juce::Graphics& g, juce::Rectangle<float> r, bool on, const Look& L);
void drawHeaderButton(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, const Look& L, bool on, bool hover, bool square);
juce::Path catPath();                                    // the cat head of the logo (viewBox 0 0 40 40)
void drawCat(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill);
void drawWood(juce::Graphics& g, juce::Rectangle<float> r, bool left);

} // namespace jp
