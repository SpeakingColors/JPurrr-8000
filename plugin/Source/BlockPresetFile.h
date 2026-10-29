// The user's own FX block presets on disk: block_presets.json next to settings.json (Studio 87/JPurrr-8000 in the user data folder).
// One store per process, shared by every plugin window. Message thread only.
#pragma once
#include <juce_core/juce_core.h>
#include "jpurrr/block_presets.h"

class BlockPresetFile {
public:
    static BlockPresetFile& get();                       // loads on first use
    explicit BlockPresetFile(const juce::File& file);    // tests use their own file
    static juce::File defaultFile();

    jpurrr::BlockPresetStore& store() { return store_; }
    const jpurrr::BlockPresetStore& store() const { return store_; }
    // Another plugin window or host process may have written the file: read it again when it changed. Call before a change.
    void refreshIfChanged();
    // Writes the store (temp file, then move). False when the disk write failed.
    bool save();
    bool loadedFromBadFile() const { return keptBad_; }  // the file was unreadable and has been kept as .bad

private:
    void load();
    juce::File file_;
    jpurrr::BlockPresetStore store_;
    juce::Time stamp_;
    bool keptBad_ = false;
};
