// Embedded fonts of the mockup (SIL OFL, see licenses/): Barlow, Barlow Condensed, JetBrains Mono.
// Sizes are CSS px of the mockup (em size at scale 1), converted with Font::withPointHeight.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace jp::fonts {
juce::Font body(float px, int weight = 400);   // Barlow 400, 500, 600
juce::Font cond(float px, int weight = 600);   // Barlow Condensed 500, 600, 700
juce::Font mono(float px, int weight = 400);   // JetBrains Mono 400, 500, 600
}
