#include "jpurrr/patch.h"
#include <algorithm>
namespace jpurrr {
Patch::Patch() { reset(); }
void Patch::reset() { for (int i = 0; i < kNumParams; ++i) v[i] = kParams[i].def; }
void Patch::clampAll() { for (int i = 0; i < kNumParams; ++i) v[i] = std::min(kParams[i].max, std::max(kParams[i].min, v[i])); }
bool Patch::sameValues(const Patch& o) const {
    for (int i = 0; i < kNumParams; ++i) if (!(kParams[i].flags & kNoSave) && v[i] != o.v[i]) return false;
    return true;
}
}
