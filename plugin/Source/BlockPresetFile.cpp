#include "BlockPresetFile.h"

BlockPresetFile& BlockPresetFile::get() { static BlockPresetFile f(defaultFile()); return f; }

juce::File BlockPresetFile::defaultFile() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Studio 87").getChildFile("JPurrr-8000").getChildFile("block_presets.json");
}

BlockPresetFile::BlockPresetFile(const juce::File& file) : file_(file) { load(); }

void BlockPresetFile::load() {
    keptBad_ = false;
    stamp_ = file_.getLastModificationTime();
    if (!file_.existsAsFile()) { store_ = jpurrr::BlockPresetStore(); return; }
    const juce::String text = file_.loadFileAsString();
    jpurrr::BlockPresetStore fresh; std::string err;
    if (fresh.fromJson(text.toStdString(), err)) { store_ = std::move(fresh); return; }
    // not a block preset file: keep it for the user as .bad, start with an empty list, never crash
    const juce::File bad = file_.withFileExtension("json.bad");
    bad.deleteFile();
    keptBad_ = file_.moveFileTo(bad);
    store_ = jpurrr::BlockPresetStore();
    stamp_ = file_.getLastModificationTime();
}

void BlockPresetFile::refreshIfChanged() {
    if (file_.getLastModificationTime() != stamp_) load();
}

bool BlockPresetFile::save() {
    if (!file_.getParentDirectory().createDirectory()) return false;
    juce::TemporaryFile tmp(file_);
    if (!tmp.getFile().replaceWithText(juce::String(store_.toJson()), false, false, "\n")) return false;
    if (!tmp.overwriteTargetFileWithTemporary()) return false;
    stamp_ = file_.getLastModificationTime();
    return true;
}
