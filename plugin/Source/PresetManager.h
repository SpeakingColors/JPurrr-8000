// Presets: factory banks (A, from core/factory_gen) and the user's own banks E to G as JSON files.
// Message thread only. Format of the files = the mockup's export: {"app","format","name","type","v":{id:value}} plus
// genres, tags, bank and slot (unknown keys are ignored when reading).
#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include "jpurrr/params_gen.h"

struct PresetEntry {
    juce::String name, type { "Lead" }, bank { "A" }, uid;
    juce::StringArray genres, tags;
    int slot = 1;
    bool factory = false, start = false;
    std::array<int, jpurrr::kNumParams> v {};
};

// The currently loaded sound (mockup `cur`). Lives in the processor so it survives closing the editor.
struct CurrentPreset {
    juce::String name { "Init" }, bank { "A" }, type { "Lead" };
    juce::StringArray genres;                                // shown on the part cards (mockup v3.5)
    int slot = 1;
    bool factory = true, dirty = false;
};

class PresetManager {
public:
    PresetManager();
    static juce::File presetsDir();
    void scanUser();

    int size() const { return (int) list.size(); }
    const PresetEntry& at(int i) const { return list[(size_t) i]; }
    int find(const juce::String& bank, int slot) const;      // -1 when empty
    int indexOfCurrent() const;
    int nextFreeSlot(const juce::String& bank) const;        // 0 when full
    const PresetEntry* factoryAt(int i) const { return i >= 0 && i < nFactory ? &list[(size_t) i] : nullptr; }

    // user presets. Return the index in the list, or -1.
    int saveUser(const PresetEntry& e, const juce::String& overwriteUid);   // bank/slot set by the caller when overwriting, else the next free slot
    bool renameUser(int index, const juce::String& newName);
    bool deleteUser(int index);
    int findUserByName(const juce::String& name, const juce::String& bank) const;

    // text format (export / import / files)
    static juce::String toJson(const PresetEntry& e, bool withPlacement);
    // Returns true and fills out. Out of range values are rejected (counted in skipped), unknown ids ignored.
    static bool fromJson(const juce::String& text, PresetEntry& out, juce::String& err, int* skipped = nullptr);

    CurrentPreset cur;
    juce::StringArray history;                               // "A1 Name" newest first, at most 12
    void pushHistory(const PresetEntry& e);
    std::array<int, jpurrr::kNumParams> defaults() const;

    // Hook for the SysEx / file formats (filled by the import/export helper): see Controller::importFile / exportPatch.
private:
    std::vector<PresetEntry> list;
    int nFactory = 0;
    void sortUser();
};
