// What the plugin calls to open and save presets and banks. Pure C++17, no JUCE. Not for the audio thread.
//   load:  file bytes -> patches + a message for the user
//   save:  patch or bank -> bytes for a .json (own format) or .syx (Roland JP-8080) file
#pragma once
#include "jpurrr/patch.h"
#include "jpurrr/sysex.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace jpurrr {

enum class FileKind { Unknown, PatchJson, BankJson, SysEx, MidiFile };

// Looks at the first bytes (and the name's extension when the bytes do not decide). Never reads past size.
FileKind guessFileKind(const uint8_t* data, size_t size, const std::string& fileName = "");

struct LoadResult {
    bool ok = false;
    FileKind kind = FileKind::Unknown;
    std::vector<Patch> patches;              // one for a single preset, many for a bank or a SysEx dump
    std::vector<std::string> slotNames;      // SysEx only: "A11", "B88", "Temp upper" ... (same length as patches)
    std::string bankName;                    // bank files only
    std::string message;                     // for the user: "Loaded 3 presets" or why it failed
    std::vector<std::string> warnings;       // approximations and skipped parts (SysEx), may be empty
};

// Opens .json / .jpurrr (own format), .syx (JP-8000 and JP-8080 dumps) and .mid (SysEx inside a MIDI file).
// Files larger than 64 MB are refused. Garbage in gives ok = false and a message, never a crash.
LoadResult loadPresetFile(const uint8_t* data, size_t size, const std::string& fileName = "");

// Saving. Own format is lossless. SysEx keeps the Roland subset only (see docs/midi-and-sysex.md).
std::string savePatchAsJson(const Patch& p);
std::string saveBankAsJson(const std::vector<Patch>& patches, const std::string& bankName);
SysexExportResult savePatchAsSysex(const Patch& p, int deviceId = 0x10, int index = 0);     // into user slot index (0 = A11)
SysexExportResult saveBankAsSysex(const std::vector<Patch>& patches, int deviceId = 0x10);  // A11 onward, up to 128

// File name helpers: "My Lead" -> "My Lead.json", safe on every OS.
std::string safeFileName(const std::string& presetName, const char* extension /* "json" or "syx" */);
}
