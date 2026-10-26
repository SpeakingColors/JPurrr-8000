// Display registry: id -> class, and the touch broadcast.
#include "Common.h"
#include <algorithm>

namespace jp {
namespace {
std::vector<Display*>& live() { static std::vector<Display*> v; return v; }
}

Display::Display(DisplayEnv& e) : env(e) { live().push_back(this); }
Display::~Display() { auto& v = live(); v.erase(std::remove(v.begin(), v.end(), this), v.end()); }

void noteTouch(int paramIndex) {
    const auto copy = live();   // a touch handler may not add or remove displays, but stay safe
    for (auto* d : copy) d->touched(paramIndex);
}

std::unique_ptr<Display> makeDisplay(const juce::String& id, DisplayEnv& env) {
    if (auto d = disp::makeOscDisplay(id, env)) return d;
    if (auto d = disp::makeFxDisplay(id, env)) return d;
    if (auto d = disp::makeControlDisplay(id, env)) return d;
    return nullptr;
}
}
