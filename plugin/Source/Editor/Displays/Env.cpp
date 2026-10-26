#include "../Display.h"
#include "../Fonts.h"

namespace jp {
juce::Font DisplayEnv::mono(float px) const { return fonts::mono(px, 400); }
juce::Font DisplayEnv::cond(float px, bool bold) const { return fonts::cond(px, bold ? 700 : 600); }
}
