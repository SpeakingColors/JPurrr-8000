#include "Fonts.h"
#include <BinaryData.h>
#include <map>
namespace jp::fonts {
namespace {
std::map<juce::String, juce::Typeface::Ptr>& cacheMap() { static std::map<juce::String, juce::Typeface::Ptr> c; return c; }
juce::Typeface::Ptr load(const juce::String& file) {
    auto& cache = cacheMap();
    auto it = cache.find(file);
    if (it != cache.end()) return it->second;
    juce::Typeface::Ptr tf;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
        const char* orig = BinaryData::originalFilenames[i];
        if (file == orig) {
            int size = 0; const char* data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], size);
            tf = juce::Typeface::createSystemTypefaceFor(data, (size_t) size);
            break;
        }
    }
    cache[file] = tf;
    return tf;
}
juce::Font make(const char* fam, int w, float px, int lo, int mid, int hi) {
    const int ww = w <= lo ? lo : (w >= hi ? hi : mid);
    auto tf = load(juce::String(fam) + "-" + juce::String(ww) + ".ttf");
    if (tf == nullptr) return juce::Font(juce::FontOptions().withHeight(px));
    return juce::Font(juce::FontOptions(tf)).withPointHeight(px);
}
}
void clearCache() { cacheMap().clear(); }
juce::Font body(float px, int w) { return make("Barlow", w, px, 400, 500, 600); }
juce::Font cond(float px, int w) { return make("BarlowCondensed", w, px, 500, 600, 700); }
juce::Font mono(float px, int w) { return make("JetBrainsMono", w, px, 400, 500, 600); }
}
