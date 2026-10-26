// Roland JP-8000 / JP-8080 SysEx import and export. See sysex.h and docs/midi-and-sysex.md.
// Written from the JP-8080 owner's manual (MIDI Implementation, pp. 191 to 207, "4-3. Patch" on p. 203).
#include "jpurrr/sysex.h"
#include <algorithm>
#include <bitset>
#include <cstdio>
#include <cstring>
#include <map>

namespace jpurrr {
namespace {

constexpr int kCoreEnd = 74;            // name and every sound parameter live below this offset
constexpr int kBlockSize = 256;         // address space per patch slot (2 x 128)

// Field table. offset = the manual's "Offset Address" read as a 7 bit number (01 18 = 152).
// 'L' byte = value - min, 'N' byte = -value, 'P' permuted, 'O' OSC2 range plus fine pair.
const SysexField kFields[] = {
    { "l1w",   16, 1, 3,   'L', "LFO 1 waveform TRI SAW SQR S/H", false, false },
    { "l1r",   17, 1, 127, 'L', "0..127", false, false },
    { "l1f",   18, 1, 127, 'L', "0..127", false, false },
    { "m2r",   19, 1, 127, 'L', "LFO 2 rate 0..127", false, false },
    { "m2s",   20, 1, 2,   'L', "LFO 2 depth select PITCH FILTER AMP", false, false },
    { "ring",  21, 1, 1,   'L', "OFF ON", false, false },
    { "ox",    22, 1, 127, 'L', "X-MOD depth 0..127", false, false },
    { "ob",    23, 1, 127, 'L', "OSC balance -64..+63 as 0..127", false, false },
    { "od",    24, 1, 2,   'L', "LFO 1 and env destination OSC1+2 OSC2 X-MOD", false, false },
    { "ol",    25, 1, 127, 'L', "OSC LFO 1 depth -64..+63 as 0..127", false, false },
    { "m2dp",  26, 1, 127, 'L', "pitch LFO 2 depth -64..+63 as 0..127", false, false },
    { "pd",    27, 1, 127, 'L', "pitch env depth -64..+63 as 0..127", false, false },
    { "pa",    28, 1, 127, 'L', "pitch env attack", false, false },
    { "pdd",   29, 1, 127, 'L', "pitch env decay", false, false },
    { "o1w",   30, 1, 6,   'L', "OSC1 waveform SUPER SAW, TRI MOD, NOISE, FEEDBACK OSC, SQR, SAW, TRI (values 2 and 3 assumed from the panel order)", false, true },
    { "o1c1",  31, 1, 127, 'L', "OSC1 control 1", false, false },
    { "o1c2",  32, 1, 127, 'L', "OSC1 control 2", false, false },
    { "o2w",   33, 1, 3,   'P', "OSC2 waveform: wire PULSE TRI SAW NOISE, ours SQR SAW TRI NOISE (SAW and TRI swap)", false, false },
    { "sync",  34, 1, 1,   'L', "OFF ON", false, false },
    { "o2r",   35, 2, 50,  'O', "range byte 0 = -WIDE, 1..49 = -24..+24, 50 = +WIDE; fine byte 0..100 = -50..+50 cent (WIDE: octaves, approximated)", false, false },
    { "o2c1",  37, 1, 127, 'L', "OSC2 control 1", false, false },
    { "o2c2",  38, 1, 127, 'L', "OSC2 control 2", false, false },
    { "ft",    39, 1, 2,   'L', "filter type HPF BPF LPF", false, false },
    { "f24",   40, 1, 1,   'L', "slope 0 = 12 dB, 1 = 24 dB", false, false },
    { "fc",    41, 1, 127, 'L', "cutoff", false, false },
    { "fr",    42, 1, 127, 'L', "resonance", false, false },
    { "fk",    43, 1, 127, 'L', "key follow -64..+63 as 0..127", false, false },
    { "fl",    44, 1, 127, 'L', "filter LFO 1 depth -64..+63 as 0..127", false, false },
    { "m2df",  45, 1, 127, 'L', "filter LFO 2 depth -64..+63 as 0..127", false, false },
    { "fe",    46, 1, 127, 'L', "filter env depth -64..+63 as 0..127", false, false },
    { "fea",   47, 1, 127, 'L', "filter env attack", false, false },
    { "fed",   48, 1, 127, 'L', "filter env decay", false, false },
    { "fes",   49, 1, 127, 'L', "filter env sustain", false, false },
    { "fer",   50, 1, 127, 'L', "filter env release", false, false },
    { "av",    51, 1, 127, 'L', "amp level", false, false },
    { "al",    52, 1, 127, 'L', "amp LFO 1 depth (pan position in MANUAL pan) -64..+63 as 0..127", false, false },
    { "m2da",  53, 1, 127, 'L', "amp LFO 2 depth -64..+63 as 0..127", false, false },
    { "aea",   54, 1, 127, 'L', "amp env attack", false, false },
    { "aed",   55, 1, 127, 'L', "amp env decay", false, false },
    { "aes",   56, 1, 127, 'L', "amp env sustain", false, false },
    { "aer",   57, 1, 127, 'L', "amp env release", false, false },
    { "ap",    58, 1, 2,   'L', "pan OFF AUTO MANUAL", false, false },
    { "tb",    59, 1, 127, 'L', "tone bass -64..+63 as 0..127", false, false },
    { "tt",    60, 1, 127, 'L', "tone treble -64..+63 as 0..127", false, false },
    { "xt",    61, 1, 12,  'L', "multi-FX type 0..12 (middle values assumed from the panel list order)", false, true },
    { "xl",    62, 1, 127, 'L', "multi-FX level", false, false },
    { "dty",   63, 1, 4,   'L', "delay type 0..4 (middle values assumed from the panel list order)", false, true },
    { "dt",    64, 1, 127, 'L', "delay time", false, false },
    { "dfb",   65, 1, 127, 'L', "delay feedback", false, false },
    { "dl",    66, 1, 127, 'L', "delay level", false, false },
    { "bu",    67, 1, 24,  'L', "bend range up 0..24", false, false },
    { "bd",    68, 1, 24,  'N', "bend range down, byte = magnitude (ours is -24..0)", false, false },
    { "pon",   69, 1, 1,   'L', "portamento OFF ON", false, false },
    { "pt",    70, 1, 127, 'L', "portamento time", false, false },
    { "mono",  71, 1, 1,   'L', "OFF ON", false, false },
    { "leg",   72, 1, 1,   'L', "OFF ON", false, false },
    { "os",    73, 1, 4,   'L', "OSC shift -2..+2 octaves as 0..4", false, false },
    { "mba",  152, 1, 1,   'L', "morph bend assign OFF ON", false, false },
    { "vel",  155, 1, 1,   'L', "velocity switch OFF ON", false, false },
    { "es",   239, 1, 1,   'L', "envelope type in solo STANDARD ANALOG", true, false },
    { "o2ext",241, 1, 1,   'L', "OSC2 external input OFF ON", true, false },
    { "vsend",242, 1, 1,   'L', "voice modulator send OFF ON", true, false },
    { "uni",  243, 1, 1,   'L', "unison OFF ON", true, false },
    { "ud",   244, 1, 50,  'L', "unison detune 0..50 cent", true, false },
    { "pg",   245, 1, 2,   'L', "patch gain 0, +6, +12 dB", true, false },
    { "ets",  246, 1, 1,   'L', "external trigger OFF ON", true, false },
    { "etd",  247, 1, 2,   'L', "external trigger destination FILTER AMP FILTER&AMP", true, false },
};
constexpr int kNumFields = int(sizeof(kFields) / sizeof(kFields[0]));

// OSC2 waveform, ours (SQR SAW TRI NOISE) to wire (PULSE TRI SAW NOISE). Self inverse.
constexpr uint8_t kOsc2Perm[4] = { 0, 2, 1, 3 };

struct FieldIdx { int p[kNumFields]; int o2f, o2wide; };
const FieldIdx& fieldIdx() {
    static const FieldIdx f = [] {
        FieldIdx r{};
        for (int i = 0; i < kNumFields; ++i) r.p[i] = paramIndexById(kFields[i].id);
        r.o2f = paramIndexById("o2f"); r.o2wide = paramIndexById("o2wide");
        return r;
    }();
    return f;
}

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void addWarn(std::vector<std::string>& w, const char* s) {
    for (const auto& x : w) if (x == s) return;
    w.emplace_back(s);
}
const char* kWarnO1 = "OSC1 NOISE and FEEDBACK OSC use the panel order for their SysEx values; the manual's table does not list them. Check on hardware.";
const char* kWarnFx = "Multi-FX type numbering follows the panel list order; the manual's SysEx table abbreviates it. Check on hardware.";
const char* kWarnDly = "Delay type numbering follows the panel list order; the manual's SysEx table abbreviates it. Check on hardware.";
const char* kWarnWide = "OSC2 WIDE range is approximated: the manual does not give the step mapping of FINE/WIDE.";

// Name: 16 characters, ASCII 20h..7Dh, padded with spaces.
void encodeName(const std::string& name, uint8_t* out, std::vector<std::string>& warn) {
    int n = 0; bool cut = false;
    size_t i = 0;
    while (i < name.size()) {
        unsigned char c = (unsigned char)name[i];
        size_t len = c < 0x80 ? 1 : (c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : (c >= 0xC0 ? 2 : 1)));
        if (n >= 16) { cut = true; break; }
        out[n++] = (c >= 0x20 && c <= 0x7D) ? c : '?';
        i += len;
    }
    while (n < 16) out[n++] = ' ';
    if (cut) addWarn(warn, "The name was shortened to 16 characters.");
}

void encodePatch(const Patch& p, uint8_t* out, std::vector<std::string>& warn) {
    std::memset(out, 0, kSysexPatchSize);
    // Morph "Control" and "Velocity" depth words: 2 bytes, neutral = 127 (00 7F). We do not model them.
    auto neutral = [&](int o) { out[o] = 0; out[o + 1] = 127; };
    for (int o = 74; o <= 150; o += 2) neutral(o);
    neutral(153);
    for (int o = 156; o <= 234; o += 2) neutral(o);
    encodeName(p.name, out, warn);
    const FieldIdx& fi = fieldIdx();
    for (int i = 0; i < kNumFields; ++i) {
        const SysexField& f = kFields[i];
        int idx = fi.p[i];
        if (idx < 0) continue;
        int v = clampi(p.v[idx], kParams[idx].min, kParams[idx].max);
        switch (f.conv) {
        case 'L': out[f.offset] = uint8_t(clampi(v - kParams[idx].min, 0, f.wireMax)); break;
        case 'N': out[f.offset] = uint8_t(clampi(-v, 0, f.wireMax)); break;
        case 'P': out[f.offset] = kOsc2Perm[clampi(v, 0, 3)]; break;
        case 'O': {
            int r = p.v[idx];
            int fn = fi.o2f >= 0 ? p.v[fi.o2f] : 0;
            bool wide = fi.o2wide >= 0 && p.v[fi.o2wide] != 0;
            if (!wide) {
                out[35] = uint8_t(clampi(r, -24, 24) + 25);
                out[36] = uint8_t(clampi((fn * 50 + (fn >= 0 ? 32 : -32)) / 64 + 50, 0, 100));
            } else {
                // The mockup moves OSC2 by r * 2 semitones in WIDE, i.e. r / 6 octaves (-4..+4).
                int rr = clampi(r, -24, 24);
                if (rr < 0) { out[35] = 0;  out[36] = uint8_t(clampi(((24 + rr) * 100 + 12) / 24, 0, 100)); }
                else        { out[35] = 50; out[36] = uint8_t(clampi((rr * 100 + 12) / 24, 0, 100)); }
                addWarn(warn, kWarnWide);
            }
            break;
        }
        }
    }
    int o1 = p.v[paramIndexById("o1w")], xt = p.v[paramIndexById("xt")], dty = p.v[paramIndexById("dty")];
    if (o1 == 2 || o1 == 3) addWarn(warn, kWarnO1);
    if (xt > 0 && xt < 12) addWarn(warn, kWarnFx);
    if (dty > 0 && dty < 4) addWarn(warn, kWarnDly);
}

void decodePatch(const uint8_t* d, const std::bitset<kBlockSize>& have, Patch& p, std::vector<std::string>& warn) {
    p.reset();
    char nm[17]; int n = 16;
    for (int i = 0; i < 16; ++i) { unsigned char c = d[i]; nm[i] = (c >= 0x20 && c <= 0x7D) ? char(c) : '?'; }
    while (n > 0 && nm[n - 1] == ' ') --n;
    p.name.assign(nm, size_t(n));
    if (p.name.empty()) p.name = "Untitled";
    p.type = "Lead";
    const FieldIdx& fi = fieldIdx();
    bool clamped = false;
    for (int i = 0; i < kNumFields; ++i) {
        const SysexField& f = kFields[i];
        int idx = fi.p[i];
        if (idx < 0) continue;
        bool covered = have[size_t(f.offset)] && (f.bytes < 2 || have[size_t(f.offset + 1)]);
        if (!covered) continue;
        int b = d[f.offset];
        if (b > f.wireMax) { clamped = true; b = f.wireMax; }
        int v = 0;
        switch (f.conv) {
        case 'L': v = b + kParams[idx].min; break;
        case 'N': v = -b; break;
        case 'P': v = kOsc2Perm[b & 3]; break;
        case 'O': {
            int fine = std::min<int>(d[36], 100);
            if (b >= 1 && b <= 49) {
                p.v[idx] = b - 25;
                if (fi.o2f >= 0) { int num = (fine - 50) * 64; p.v[fi.o2f] = clampi((num >= 0 ? num + 25 : num - 25) / 50, kParams[fi.o2f].min, kParams[fi.o2f].max); }
                if (fi.o2wide >= 0) p.v[fi.o2wide] = 0;
            } else {
                // -WIDE: fine 0..100 = -4..0 octaves; +WIDE: 0..+4 octaves (approximated).
                int r = (b == 0) ? -(24 - (fine * 24 + 50) / 100) : (fine * 24 + 50) / 100;
                p.v[idx] = clampi(r, -24, 24);
                if (fi.o2f >= 0) p.v[fi.o2f] = 0;
                if (fi.o2wide >= 0) p.v[fi.o2wide] = 1;
                addWarn(warn, kWarnWide);
            }
            continue;
        }
        }
        p.v[idx] = clampi(v, kParams[idx].min, kParams[idx].max);
    }
    if (clamped) addWarn(warn, "Some values were outside the range the manual allows and were limited.");
    int o1 = p.v[paramIndexById("o1w")], xt = p.v[paramIndexById("xt")], dty = p.v[paramIndexById("dty")];
    if (o1 == 2 || o1 == 3) addWarn(warn, kWarnO1);
    if (xt > 0 && xt < 12) addWarn(warn, kWarnFx);
    if (dty > 0 && dty < 4) addWarn(warn, kWarnDly);
}

std::string hex(uint32_t a, int digits) {
    char b[16]; std::snprintf(b, sizeof b, "%0*X", digits, (unsigned)a); return b;
}
std::string addrText(uint32_t a) {
    return hex((a >> 21) & 0x7F, 2) + " " + hex((a >> 14) & 0x7F, 2) + " " + hex((a >> 7) & 0x7F, 2) + " " + hex(a & 0x7F, 2);
}

enum class Fail { None, NotRoland, WrongModel, BadChecksum, Malformed, Command };

Fail parseInternal(const uint8_t* m, size_t n, RolandMessage& out, std::string& err, uint8_t* sumGot = nullptr, uint8_t* sumWant = nullptr) {
    if (n < 5 || m[0] != 0xF0 || m[n - 1] != 0xF7) { err = "not a complete SysEx message"; return Fail::Malformed; }
    for (size_t i = 1; i + 1 < n; ++i) if (m[i] & 0x80) { err = "data byte above 7F inside a SysEx message"; return Fail::Malformed; }
    if (m[1] != 0x41) { err = "not a Roland message (manufacturer ID " + hex(m[1], 2) + ")"; return Fail::NotRoland; }
    if (n < 13) { err = "Roland message too short"; return Fail::Malformed; }
    if (m[3] != 0x00 || m[4] != 0x06) { err = "model ID " + hex(m[3], 2) + " " + hex(m[4], 2) + " is not JP-8000/JP-8080 (00 06)"; return Fail::WrongModel; }
    int cmd = m[5];
    if (cmd != 0x12 && cmd != 0x11) { err = "unsupported command " + hex(unsigned(cmd), 2); return Fail::Command; }
    if (cmd == 0x11 && n != 16) { err = "data request has the wrong length"; return Fail::Malformed; }
    uint8_t want = rolandChecksum(m + 6, n - 8);
    if (sumGot) *sumGot = m[n - 2];
    if (sumWant) *sumWant = want;
    if (m[n - 2] != want) { err = "checksum mismatch (got " + hex(m[n - 2], 2) + ", expected " + hex(want, 2) + ")"; return Fail::BadChecksum; }
    out.deviceId = m[2]; out.command = cmd;
    out.address = sysexAddr(m[6], m[7], m[8], m[9]);
    out.data.assign(m + 10, m + n - 2);
    return Fail::None;
}

struct Block {
    SysexArea area = SysexArea::UserPatch;
    int index = 0;
    int dev = 0;
    uint8_t data[kBlockSize] = {};
    std::bitset<kBlockSize> have;
};

// Which patch block does this address belong to? base = packed address of the block start.
bool classify(uint32_t addr, uint32_t& base, SysexArea& area, int& index, int& offset) {
    int a0 = (addr >> 21) & 0x7F, a1 = (addr >> 14) & 0x7F, a2 = (addr >> 7) & 0x7F, a3 = addr & 0x7F;
    offset = ((a2 & 1) << 7) | a3;
    base = sysexAddr(a0, a1, a2 & ~1, 0);
    if (a0 == 2 && a1 <= 1) { area = SysexArea::UserPatch; index = a1 * 64 + (a2 >> 1); return true; }
    if (a0 == 1 && a1 == 0 && a2 >= 0x40 && a2 <= 0x43) { area = a2 < 0x42 ? SysexArea::TempUpper : SysexArea::TempLower; index = 0; return true; }
    if (a0 == 3 && a1 <= 0x3F && a2 >= 0x40 && a2 <= 0x43) { area = a2 < 0x42 ? SysexArea::PerformanceUpper : SysexArea::PerformanceLower; index = a1; return true; }
    return false;
}

std::string slotText(SysexArea a, int index) {
    switch (a) {
    case SysexArea::UserPatch: return userPatchName(index);
    case SysexArea::TempUpper: return "Temp upper";
    case SysexArea::TempLower: return "Temp lower";
    case SysexArea::PerformanceUpper: { char b[40]; std::snprintf(b, sizeof b, "Perf %d%d upper", index / 8 + 1, index % 8 + 1); return b; }
    case SysexArea::PerformanceLower: { char b[40]; std::snprintf(b, sizeof b, "Perf %d%d lower", index / 8 + 1, index % 8 + 1); return b; }
    }
    return "";
}

bool okDevice(int d, std::string& err) {
    if (d < 0x10 || d > 0x1F) { err = "device ID must be 10h to 1Fh (DEVICE ID 17 to 32)"; return false; }
    return true;
}

// One patch as the two DT1 messages the instrument itself sends (242 bytes, then the last 6).
void emitPatch(SysexExportResult& r, int dev, uint32_t base, const uint8_t* data) {
    auto push = [&](uint32_t addr, const uint8_t* d, size_t n) {
        std::vector<uint8_t> m = makeDt1(dev, addr, d, n);
        r.bytes.insert(r.bytes.end(), m.begin(), m.end());
        r.messages.push_back(std::move(m));
    };
    push(base, data, 242);
    push(base + 242, data + 242, kSysexPatchSize - 242);   // +242 = +01 72 in 7 bit units
}
uint32_t userBase(int index) { return sysexAddr(2, index >> 6, (index & 63) * 2, 0); }

bool exportCommon(SysexModel model, int dev, SysexExportResult& r) {
    if (!okDevice(dev, r.error)) return false;
    if (model == SysexModel::JP8000) {
        r.error = "JP-8000 export is not available: the JP-8000 model ID could not be confirmed from a primary source. Export as JP-8080.";
        return false;
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------- helpers
std::string userPatchName(int index) {
    if (index < 0 || index >= kUserPatchCount) return "";
    int n = index & 63;
    char b[8]; std::snprintf(b, sizeof b, "%c%d%d", index >= 64 ? 'B' : 'A', n / 8 + 1, n % 8 + 1);
    return b;
}
bool parseUserPatchName(const std::string& s0, int& index) {
    std::string s = s0;
    if (s.size() >= 2 && (s[0] == 'U' || s[0] == 'u') && s[1] == ':') s = s.substr(2);
    if (s.size() != 3) return false;
    char b = s[0]; if (b >= 'a' && b <= 'b') b = char(b - 32);
    if (b != 'A' && b != 'B') return false;
    if (s[1] < '1' || s[1] > '8' || s[2] < '1' || s[2] > '8') return false;
    index = (b == 'B' ? 64 : 0) + (s[1] - '1') * 8 + (s[2] - '1');
    return true;
}

uint8_t rolandChecksum(const uint8_t* d, size_t n) {
    unsigned sum = 0;
    for (size_t i = 0; i < n; ++i) sum += d[i];
    return uint8_t((128 - (sum % 128)) % 128);
}

std::vector<uint8_t> makeDt1(int dev, uint32_t address, const uint8_t* data, size_t n) {
    std::vector<uint8_t> m;
    m.reserve(n + 13);
    m.insert(m.end(), { 0xF0, 0x41, uint8_t(dev & 0x7F), 0x00, 0x06, 0x12,
                        uint8_t((address >> 21) & 0x7F), uint8_t((address >> 14) & 0x7F), uint8_t((address >> 7) & 0x7F), uint8_t(address & 0x7F) });
    m.insert(m.end(), data, data + n);
    m.push_back(rolandChecksum(m.data() + 6, m.size() - 6));
    m.push_back(0xF7);
    return m;
}
std::vector<uint8_t> makeRq1(int dev, uint32_t address, uint32_t size) {
    std::vector<uint8_t> m = { 0xF0, 0x41, uint8_t(dev & 0x7F), 0x00, 0x06, 0x11,
                               uint8_t((address >> 21) & 0x7F), uint8_t((address >> 14) & 0x7F), uint8_t((address >> 7) & 0x7F), uint8_t(address & 0x7F),
                               uint8_t((size >> 21) & 0x7F), uint8_t((size >> 14) & 0x7F), uint8_t((size >> 7) & 0x7F), uint8_t(size & 0x7F) };
    m.push_back(rolandChecksum(m.data() + 6, m.size() - 6));
    m.push_back(0xF7);
    return m;
}

bool parseRolandMessage(const uint8_t* msg, size_t n, RolandMessage& out, std::string& err) {
    return parseInternal(msg, n, out, err) == Fail::None;
}

std::vector<std::vector<uint8_t>> splitSysexMessages(const uint8_t* b, size_t n, int* truncated) {
    std::vector<std::vector<uint8_t>> out;
    if (truncated) *truncated = 0;
    std::vector<uint8_t> cur; bool open = false;
    for (size_t i = 0; i < n; ++i) {
        uint8_t c = b[i];
        if (c >= 0xF8) continue;                         // realtime bytes may sit anywhere
        if (c == 0xF0) {
            if (open && truncated) ++*truncated;         // previous one never ended
            cur.assign(1, 0xF0); open = true;
        } else if (c == 0xF7) {
            if (open) { cur.push_back(0xF7); out.push_back(std::move(cur)); cur.clear(); open = false; }
        } else if (c & 0x80) {                           // another status byte aborts the message
            if (open && truncated) ++*truncated;
            open = false; cur.clear();
        } else if (open) {
            cur.push_back(c);
        }
    }
    if (open && truncated) ++*truncated;
    return out;
}

const SysexField* sysexFields(int& count) { count = kNumFields; return kFields; }

// ---------------------------------------------------------------- export
SysexExportResult patchToSysexIndex(const Patch& p, SysexModel model, int dev, int index) {
    SysexExportResult r;
    if (!exportCommon(model, dev, r)) return r;
    if (index < 0 || index >= kUserPatchCount) { r.error = "patch slot must be A11 to B88"; return r; }
    uint8_t buf[kSysexPatchSize];
    encodePatch(p, buf, r.warnings);
    emitPatch(r, dev, userBase(index), buf);
    r.ok = true;
    return r;
}
SysexExportResult patchToSysex(const Patch& p, SysexModel model, int dev, int bank, int slot) {
    if (bank < 0 || bank > 1 || slot < 0 || slot > 63) {
        SysexExportResult r; r.error = "bank must be 0 (A) or 1 (B) and slot 0 to 63"; return r;
    }
    return patchToSysexIndex(p, model, dev, bank * 64 + slot);
}
SysexExportResult patchToSysexTemp(const Patch& p, SysexModel model, int dev, bool lower) {
    SysexExportResult r;
    if (!exportCommon(model, dev, r)) return r;
    uint8_t buf[kSysexPatchSize];
    encodePatch(p, buf, r.warnings);
    emitPatch(r, dev, sysexAddr(1, 0, lower ? 0x42 : 0x40, 0), buf);
    r.ok = true;
    return r;
}
SysexExportResult bankToSysex(const std::vector<Patch>& patches, SysexModel model, int dev, int first) {
    SysexExportResult r;
    if (!exportCommon(model, dev, r)) return r;
    if (first < 0 || first >= kUserPatchCount) { r.error = "first slot must be A11 to B88"; return r; }
    if (patches.empty()) { r.error = "no patches to export"; return r; }
    if (int(patches.size()) > kUserPatchCount - first) { r.error = "too many patches: the user memory holds 128 (A11 to B88)"; return r; }
    for (size_t i = 0; i < patches.size(); ++i) {
        uint8_t buf[kSysexPatchSize];
        std::vector<std::string> w;
        encodePatch(patches[i], buf, w);
        for (const auto& s : w) addWarn(r.warnings, s.c_str());
        emitPatch(r, dev, userBase(first + int(i)), buf);
    }
    r.ok = true;
    return r;
}

// ---------------------------------------------------------------- import
SysexImportResult sysexToPatches(const uint8_t* bytes, size_t n) {
    SysexImportResult r;
    auto msgs = splitSysexMessages(bytes, n, &r.truncated);
    r.messages = int(msgs.size());
    if (r.truncated) r.warnings.push_back(std::to_string(r.truncated) + " SysEx message(s) were cut off and skipped.");
    std::map<uint32_t, Block> blocks;
    std::vector<uint32_t> order;
    std::string firstProblem;
    auto problem = [&](const std::string& s) { if (firstProblem.empty()) firstProblem = s; };
    if (r.truncated) problem("The file ends in the middle of a SysEx message (truncated file).");
    int wrongModel = 0, otherMaker = 0;

    for (const auto& m : msgs) {
        RolandMessage rm; std::string err;
        Fail f = parseInternal(m.data(), m.size(), rm, err);
        if (f == Fail::NotRoland) { ++r.ignoredMessages; ++otherMaker; continue; }
        if (f == Fail::WrongModel) { ++r.ignoredMessages; ++wrongModel; continue; }
        if (f == Fail::BadChecksum) {
            ++r.badChecksum;
            std::string at = m.size() >= 10 ? " at address " + addrText(sysexAddr(m[6], m[7], m[8], m[9])) : "";
            r.warnings.push_back("Skipped a message with a " + err + at + ".");
            problem("Checksum error: " + err + at + ".");
            continue;
        }
        if (f != Fail::None) { ++r.ignoredMessages; if (f == Fail::Malformed) problem("Damaged SysEx message: " + err + "."); continue; }
        if (rm.command != 0x12) { ++r.ignoredMessages; continue; }      // data requests carry no sound
        uint32_t base; SysexArea area; int index, offset;
        if (!classify(rm.address, base, area, index, offset)) { ++r.ignoredMessages; continue; }
        if (offset + int(rm.data.size()) > kSysexPatchSize) {
            r.warnings.push_back("Skipped a patch message at " + addrText(rm.address) + " that runs past the end of the patch.");
            problem("A patch message at " + addrText(rm.address) + " is longer than a patch.");
            continue;
        }
        ++r.patchMessages;
        auto it = blocks.find(base);
        if (it == blocks.end()) { it = blocks.emplace(base, Block()).first; order.push_back(base); it->second.area = area; it->second.index = index; it->second.dev = rm.deviceId; }
        Block& b = it->second;
        for (size_t i = 0; i < rm.data.size(); ++i) { b.data[offset + int(i)] = rm.data[i]; b.have.set(size_t(offset) + i); }
    }

    for (uint32_t base : order) {
        const Block& b = blocks[base];
        bool core = true;
        for (int i = 0; i < kCoreEnd; ++i) if (!b.have[size_t(i)]) { core = false; break; }
        std::string slot = slotText(b.area, b.index);
        if (!core) {
            r.warnings.push_back("Patch " + slot + " is incomplete in this file and was skipped.");
            problem("Patch " + slot + " is incomplete (some of its messages are missing or were rejected).");
            continue;
        }
        Patch p;
        decodePatch(b.data, b.have, p, r.warnings);
        SysexPatchInfo info;
        info.area = b.area; info.index = b.index; info.slotName = slot; info.deviceId = b.dev;
        info.hasEightyEightyExtras = b.have[245] && b.have[244] && b.have[243];
        r.patches.push_back(std::move(p));
        r.info.push_back(std::move(info));
    }
    if (wrongModel) r.warnings.push_back(std::to_string(wrongModel) + " message(s) for other Roland models were ignored.");
    r.ok = !r.patches.empty();
    if (!r.ok) {
        if (!firstProblem.empty()) r.error = firstProblem;
        else if (r.messages == 0) r.error = "No SysEx data found in this file.";
        else if (otherMaker == r.messages) r.error = "This SysEx file is not from a Roland JP-8000 or JP-8080.";
        else if (wrongModel) r.error = "This SysEx file is for another Roland model, not the JP-8000 or JP-8080.";
        else r.error = "This SysEx file has no JP-8000 or JP-8080 patch data (it may hold only system, performance or motion data).";
    }
    return r;
}

// ---------------------------------------------------------------- standard MIDI file
bool sysexFromMidiFile(const uint8_t* b, size_t n, std::vector<uint8_t>& out, std::string& err) {
    out.clear();
    auto be32 = [&](size_t p) { return (uint32_t(b[p]) << 24) | (uint32_t(b[p + 1]) << 16) | (uint32_t(b[p + 2]) << 8) | b[p + 3]; };
    if (n < 14 || std::memcmp(b, "MThd", 4) != 0) { err = "not a standard MIDI file"; return false; }
    size_t pos = 8 + be32(4);
    if (be32(4) < 6 || pos > n) { err = "damaged MIDI file header"; return false; }
    while (pos + 8 <= n) {
        uint32_t len = be32(pos + 4);
        bool isTrack = std::memcmp(b + pos, "MTrk", 4) == 0;
        size_t start = pos + 8, end = std::min<size_t>(n, start + len);
        pos = start + len;
        if (!isTrack) continue;
        size_t p = start; int running = 0;
        std::vector<uint8_t> pending; bool open = false;
        auto vlq = [&](uint32_t& v) { v = 0; for (int i = 0; i < 4; ++i) { if (p >= end) return false; uint8_t c = b[p++]; v = (v << 7) | (c & 0x7F); if (!(c & 0x80)) return true; } return false; };
        while (p < end) {
            uint32_t delta; if (!vlq(delta)) break;
            if (p >= end) break;
            int st = b[p];
            if (st < 0x80) { st = running; if (!st) break; } else ++p;
            if (st == 0xFF) {                                        // meta
                if (p >= end) break;
                ++p; uint32_t l; if (!vlq(l)) break; p += l;
            } else if (st == 0xF0 || st == 0xF7) {
                uint32_t l; if (!vlq(l)) break;
                if (p + l > end) break;
                if (st == 0xF0) { pending.assign(1, 0xF0); open = true; }
                if (open) pending.insert(pending.end(), b + p, b + p + l);
                p += l;
                if (open && !pending.empty() && pending.back() == 0xF7) { out.insert(out.end(), pending.begin(), pending.end()); pending.clear(); open = false; }
                running = 0;
            } else {
                running = st;
                int need = ((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0) ? 1 : 2;
                p += size_t(need);
            }
        }
    }
    if (out.empty()) { err = "this MIDI file contains no SysEx data"; return false; }
    return true;
}

}  // namespace jpurrr
