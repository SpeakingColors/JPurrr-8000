// Your own presets for the FX blocks (trance gate, drive, low end, ... multiband). Pure C++17, no JUCE.
// The factory block presets live in the editor layout; this store only holds the user's own.
// File text: {"app":"JPurrr-8000","format":1,"blocks":{"<block key>":[{"n":"name","v":{"<param id>":<int>,..}},..]}}
#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace jpurrr {

struct BlockPresetEntry {
    std::string name;
    std::vector<std::pair<std::string, int>> vals;   // parameter id -> value, in the block's own order
};

class BlockPresetStore {
public:
    static constexpr int kMaxPerBlock = 64;
    static constexpr int kMaxName = 40;
    enum class Result { Ok, EmptyName, Exists, NotFound, Full };

    const std::vector<BlockPresetEntry>& list(const std::string& block) const;
    int find(const std::string& block, const std::string& name) const;        // -1 when missing (names compare without case)
    // Saves under that name. An existing name is replaced only when overwrite is true, else Exists is returned.
    Result save(const std::string& block, const std::string& name, const std::vector<std::pair<std::string, int>>& vals, bool overwrite);
    // A name that is already taken by another entry returns Exists.
    Result rename(const std::string& block, const std::string& oldName, const std::string& newName);
    Result remove(const std::string& block, const std::string& name);
    // Free name for "save as new": "Name 2", "Name 3", ...
    std::string freeName(const std::string& block, const std::string& name) const;

    std::string toJson() const;
    // Replaces the content. Unknown keys are ignored, wrong types skipped, limits applied. False and err set only when the text is not a block preset file.
    bool fromJson(const std::string& text, std::string& err);
    bool empty() const { return blocks_.empty(); }

    static std::string cleanName(const std::string& s);   // trims, limits to kMaxName characters, removes control characters

private:
    std::map<std::string, std::vector<BlockPresetEntry>> blocks_;
};

}  // namespace jpurrr
