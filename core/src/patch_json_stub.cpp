// STUB until the JSON module lands (owner: presets/SysEx). Replaced by patch_json.cpp.
#include "jpurrr/patch.h"
namespace jpurrr {
std::string patchToJson(const Patch&) { return "{}"; }
bool patchFromJson(const std::string&, Patch&, std::string& err) { err = "not implemented"; return false; }
std::string bankToJson(const std::vector<Patch>&, const std::string&) { return "{}"; }
bool bankFromJson(const std::string&, std::vector<Patch>&, std::string&, std::string& err) { err = "not implemented"; return false; }
}
