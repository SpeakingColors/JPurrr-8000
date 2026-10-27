#include "jpurrr/preset_io.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace jpurrr {
namespace {

constexpr size_t kMaxFile = 64u * 1024u * 1024u;

std::string lowerExt(const std::string& name) {
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string e = name.substr(dot + 1);
    for (char& c : e) c = char(std::tolower((unsigned char)c));
    return e;
}

std::string plural(size_t n, const char* one, const char* many) {
    return std::to_string(n) + " " + (n == 1 ? one : many);
}

}  // namespace

FileKind guessFileKind(const uint8_t* d, size_t n, const std::string& fileName) {
    if (!d || n == 0) return FileKind::Unknown;
    if (n >= 4 && std::memcmp(d, "MThd", 4) == 0) return FileKind::MidiFile;
    if (d[0] == 0xF0) return FileKind::SysEx;
    size_t i = 0;
    if (n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF) i = 3;
    while (i < n && (d[i] == ' ' || d[i] == '\t' || d[i] == '\n' || d[i] == '\r')) ++i;
    if (i < n && d[i] == '{') {
        // A bank has a "patches" list; look in the first 4 KB only (the key sits near the top in our own files).
        std::string head((const char*)d + i, std::min<size_t>(n - i, 4096));
        return head.find("\"patches\"") != std::string::npos ? FileKind::BankJson : FileKind::PatchJson;
    }
    std::string e = lowerExt(fileName);
    if (e == "syx" || e == "sysex") return FileKind::SysEx;
    if (e == "mid" || e == "midi") return FileKind::MidiFile;
    if (e == "json" || e == "jpurrr") return FileKind::PatchJson;
    // Some tools put a few stray bytes in front of the first F0.
    size_t lim = std::min<size_t>(n, 64);
    for (size_t k = 0; k + 1 < lim; ++k) if (d[k] == 0xF0 && d[k + 1] == 0x41) return FileKind::SysEx;
    return FileKind::Unknown;
}

LoadResult loadPresetFile(const uint8_t* data, size_t size, const std::string& fileName) {
    LoadResult r;
    if (!data || size == 0) { r.message = "The file is empty."; return r; }
    if (size > kMaxFile) { r.message = "The file is too large to be a preset file."; return r; }
    r.kind = guessFileKind(data, size, fileName);
    switch (r.kind) {
    case FileKind::PatchJson:
    case FileKind::BankJson: {
        std::string text((const char*)data, size);
        std::string err;
        if (r.kind == FileKind::PatchJson) {
            Patch p;
            if (!patchFromJson(text, p, err)) {
                // A bank that did not look like one in the first 4 KB still loads as a bank.
                std::vector<Patch> many; std::string bn, e2;
                if (bankFromJson(text, many, bn, e2) && !many.empty()) { r.patches = std::move(many); r.bankName = bn; r.kind = FileKind::BankJson; }
                else { r.message = err; return r; }
            } else r.patches.push_back(std::move(p));
        } else {
            if (!bankFromJson(text, r.patches, r.bankName, err)) { r.message = err; return r; }
        }
        r.ok = !r.patches.empty();
        r.message = r.ok ? (r.patches.size() == 1 ? "Loaded preset \"" + r.patches[0].name + "\"." : "Loaded " + plural(r.patches.size(), "preset", "presets") + (r.bankName.empty() ? "" : " from bank \"" + r.bankName + "\"") + ".")
                         : "The bank has no presets.";
        return r;
    }
    case FileKind::SysEx:
    case FileKind::MidiFile: {
        std::vector<uint8_t> raw;
        const uint8_t* bytes = data; size_t n = size;
        if (r.kind == FileKind::MidiFile) {
            std::string err;
            if (!sysexFromMidiFile(data, size, raw, err)) { r.message = "Cannot read this MIDI file: " + err + "."; return r; }
            bytes = raw.data(); n = raw.size();
        }
        SysexImportResult s = sysexToPatches(bytes, n);
        r.warnings = s.warnings;
        if (!s.ok) { r.message = s.error; return r; }
        r.patches = std::move(s.patches);
        for (const auto& i : s.info) r.slotNames.push_back(i.slotName);
        r.ok = true;
        r.message = "Loaded " + plural(r.patches.size(), "patch", "patches") + " from the JP-8000/JP-8080 SysEx data";
        if (s.badChecksum || s.truncated) r.message += " (" + std::to_string(s.badChecksum + s.truncated) + " damaged message(s) skipped)";
        r.message += ".";
        return r;
    }
    case FileKind::Unknown:
        break;
    }
    r.message = "This is not a preset file JPurrr-8000 can read. It opens .json / .jpurrr presets and banks, and JP-8000 / JP-8080 .syx or .mid dumps.";
    return r;
}

std::string savePatchAsJson(const Patch& p) { return patchToJson(p); }
std::string saveBankAsJson(const std::vector<Patch>& patches, const std::string& bankName) { return bankToJson(patches, bankName); }
SysexExportResult savePatchAsSysex(const Patch& p, int deviceId, int index, SysexModel model) { return patchToSysexIndex(p, model, deviceId, index); }
SysexExportResult saveBankAsSysex(const std::vector<Patch>& patches, int deviceId, SysexModel model) { return bankToSysex(patches, model, deviceId, 0); }

std::string safeFileName(const std::string& name, const char* ext) {
    std::string s;
    for (unsigned char c : name) {
        if (c < 0x20 || c == 0x7F || std::strchr("/\\:*?\"<>|", c)) s += '_';
        else s += char(c);
    }
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '.')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '.')) --b;
    s = s.substr(a, b - a);
    if (s.size() > 80) { s.resize(80); while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back(); if (!s.empty() && (unsigned char)s.back() >= 0xC0) s.pop_back(); }
    if (s.empty()) s = "Untitled";
    return s + "." + ext;
}

}  // namespace jpurrr
