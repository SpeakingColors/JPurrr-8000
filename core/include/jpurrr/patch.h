// A patch is the full value set of every saved parameter, plus name and tags. Pure C++17.
#pragma once
#include "jpurrr/params_gen.h"
#include <string>
#include <vector>
namespace jpurrr {
struct Patch {
    std::string name = "Init";
    std::string type;                     // Lead, Bass, Pad, ...
    std::vector<std::string> genres;
    int v[kNumParams];
    Patch();
    void reset();                         // every parameter to its default
    void clampAll();
    bool sameValues(const Patch& o) const; // compares saved (non nosave) parameters only
};

// Factory presets (embedded, generated from data/factory.json by tools/gen_params.py).
int factoryCount();
const Patch& factoryPatch(int index);

// Own preset format (JSON). Same layout the mockup exports: {"app","format","name","type","v":{id:value}}.
std::string patchToJson(const Patch& p);                                   // only non default values
bool patchFromJson(const std::string& text, Patch& out, std::string& err);  // unknown keys ignored, out of range rejected
std::string bankToJson(const std::vector<Patch>& patches, const std::string& bankName);
bool bankFromJson(const std::string& text, std::vector<Patch>& out, std::string& bankName, std::string& err);
}
