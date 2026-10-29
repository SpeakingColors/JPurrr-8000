// Roland JP-8000 / JP-8080 SysEx: patch and bank import and export. Pure C++17, no JUCE.
// Source: JP-8080 owner's manual, MIDI Implementation (pp. 191 to 207). See docs/midi-and-sysex.md for
// what is verified, what is approximated and what is left out. Not for the audio thread (allocates).
#pragma once
#include "jpurrr/patch.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace jpurrr {

enum class SysexModel { JP8080, JP8000 };

constexpr int kSysexPatchSize = 248;     // "01 78" in 7 bit units, one patch
constexpr int kUserPatchCount = 128;     // U:A11 .. U:B88

// "A11" .. "B88" for index 0 .. 127 (bank A 0..63, B 64..127; digits are row then column, 1..8).
std::string userPatchName(int index);
bool parseUserPatchName(const std::string& name, int& index);   // accepts "A11", "b88", "U:A11"

// Roland checksum: (128 - (sum of address and data bytes mod 128)) mod 128.
uint8_t rolandChecksum(const uint8_t* addressAndData, size_t n);

// The DEVICE ID setting (parameter 'did', 17..32, as the instrument shows it) as the wire value 10h..1Fh. The default is 17 (10h).
constexpr int kSysexDeviceNumberMin = 17, kSysexDeviceNumberMax = 32, kSysexDeviceNumberDefault = 17;
constexpr bool sysexDeviceNumberValid(int number) { return number >= kSysexDeviceNumberMin && number <= kSysexDeviceNumberMax; }
inline int sysexDeviceFromParam(int did) { return did - 1; }

// Pack four 7 bit address bytes into one number and back.
constexpr uint32_t sysexAddr(int a, int b, int c, int d) { return (uint32_t(a) << 21) | (uint32_t(b) << 14) | (uint32_t(c) << 7) | uint32_t(d); }

// Build one DT1 (12h) or RQ1 (11h) message. deviceId is the wire value 10h..1Fh (the DEVICE ID setting minus 1).
std::vector<uint8_t> makeDt1(int deviceId, uint32_t address, const uint8_t* data, size_t n);
std::vector<uint8_t> makeRq1(int deviceId, uint32_t address, uint32_t size);

// A parsed Roland message (model ID 00 06).
struct RolandMessage {
    int deviceId = 0;
    int command = 0;               // 0x12 DT1, 0x11 RQ1
    uint32_t address = 0;          // 7 bit packed
    std::vector<uint8_t> data;     // DT1 data, or the 4 size bytes for RQ1
};
// Checks F0/F7 framing, manufacturer, model ID, checksum. On failure err says why.
bool parseRolandMessage(const uint8_t* msg, size_t n, RolandMessage& out, std::string& err);

// Splits a byte stream (.syx, or concatenated messages) into F0..F7 messages. Realtime bytes inside are dropped.
// Messages that are cut off (no F7) are reported in truncated.
std::vector<std::vector<uint8_t>> splitSysexMessages(const uint8_t* bytes, size_t n, int* truncated = nullptr);

// ---- export ----

struct SysexExportResult {
    bool ok = false;
    std::vector<uint8_t> bytes;                 // ready to save as .syx or to send, DT1 messages back to back
    std::vector<std::vector<uint8_t>> messages; // the same, one entry per message (send with >= 20 ms between them)
    std::string error;
    std::vector<std::string> warnings;          // approximations that apply to this patch (see docs)
};

// Patch into user memory. bank 0 = A, 1 = B; slot 0..63 (11..88 in reading order).
SysexExportResult patchToSysex(const Patch& p, SysexModel model, int deviceId, int bank, int slot);
SysexExportResult patchToSysexIndex(const Patch& p, SysexModel model, int deviceId, int index /*0..127*/);
// Patch into the temporary area of the upper or lower part (plays at once, nothing is stored).
SysexExportResult patchToSysexTemp(const Patch& p, SysexModel model, int deviceId, bool lowerPart);
// Many patches into consecutive user slots starting at firstIndex. At most 128 - firstIndex patches.
SysexExportResult bankToSysex(const std::vector<Patch>& patches, SysexModel model, int deviceId, int firstIndex = 0);

// ---- import ----

enum class SysexArea { UserPatch, TempUpper, TempLower, PerformanceUpper, PerformanceLower };

struct SysexPatchInfo {
    SysexArea area = SysexArea::UserPatch;
    int index = 0;                 // user patch 0..127, or performance number 0..63; 0 for the temporary area
    std::string slotName;          // "A11", "Temp upper", "Perf 11 upper", ...
    int deviceId = 0;
    bool hasEightyEightyExtras = false;   // the JP-8080 only bytes (unison, gain, ...) were present
};

struct SysexImportResult {
    bool ok = false;               // at least one patch was decoded
    std::vector<Patch> patches;
    std::vector<SysexPatchInfo> info;      // same length as patches
    std::string error;             // set when ok is false: the first thing that went wrong
    std::vector<std::string> warnings;
    int messages = 0;              // F0..F7 messages found
    int patchMessages = 0;         // DT1 messages that carried patch data
    int ignoredMessages = 0;       // other Roland data (system, performance, motion), requests, other makers
    int badChecksum = 0;
    int truncated = 0;
};
SysexImportResult sysexToPatches(const uint8_t* bytes, size_t n);
inline SysexImportResult sysexToPatches(const std::vector<uint8_t>& v) { return sysexToPatches(v.data(), v.size()); }

// Standard MIDI file with SysEx events (some librarians store dumps as .mid): returns the SysEx messages in order.
bool sysexFromMidiFile(const uint8_t* bytes, size_t n, std::vector<uint8_t>& sysexBytes, std::string& err);

// ---- the parameter map, public so tests and docs can list it ----
struct SysexField {
    const char* id;                // our parameter id
    int offset;                    // byte offset in the 248 byte patch (7 bit address units)
    int bytes;                     // 1, or 2 for the OSC2 range plus fine pair (offset is the first)
    int wireMax;                   // highest byte value the manual allows (first byte)
    char conv;                     // 'L' linear (byte = value - min), 'N' negated, 'P' permuted, 'O' OSC2 range pair
    const char* note;              // scaling in words
    bool eightyEightyOnly;         // address 01 6F and up
    bool orderAssumed;             // enumeration order taken from the panel list (the SysEx table says "...")
};
const SysexField* sysexFields(int& count);
}
