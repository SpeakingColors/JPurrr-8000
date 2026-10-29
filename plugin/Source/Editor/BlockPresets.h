// The FX block presets (SAVE, "..." menu, the select): the logic of the mockup's mp* functions, without drawing.
// Factory presets are kMp (Layout.h); your own live in the shared BlockPresetFile.
#pragma once
#include <juce_core/juce_core.h>
#include <string>
#include <vector>
#include "jpurrr/block_presets.h"
#include "Controller.h"

namespace jp {

struct MpChoice { int factory = -1; int user = -1; bool custom() const { return factory < 0 && user < 0; } };

int mpIndexOf(const char* key);                                     // kMp index of a block key, -1 when unknown
std::vector<std::string> mpIds(int mp);                             // parameter ids of the block (kMp)
std::vector<std::string> mpSaveIds(int mp);                         // ids that are saved: the block's own, plus "gpat" for the trance gate
std::vector<std::string> mpFactoryNames(int mp);
const std::vector<jpurrr::BlockPresetEntry>& mpUserList(int mp);   // your presets of the block

MpChoice mpCurrent(Controller& c, int mp);                          // what the select shows: the last pick while it still matches, else the first match
void mpApplyUser(Controller& c, int mp, int userIndex);             // set the block's knobs from one of your presets (like mpApply)

bool mpFactoryNameTaken(int mp, const juce::String& name);          // a factory preset of the block has this name
juce::String mpCleanName(const juce::String& typed);                // trimmed, 40 characters at most; empty becomes "Untitled"
int mpUserIndex(int mp, const juce::String& name);                  // -1 when you have no preset of that name

// Changes of your list. All write the file; false and a message through c.say() when something could not be done.
enum class MpDone { Ok, Full, Failed };
MpDone mpStore(Controller& c, int mp, const juce::String& name, bool overwrite);    // name must be free, or overwrite true
MpDone mpStoreAsNew(Controller& c, int mp, const juce::String& base, juce::String& savedAs);   // "Name 2", "Name 3", ...
bool mpRename(Controller& c, int mp, const juce::String& oldName, const juce::String& newName, juce::String& hint);
bool mpDelete(Controller& c, int mp, const juce::String& name);
// The selected preset of the block if it is one of your own, else says why not (mockup mpOwn). Returns its name or empty.
juce::String mpOwnSelected(Controller& c, int mp);

}  // namespace jp
