// Tests for preset files (JSON), SysEx import/export and the MIDI controller map.
#include "test_util.h"
#include "jpurrr/midi_map.h"
#include "jpurrr/patch.h"
#include "jpurrr/preset_io.h"
#include "jpurrr/sysex.h"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
using namespace jpurrr;

namespace {

std::vector<uint8_t> readFile(const std::string& rel) {
    std::ifstream f(std::string(JPURRR_SOURCE_DIR) + "/tests/data/" + rel, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
std::string readText(const std::string& rel) { auto v = readFile(rel); return std::string(v.begin(), v.end()); }

struct Rng {   // deterministic xorshift
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 2654435761u + 88172645463325252ull) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return uint32_t(s >> 11); }
    int below(int n) { return int(next() % uint32_t(n)); }
};

Patch demoPatch() {
    Patch p; p.name = "Demo \"Quote\" \\ Lead"; p.type = "Pad"; p.genres = { "Trance", "Goa" };
    p.v[P_fc] = 50; p.v[P_fr] = 100; p.v[P_ob] = -20; p.v[P_o1w] = 5; p.v[P_xt] = 12;
    return p;
}
bool inRange(const Patch& p) {
    for (int i = 0; i < kNumParams; ++i) if (p.v[i] < kParams[i].min || p.v[i] > kParams[i].max) return false;
    return true;
}
int v(const Patch& p, const char* id) { return p.v[paramIndexById(id)]; }

// Standard MIDI file with the given SysEx messages in one track.
std::vector<uint8_t> makeMidiFile(const std::vector<uint8_t>& sysex) {
    std::vector<uint8_t> trk;
    auto msgs = splitSysexMessages(sysex.data(), sysex.size());
    for (const auto& m : msgs) {
        trk.push_back(0);                       // delta
        trk.push_back(0xF0);
        size_t len = m.size() - 1;              // data after F0, including F7
        std::vector<uint8_t> vl; vl.push_back(uint8_t(len & 0x7F)); len >>= 7;
        while (len) { vl.push_back(uint8_t(0x80 | (len & 0x7F))); len >>= 7; }
        trk.insert(trk.end(), vl.rbegin(), vl.rend());
        trk.insert(trk.end(), m.begin() + 1, m.end());
    }
    trk.insert(trk.end(), { 0, 0xFF, 0x2F, 0 });
    std::vector<uint8_t> f = { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0, 'M', 'T', 'r', 'k',
                               uint8_t(trk.size() >> 24), uint8_t(trk.size() >> 16), uint8_t(trk.size() >> 8), uint8_t(trk.size()) };
    f.insert(f.end(), trk.begin(), trk.end());
    return f;
}

}  // namespace

// ---------------------------------------------------------------- JSON
TEST(io_json_roundtrip_factory) {
    for (int i = 0; i < factoryCount(); ++i) {
        const Patch& a = factoryPatch(i);
        Patch b; std::string err;
        CHECK(patchFromJson(patchToJson(a), b, err));
        CHECK(err.empty());
        CHECK(a.sameValues(b));
        CHECK(a.name == b.name);
        CHECK(a.type == b.type);
        CHECK(a.genres == b.genres);
    }
    std::vector<Patch> all;
    for (int i = 0; i < factoryCount(); ++i) all.push_back(factoryPatch(i));
    std::vector<Patch> back; std::string name, err;
    CHECK(bankFromJson(bankToJson(all, "Factory test"), back, name, err));
    CHECK(name == "Factory test");
    CHECK(back.size() == all.size());
    for (size_t i = 0; i < back.size() && i < all.size(); ++i) CHECK(back[i].sameValues(all[i]) && back[i].name == all[i].name);
}

TEST(io_json_format_matches_mockup) {
    // JSON.stringify(o, null, 1) of {app, format, name, type, genres, v}; only non default values, no live-only controls.
    Patch p; p.name = "Plain"; p.type = "Bass"; p.v[P_fc] = 50; p.v[P_ob] = -20; p.v[P_bend] = 30;
    const std::string want =
        "{\n"
        " \"app\": \"JPurrr-8000\",\n"
        " \"format\": 1,\n"
        " \"name\": \"Plain\",\n"
        " \"type\": \"Bass\",\n"
        " \"genres\": [],\n"
        " \"v\": {\n"
        "  \"ob\": -20,\n"
        "  \"fc\": 50\n"
        " }\n"
        "}\n";
    CHECK(patchToJson(p) == want);
    Patch init; init.name = "Init"; init.type = "Lead";
    CHECK(patchToJson(init).find("\"v\": {}") != std::string::npos);
    // escaping round trip
    Patch q = demoPatch(), r; std::string err;
    CHECK(patchFromJson(patchToJson(q), r, err));
    CHECK(r.name == q.name);
}

TEST(io_json_reads_mockup_files) {
    Patch p; std::string err;
    CHECK(patchFromJson(readText("mockup_export.json"), p, err));
    CHECK(p.name == "Pasted From Mockup" && p.type == "Pad");
    CHECK(v(p, "fc") == 64 && v(p, "fr") == 90 && v(p, "ob") == -10 && v(p, "xt") == 12 && v(p, "o1w") == 5);
    CHECK(v(p, "bend") == 0);                       // live-only control in the file is ignored
    CHECK(v(p, "fe") == kParams[P_fe].def);         // everything else stays default
    // unknown type falls back to Lead, rounding like Math.round
    CHECK(patchFromJson("{\"app\":\"JPurrr-8000\",\"format\":1,\"name\":\"x\",\"type\":\"Banana\",\"v\":{\"fc\":10.5,\"fr\":3.4}}", p, err));
    CHECK(p.type == "Lead" && v(p, "fc") == 11 && v(p, "fr") == 3);
}

TEST(io_json_rejects_bad_input) {
    Patch p; std::string err;
    CHECK(!patchFromJson("", p, err) && !err.empty());
    CHECK(!patchFromJson("[1,2]", p, err));
    CHECK(!patchFromJson("{\"app\":\"Other\",\"name\":\"x\",\"v\":{}}", p, err));
    CHECK(err.find("not a JPurrr-8000 preset") != std::string::npos);
    CHECK(!patchFromJson("{\"app\":\"JPurrr-8000\",\"format\":1,\"name\":\"x\",\"v\":{\"fc\":300}}", p, err));
    CHECK(err.find("'fc'") != std::string::npos && err.find("0 to 127") != std::string::npos);
    CHECK(!patchFromJson("{\"app\":\"JPurrr-8000\",\"format\":1,\"name\":\"x\",\"v\":{\"fc\":-1}}", p, err));
    CHECK(!patchFromJson("{\"app\":\"JPurrr-8000\",\"format\":1,\"name\":\"x\",\"v\":{\"fc\":\"loud\"}}", p, err));
    CHECK(err.find("not a number") != std::string::npos);
    CHECK(!patchFromJson("{\"app\":\"JPurrr-8000\",\"format\":9,\"name\":\"x\",\"v\":{}}", p, err));
    CHECK(err.find("newer version") != std::string::npos);
    CHECK(!patchFromJson("{\"app\":\"JPurrr-8000\",\"format\":1,\"name\":\"x\",\"v\":{}} trailing", p, err));
    CHECK(!patchFromJson("{\"app\":\"JPurrr-8000\",\"name\":\"x\",\"v\":{\"fc\":1e999}}", p, err));
    CHECK(!patchFromJson("{\"app\":\"JPurrr-8000\",\"name\":\"x\",\"v\":{\"fc\":01}}", p, err));   // leading zero is not JSON
    // a failed read leaves the output alone
    Patch keep = demoPatch(), copy = keep;
    CHECK(!patchFromJson("{nope", keep, err));
    CHECK(keep.sameValues(copy) && keep.name == copy.name);
    // a bank is not a single preset, and one bad entry names its position
    std::vector<Patch> two = { demoPatch(), demoPatch() };
    CHECK(!patchFromJson(bankToJson(two, "B"), p, err) && err.find("bank") != std::string::npos);
    std::string bad = bankToJson(two, "B");
    size_t at = bad.find("\"fc\": 50", bad.find("\"fc\": 50") + 1);
    bad.replace(at, 8, "\"fc\": 500");
    std::vector<Patch> out; std::string bn;
    CHECK(!bankFromJson(bad, out, bn, err) && err.find("Preset 2") != std::string::npos);
}

TEST(io_json_name_hygiene) {
    Patch p; std::string err;
    CHECK(patchFromJson("{\"app\":\"JPurrr-8000\",\"name\":\"  tab\\there\\u0001 \",\"v\":{}}", p, err));
    CHECK(p.name == "tab here");
    CHECK(patchFromJson("{\"app\":\"JPurrr-8000\",\"name\":\"\",\"v\":{}}", p, err) && p.name == "Untitled");
    std::string longName(200, 'a');
    CHECK(patchFromJson("{\"app\":\"JPurrr-8000\",\"name\":\"" + longName + "\",\"v\":{}}", p, err) && p.name.size() == 40);
    CHECK(patchFromJson("{\"app\":\"JPurrr-8000\",\"name\":\"caf\\u00e9 \\ud83d\\ude00\",\"v\":{}}", p, err));
    CHECK(p.name == "caf\xC3\xA9 \xF0\x9F\x98\x80");
    CHECK(patchFromJson("{\"app\":\"JPurrr-8000\",\"name\":\"bad\xFF\xFE\",\"v\":{}}", p, err) && p.name == "bad??");
}

TEST(io_json_fuzz_no_crash) {
    std::string base = patchToJson(demoPatch());
    std::string bankText;
    { std::vector<Patch> some = { demoPatch(), factoryPatch(3), factoryPatch(10) }; bankText = bankToJson(some, "Fuzz"); }
    Rng rng(7);
    int accepted = 0;
    for (int it = 0; it < 6000; ++it) {
        std::string t = (it & 1) ? bankText : base;
        int edits = 1 + rng.below(6);
        for (int e = 0; e < edits; ++e) {
            if (t.empty()) break;
            switch (rng.below(5)) {
            case 0: t[size_t(rng.below(int(t.size())))] = char(rng.below(256)); break;
            case 1: t.resize(size_t(rng.below(int(t.size()) + 1))); break;
            case 2: t.insert(size_t(rng.below(int(t.size()) + 1)), 1, "{}[]\",:0-e\\u"[rng.below(12)]); break;
            case 3: t.erase(size_t(rng.below(int(t.size()))), size_t(1 + rng.below(8))); break;
            default: { size_t a = size_t(rng.below(int(t.size()))); t.insert(a, t.substr(a, 20)); break; }
            }
        }
        Patch p; std::string err;
        if (patchFromJson(t, p, err)) { ++accepted; CHECK(inRange(p)); CHECK(!p.name.empty() && p.name.size() <= 200); }
        else CHECK(!err.empty());
        std::vector<Patch> out; std::string bn;
        if (bankFromJson(t, out, bn, err)) { for (const auto& q : out) CHECK(inRange(q)); }
        else CHECK(!err.empty());
    }
    CHECK(accepted > 0);
    // pure noise and deep nesting
    for (int it = 0; it < 500; ++it) {
        std::string t(size_t(rng.below(300)), ' ');
        for (char& c : t) c = char(rng.below(256));
        Patch p; std::string err; CHECK(!patchFromJson(t, p, err));
    }
    std::string deep(200000, '['); Patch p; std::string err;
    CHECK(!patchFromJson(deep, p, err));
    std::string deepObj; for (int i = 0; i < 5000; ++i) deepObj += "{\"a\":";
    CHECK(!patchFromJson(deepObj, p, err));
}

TEST(io_json_bank_1000_fast) {
    std::vector<Patch> many;
    for (int i = 0; i < 1000; ++i) { Patch p = factoryPatch(i % factoryCount()); p.name += " " + std::to_string(i); many.push_back(p); }
    auto t0 = std::chrono::steady_clock::now();
    std::string text = bankToJson(many, "Big");
    std::vector<Patch> back; std::string bn, err;
    bool ok = bankFromJson(text, back, bn, err);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    CHECK(ok && back.size() == 1000);
    CHECK(ms < 3000);                      // generous: tests run with sanitizers
    std::printf("      1000 presets: write + read %.0f ms, %zu KB\n", ms, text.size() / 1024);
}

// ---------------------------------------------------------------- SysEx
TEST(io_sysex_checksum_manual_examples) {
    // JP-8080 owner's manual p.206 to 207, the three worked examples.
    const uint8_t d1[] = { 0x01, 0x00, 0x10, 0x03, 0x1D };
    CHECK(rolandChecksum(d1, 5) == 0x4F);
    std::vector<uint8_t> m1 = makeDt1(0x10, sysexAddr(1, 0, 0x10, 3), d1 + 4, 1);
    const uint8_t w1[] = { 0xF0, 0x41, 0x10, 0x00, 0x06, 0x12, 0x01, 0x00, 0x10, 0x03, 0x1D, 0x4F, 0xF7 };
    CHECK(m1 == std::vector<uint8_t>(w1, w1 + sizeof w1));
    std::vector<uint8_t> m2 = makeRq1(0x10, sysexAddr(3, 2, 0x42, 0), kSysexPatchSize);
    const uint8_t w2[] = { 0xF0, 0x41, 0x10, 0x00, 0x06, 0x11, 0x03, 0x02, 0x42, 0x00, 0x00, 0x00, 0x01, 0x78, 0x40, 0xF7 };
    CHECK(m2 == std::vector<uint8_t>(w2, w2 + sizeof w2));
    std::vector<uint8_t> m3 = makeRq1(0x10, sysexAddr(1, 0, 0, 0), (0x43 << 7) | 0x78);
    const uint8_t w3[] = { 0xF0, 0x41, 0x10, 0x00, 0x06, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43, 0x78, 0x44, 0xF7 };
    CHECK(m3 == std::vector<uint8_t>(w3, w3 + sizeof w3));
    RolandMessage rm; std::string err;
    CHECK(parseRolandMessage(w1, sizeof w1, rm, err) && rm.command == 0x12 && rm.address == sysexAddr(1, 0, 0x10, 3) && rm.data.size() == 1 && rm.data[0] == 0x1D);
    uint8_t bad[sizeof w1]; std::memcpy(bad, w1, sizeof w1); bad[11] = 0x4E;
    CHECK(!parseRolandMessage(bad, sizeof bad, rm, err) && err.find("checksum") != std::string::npos);
}

TEST(io_sysex_names_and_addresses) {
    CHECK(userPatchName(0) == "A11" && userPatchName(7) == "A18" && userPatchName(8) == "A21" && userPatchName(63) == "A88");
    CHECK(userPatchName(64) == "B11" && userPatchName(127) == "B88" && userPatchName(128).empty());
    int idx = -1;
    CHECK(parseUserPatchName("B88", idx) && idx == 127);
    CHECK(parseUserPatchName("u:a11", idx) && idx == 0);
    CHECK(!parseUserPatchName("A19", idx) && !parseUserPatchName("C11", idx) && !parseUserPatchName("A1", idx));
    // manual p.204: A11 = 02 00 00 00, A12 = 02 00 02 00, A88 = 02 00 7E 00, B11 = 02 01 00 00, B88 = 02 01 7E 00
    auto first4 = [](const SysexExportResult& r) { return std::vector<uint8_t>(r.bytes.begin() + 6, r.bytes.begin() + 10); };
    Patch p;
    auto at = [&](int i) { return first4(patchToSysexIndex(p, SysexModel::JP8080, 0x10, i)); };
    CHECK((at(0) == std::vector<uint8_t>{ 2, 0, 0, 0 }));
    CHECK((at(1) == std::vector<uint8_t>{ 2, 0, 2, 0 }));
    CHECK((at(63) == std::vector<uint8_t>{ 2, 0, 0x7E, 0 }));
    CHECK((at(64) == std::vector<uint8_t>{ 2, 1, 0, 0 }));
    CHECK((at(127) == std::vector<uint8_t>{ 2, 1, 0x7E, 0 }));
    auto t = patchToSysexTemp(p, SysexModel::JP8080, 0x10, false);
    CHECK((first4(t) == std::vector<uint8_t>{ 1, 0, 0x40, 0 }));
    t = patchToSysexTemp(p, SysexModel::JP8080, 0x10, true);
    CHECK((first4(t) == std::vector<uint8_t>{ 1, 0, 0x42, 0 }));
    // like the instrument's own bulk dump: 242 bytes, then the last 6 at +01 72
    auto r = patchToSysexIndex(p, SysexModel::JP8080, 0x10, 0);
    CHECK(r.ok && r.messages.size() == 2 && r.messages[0].size() == 10 + 242 + 2 && r.messages[1].size() == 10 + 6 + 2);
    CHECK((std::vector<uint8_t>(r.messages[1].begin() + 6, r.messages[1].begin() + 10) == std::vector<uint8_t>{ 2, 0, 1, 0x72 }));
}

TEST(io_sysex_export_limits) {
    Patch p;
    CHECK(!patchToSysex(p, SysexModel::JP8080, 0x10, 2, 0).ok);
    CHECK(!patchToSysex(p, SysexModel::JP8080, 0x10, 0, 64).ok);
    auto r = patchToSysex(p, SysexModel::JP8080, 0x20, 0, 0);
    CHECK(!r.ok && r.error.find("device ID") != std::string::npos);
    r = patchToSysex(p, SysexModel::JP8000, 0x10, 0, 0);
    CHECK(!r.ok && r.error.find("JP-8000") != std::string::npos);        // not verified, so not offered
    std::vector<Patch> many(129);
    CHECK(!bankToSysex(many, SysexModel::JP8080, 0x10).ok);
    CHECK(!bankToSysex({}, SysexModel::JP8080, 0x10).ok);
    many.resize(128);
    r = bankToSysex(many, SysexModel::JP8080, 0x10);
    CHECK(r.ok && r.messages.size() == 256);
    p.name = "A very long patch name that will not fit";
    r = patchToSysex(p, SysexModel::JP8080, 0x10, 0, 0);
    CHECK(r.ok && !r.warnings.empty());
    auto imp = sysexToPatches(r.bytes);
    CHECK(imp.ok && imp.patches[0].name == "A very long patc");
}

// A patch with every Roland-mapped value set to something in range, from a seed.
Patch randomMappedPatch(Rng& rng) {
    Patch p;
    int n; const SysexField* f = sysexFields(n);
    for (int i = 0; i < n; ++i) {
        int idx = paramIndexById(f[i].id);
        p.v[idx] = kParams[idx].min + rng.below(kParams[idx].max - kParams[idx].min + 1);
    }
    p.v[paramIndexById("o2wide")] = 0;
    p.name = "Rnd" + std::to_string(rng.below(1000)); p.type = "Lead";
    return p;
}

TEST(io_sysex_field_table_is_consistent) {
    int n; const SysexField* f = sysexFields(n);
    CHECK(n > 60);
    bool seenOffset[256] = {};
    for (int i = 0; i < n; ++i) {
        int idx = paramIndexById(f[i].id);
        CHECK(idx >= 0);
        if (idx < 0) continue;
        CHECK(!(kParams[idx].flags & kNoSave));
        CHECK(f[i].offset >= 16 && f[i].offset + f[i].bytes <= kSysexPatchSize);
        for (int k = 0; k < f[i].bytes; ++k) { CHECK(!seenOffset[f[i].offset + k]); seenOffset[f[i].offset + k] = true; }
        CHECK((f[i].offset >= 239) == f[i].eightyEightyOnly);
        if (f[i].conv == 'L') CHECK(kParams[idx].max - kParams[idx].min == f[i].wireMax);
        if (f[i].conv == 'N') CHECK(-kParams[idx].min == f[i].wireMax && kParams[idx].max == 0);
    }
    CHECK(seenOffset[36]);          // OSC2 fine byte belongs to the pair
}

TEST(io_sysex_hand_vector) {
    auto bytes = readFile("hand_patch.syx");
    CHECK(bytes.size() == 272);
    auto r = sysexToPatches(bytes);
    CHECK(r.ok && r.patches.size() == 1 && r.info.size() == 1);
    CHECK(r.messages == 2 && r.patchMessages == 2 && r.badChecksum == 0 && r.ignoredMessages == 0);
    CHECK(r.info[0].slotName == "B11" && r.info[0].index == 64 && r.info[0].hasEightyEightyExtras);
    const Patch& p = r.patches[0];
    CHECK(p.name == "Hand Test 1");
    struct E { const char* id; int val; };
    const E want[] = {
        { "l1w", 2 }, { "l1r", 90 }, { "l1f", 5 }, { "m2r", 77 }, { "m2s", 1 }, { "ring", 1 }, { "ox", 12 }, { "ob", 36 }, { "od", 1 },
        { "ol", 6 }, { "m2dp", -4 }, { "pd", 16 }, { "pa", 9 }, { "pdd", 33 }, { "o1w", 4 }, { "o1c1", 100 }, { "o1c2", 33 },
        { "o2w", 1 }, { "sync", 1 }, { "o2r", -12 }, { "o2f", 13 }, { "o2wide", 0 }, { "o2c1", 64 }, { "o2c2", 20 }, { "ft", 2 }, { "f24", 1 },
        { "fc", 90 }, { "fr", 44 }, { "fk", 32 }, { "fl", 0 }, { "m2df", 6 }, { "fe", -4 }, { "fea", 1 }, { "fed", 50 }, { "fes", 64 },
        { "fer", 30 }, { "av", 110 }, { "al", 0 }, { "m2da", 0 }, { "aea", 3 }, { "aed", 80 }, { "aes", 100 }, { "aer", 70 }, { "ap", 1 },
        { "tb", 6 }, { "tt", -6 }, { "xt", 12 }, { "xl", 50 }, { "dty", 0 }, { "dt", 70 }, { "dfb", 40 }, { "dl", 22 }, { "bu", 12 },
        { "bd", -12 }, { "pon", 1 }, { "pt", 33 }, { "mono", 1 }, { "leg", 0 }, { "os", 1 }, { "mba", 1 }, { "vel", 1 }, { "es", 1 },
        { "o2ext", 0 }, { "vsend", 1 }, { "uni", 1 }, { "ud", 20 }, { "pg", 2 }, { "ets", 1 }, { "etd", 2 },
    };
    for (const E& e : want) { CHECK(paramIndexById(e.id) >= 0); if (v(p, e.id) != e.val) std::printf("      %s: %d, expected %d\n", e.id, v(p, e.id), e.val); CHECK(v(p, e.id) == e.val); }
    // Parameters the Roland map does not carry stay at their defaults.
    CHECK(v(p, "qon") == kParams[P_qon].def && v(p, "gon") == kParams[P_gon].def && v(p, "vol") == kParams[P_vol].def);
    // Our exporter writes the same bytes the independent script wrote.
    auto out = patchToSysex(p, SysexModel::JP8080, 0x10, 1, 0);
    CHECK(out.ok && out.bytes == bytes);
    CHECK(out.warnings.empty());           // only verified enumeration values in this patch
}

TEST(io_sysex_roundtrip_values) {
    // patch -> syx -> patch gives identical values for everything the Roland map carries (OSC2 fine has 101 steps, so +-1).
    Rng rng(42);
    int n; const SysexField* f = sysexFields(n);
    for (int it = 0; it < 300; ++it) {
        Patch a = (it < factoryCount()) ? factoryPatch(it) : randomMappedPatch(rng);
        a.v[P_o2wide] = 0;
        auto ex = patchToSysexIndex(a, SysexModel::JP8080, 0x10, it % 128);
        CHECK(ex.ok);
        auto im = sysexToPatches(ex.bytes);
        CHECK(im.ok && im.patches.size() == 1);
        if (!im.ok) continue;
        const Patch& b = im.patches[0];
        for (int i = 0; i < n; ++i) {
            int idx = paramIndexById(f[i].id);
            if (f[i].conv == 'O') {
                if (f[i].offset == 35) { CHECK(a.v[idx] == b.v[idx]); CHECK(std::abs(a.v[P_o2f] - b.v[P_o2f]) <= 1 || a.v[P_o2f] >= 62); }
                continue;
            }
            if (a.v[idx] != b.v[idx]) std::printf("      %s: %d -> %d\n", f[i].id, a.v[idx], b.v[idx]);
            CHECK(a.v[idx] == b.v[idx]);
        }
        CHECK(im.info[0].index == it % 128);
        // syx -> patch -> syx -> patch is stable
        auto ex2 = patchToSysexIndex(b, SysexModel::JP8080, 0x10, it % 128);
        auto im2 = sysexToPatches(ex2.bytes);
        CHECK(im2.ok && im2.patches[0].sameValues(b));
    }
}

TEST(io_sysex_bank_roundtrip) {
    std::vector<Patch> bank;
    for (int i = 0; i < 128; ++i) { Patch p = factoryPatch(i % factoryCount()); p.name = "P" + std::to_string(i); bank.push_back(p); }
    auto ex = bankToSysex(bank, SysexModel::JP8080, 0x11);
    CHECK(ex.ok && ex.messages.size() == 256);
    auto im = sysexToPatches(ex.bytes);
    CHECK(im.ok && im.patches.size() == 128 && im.messages == 256 && im.badChecksum == 0);
    for (size_t i = 0; i < im.patches.size() && i < 128; ++i) {
        CHECK(im.patches[i].name == bank[i].name);
        CHECK(im.info[i].index == int(i));
        CHECK(im.info[i].deviceId == 0x11);
    }
    CHECK(im.info.back().slotName == "B88");
    // 40 patches starting at B11
    std::vector<Patch> some(bank.begin(), bank.begin() + 40);
    auto ex2 = bankToSysex(some, SysexModel::JP8080, 0x10, 64);
    auto im2 = sysexToPatches(ex2.bytes);
    CHECK(im2.ok && im2.patches.size() == 40 && im2.info[0].slotName == "B11");
}

TEST(io_sysex_import_edge_cases) {
    Patch p; p.name = "Edge"; p.v[P_fc] = 33;
    auto ex = patchToSysexIndex(p, SysexModel::JP8080, 0x10, 5);
    // Only the first packet (242 bytes): imports, JP-8080 extras stay default.
    {
        auto im = sysexToPatches(ex.messages[0]);
        CHECK(im.ok && im.patches.size() == 1 && v(im.patches[0], "fc") == 33 && !im.info[0].hasEightyEightyExtras);
        CHECK(v(im.patches[0], "pg") == kParams[P_pg].def && v(im.patches[0], "ud") == kParams[P_ud].def);
    }
    // Only the tail packet: incomplete, skipped with a clear message.
    {
        auto im = sysexToPatches(ex.messages[1]);
        CHECK(!im.ok && im.error.find("incomplete") != std::string::npos);
    }
    // Bad checksum on the first packet: nothing usable, error names the checksum.
    {
        std::vector<uint8_t> b = ex.bytes; b[b.size() - 2 - (ex.messages[1].size())] ^= 1;     // checksum byte of message 0
        auto im = sysexToPatches(b);
        CHECK(!im.ok && im.badChecksum == 1 && im.error.find("hecksum") != std::string::npos);
    }
    // Truncated file.
    {
        std::vector<uint8_t> b(ex.bytes.begin(), ex.bytes.begin() + 100);
        auto im = sysexToPatches(b);
        CHECK(!im.ok && im.truncated == 1 && im.error.find("truncated") != std::string::npos);
    }
    // Garbage, empty, other maker, other model, universal identity request.
    {
        const uint8_t junk[] = { 1, 2, 3, 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
        CHECK(!sysexToPatches(junk, sizeof junk).ok);
        CHECK(!sysexToPatches(nullptr, 0).ok && sysexToPatches(nullptr, 0).error.find("No SysEx") != std::string::npos);
        const uint8_t korg[] = { 0xF0, 0x42, 0x30, 0x58, 0x4C, 0xF7 };
        auto im = sysexToPatches(korg, sizeof korg);
        CHECK(!im.ok && im.error.find("not from a Roland") != std::string::npos);
        auto other = makeDt1(0x10, sysexAddr(2, 0, 0, 0), ex.messages[0].data() + 10, 242);
        other[4] = 0x2D;                                   // another Roland model; checksum is not even looked at
        im = sysexToPatches(other);
        CHECK(!im.ok && im.error.find("another Roland model") != std::string::npos);
    }
    // System and performance data in the same stream is ignored, the patch still loads.
    {
        const uint8_t sysData[3] = { 1, 2, 3 };
        std::vector<uint8_t> b = makeDt1(0x10, sysexAddr(0, 0, 0x30, 0), sysData, 3);
        auto rq = makeRq1(0x10, sysexAddr(2, 0, 0, 0), 0x178);
        b.insert(b.end(), rq.begin(), rq.end());
        b.insert(b.end(), ex.bytes.begin(), ex.bytes.end());
        auto im = sysexToPatches(b);
        CHECK(im.ok && im.patches.size() == 1 && im.ignoredMessages == 2);
    }
    // JP-8000 style temporary dump: patch at 01 00 40 00, extension at 01 00 41 72 sent first.
    {
        auto t = patchToSysexTemp(p, SysexModel::JP8080, 0x10, false);
        std::vector<uint8_t> b = t.messages[1];
        b.insert(b.end(), t.messages[0].begin(), t.messages[0].end());
        auto im = sysexToPatches(b);
        CHECK(im.ok && im.info[0].area == SysexArea::TempUpper && im.info[0].slotName == "Temp upper" && v(im.patches[0], "fc") == 33);
        auto tl = patchToSysexTemp(p, SysexModel::JP8080, 0x10, true);
        im = sysexToPatches(tl.bytes);
        CHECK(im.ok && im.info[0].area == SysexArea::TempLower);
    }
    // 239 byte patch block (no JP-8080 bytes) is accepted.
    {
        std::vector<uint8_t> data(ex.messages[0].begin() + 10, ex.messages[0].end() - 2);
        data.resize(239);
        auto m = makeDt1(0x10, sysexAddr(2, 0, 0, 0), data.data(), data.size());
        auto im = sysexToPatches(m);
        CHECK(im.ok && v(im.patches[0], "fc") == 33 && !im.info[0].hasEightyEightyExtras);
    }
    // A patch inside a user performance (03 nn 40 00 / 42 00).
    {
        auto m = makeDt1(0x10, sysexAddr(3, 5, 0x42, 0), ex.messages[0].data() + 10, 242);
        auto m2 = makeDt1(0x10, sysexAddr(3, 5, 0x43, 0x72), ex.messages[1].data() + 10, 6);
        m.insert(m.end(), m2.begin(), m2.end());
        auto im = sysexToPatches(m);
        CHECK(im.ok && im.info[0].area == SysexArea::PerformanceLower && im.info[0].index == 5);
    }
}

TEST(io_sysex_fuzz_no_crash) {
    std::vector<Patch> bank;
    for (int i = 0; i < 12; ++i) bank.push_back(factoryPatch(i));
    auto ex = bankToSysex(bank, SysexModel::JP8080, 0x10);
    Rng rng(99);
    int okCount = 0;
    for (int it = 0; it < 4000; ++it) {
        std::vector<uint8_t> b = ex.bytes;
        int edits = 1 + rng.below(8);
        for (int e = 0; e < edits; ++e) {
            if (b.empty()) break;
            switch (rng.below(5)) {
            case 0: b[size_t(rng.below(int(b.size())))] = uint8_t(rng.below(256)); break;
            case 1: b.resize(size_t(rng.below(int(b.size()) + 1))); break;
            case 2: b.insert(b.begin() + rng.below(int(b.size()) + 1), uint8_t(rng.below(2) ? 0xF0 : 0xF7)); break;
            case 3: { int a = rng.below(int(b.size())); b.erase(b.begin() + a, b.begin() + std::min<int>(int(b.size()), a + 1 + rng.below(40))); break; }
            default: b[size_t(rng.below(int(b.size())))] = uint8_t(rng.below(128)); break;
            }
        }
        auto im = sysexToPatches(b);
        if (im.ok) { ++okCount; for (const auto& p : im.patches) CHECK(inRange(p) && !p.name.empty()); }
        else CHECK(!im.error.empty());
        CHECK(im.patches.size() == im.info.size());
    }
    CHECK(okCount > 0);
    for (int it = 0; it < 500; ++it) {
        std::vector<uint8_t> b(size_t(rng.below(400)));
        for (auto& c : b) c = uint8_t(rng.below(256));
        if (!b.empty() && it % 2) b[0] = 0xF0;
        auto im = sysexToPatches(b);
        CHECK(im.patches.size() == im.info.size());
        std::vector<uint8_t> out; std::string err;
        sysexFromMidiFile(b.data(), b.size(), out, err);
    }
}

TEST(io_sysex_warnings_name_assumptions) {
    Patch p; p.v[P_o1w] = 2; p.v[P_xt] = 5; p.v[P_dty] = 2;
    auto r = patchToSysex(p, SysexModel::JP8080, 0x10, 0, 0);
    CHECK(r.ok && r.warnings.size() == 3);
    Patch q; q.v[P_xt] = 0;                    // verified values only (first and last list entries, ends of the waveform lists)
    r = patchToSysex(q, SysexModel::JP8080, 0x10, 0, 0);
    CHECK(r.ok && r.warnings.empty());
    p.v[P_o2wide] = 1; p.v[P_o2r] = -12;
    r = patchToSysex(p, SysexModel::JP8080, 0x10, 0, 0);
    bool wide = false; for (const auto& w : r.warnings) if (w.find("WIDE") != std::string::npos) wide = true;
    CHECK(wide);
    auto im = sysexToPatches(r.bytes);
    CHECK(im.ok && v(im.patches[0], "o2wide") == 1 && std::abs(v(im.patches[0], "o2r") - (-12)) <= 1);
}

TEST(io_midi_file_sysex) {
    Patch p; p.name = "InMid"; p.v[P_fc] = 44;
    auto ex = patchToSysexIndex(p, SysexModel::JP8080, 0x10, 3);
    auto mid = makeMidiFile(ex.bytes);
    std::vector<uint8_t> out; std::string err;
    CHECK(sysexFromMidiFile(mid.data(), mid.size(), out, err));
    CHECK(out == ex.bytes);
    auto lr = loadPresetFile(mid.data(), mid.size(), "dump.mid");
    CHECK(lr.ok && lr.kind == FileKind::MidiFile && lr.patches.size() == 1 && lr.patches[0].name == "InMid" && lr.slotNames[0] == "A14");
    // a MIDI file with no SysEx
    std::vector<uint8_t> plain = { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 1, 0xE0, 'M', 'T', 'r', 'k', 0, 0, 0, 4, 0, 0xFF, 0x2F, 0 };
    CHECK(!sysexFromMidiFile(plain.data(), plain.size(), out, err));
    CHECK(!loadPresetFile(plain.data(), plain.size()).ok);
}

// ---------------------------------------------------------------- preset_io
TEST(io_file_kinds_and_loading) {
    auto j = patchToJson(demoPatch());
    CHECK(guessFileKind((const uint8_t*)j.data(), j.size()) == FileKind::PatchJson);
    std::vector<Patch> two = { demoPatch(), demoPatch() };
    auto b = bankToJson(two, "Two");
    CHECK(guessFileKind((const uint8_t*)b.data(), b.size()) == FileKind::BankJson);
    auto s = patchToSysex(demoPatch(), SysexModel::JP8080, 0x10, 0, 0);
    CHECK(guessFileKind(s.bytes.data(), s.bytes.size()) == FileKind::SysEx);
    CHECK(guessFileKind(nullptr, 0) == FileKind::Unknown);
    const uint8_t noise[] = { 1, 2, 3, 4 };
    CHECK(guessFileKind(noise, 4, "x.bin") == FileKind::Unknown);
    CHECK(guessFileKind(noise, 4, "x.SYX") == FileKind::SysEx);
    const std::string bom = "\xEF\xBB\xBF" + j;
    CHECK(guessFileKind((const uint8_t*)bom.data(), bom.size()) == FileKind::PatchJson);

    auto lj = loadPresetFile((const uint8_t*)bom.data(), bom.size(), "a.jpurrr");
    CHECK(lj.ok && lj.patches.size() == 1 && lj.patches[0].name == demoPatch().name && lj.message.find("Loaded preset") == 0);
    auto lb = loadPresetFile((const uint8_t*)b.data(), b.size(), "b.json");
    CHECK(lb.ok && lb.kind == FileKind::BankJson && lb.patches.size() == 2 && lb.bankName == "Two");
    auto ls = loadPresetFile(s.bytes.data(), s.bytes.size(), "c.syx");
    CHECK(ls.ok && ls.kind == FileKind::SysEx && ls.patches.size() == 1 && ls.slotNames[0] == "A11");
    // single preset file also opens through the bank reader
    std::vector<Patch> one; std::string bn, err;
    CHECK(bankFromJson(j, one, bn, err) && one.size() == 1);
    // failures come with a message
    for (const char* bad : { "", "hello", "{\"app\":\"JPurrr-8000\"}", "{\"x\":1}" }) {
        auto lr = loadPresetFile((const uint8_t*)bad, std::strlen(bad), "z.json");
        CHECK(!lr.ok && !lr.message.empty());
    }
    auto empty = loadPresetFile(nullptr, 0);
    CHECK(!empty.ok && empty.message.find("empty") != std::string::npos);
    // a file name never leaves the preset name unsafe
    CHECK(safeFileName("A/B:C*?", "json") == "A_B_C__.json");
    CHECK(safeFileName("  ..  ", "syx") == "Untitled.syx");
    CHECK(safeFileName("Lead 1", "syx") == "Lead 1.syx");
}

TEST(io_save_helpers) {
    Patch p = demoPatch();
    CHECK(savePatchAsJson(p) == patchToJson(p));
    auto sx = savePatchAsSysex(p, 0x10, 70);
    CHECK(sx.ok);
    auto im = sysexToPatches(sx.bytes);
    CHECK(im.ok && im.info[0].slotName == "B17");
    std::vector<Patch> two = { p, p };
    CHECK(saveBankAsSysex(two).ok && saveBankAsSysex(two).messages.size() == 4);
    CHECK(saveBankAsJson(two, "x").find("\"bank\": \"x\"") != std::string::npos);
}

// ---------------------------------------------------------------- MIDI map
TEST(io_midi_cc_table) {
    int n; const MidiCcEntry* t = midiCcTable(n);
    CHECK(n >= 38);
    bool seen[128] = {};
    for (int i = 0; i < n; ++i) {
        CHECK(!seen[t[i].cc]); seen[t[i].cc] = true;
        int p = paramIndexById(t[i].paramId);
        CHECK(p >= 0);
        CHECK(midiParamForCc(t[i].cc) == p && midiCcForParam(p) == t[i].cc);
        CHECK(t[i].cc >= 1 && t[i].cc <= 95);            // Tx/Rx settings exist for CC 1..31 and 33..95 only (p.192)
        CHECK(t[i].cc != 32 && t[i].cc != 6 && t[i].cc != 38 && t[i].cc != 64 && t[i].cc != 65 && t[i].cc != 7 && t[i].cc != 10 && t[i].cc != 1);
    }
    // the ones the manual (p.188) names
    CHECK(midiParamForCc(74) == P_fc && midiParamForCc(71) == P_fr && midiParamForCc(73) == P_aea && midiParamForCc(72) == P_aer);
    CHECK(midiParamForCc(75) == P_aed && midiParamForCc(31) == P_aes && midiParamForCc(8) == P_ob && midiParamForCc(5) == P_pt);
    CHECK(midiParamForCc(2) == -1 && midiParamForCc(200) == -1 && midiParamForCc(-1) == -1);
}

TEST(io_midi_cc_scaling) {
    MidiMapper m;
    MidiMapResult r = m.controlChange(74, 100);
    CHECK(r.count == 1 && r.updates[0].param == P_fc && r.updates[0].value == 100);
    r = m.controlChange(8, 0);                                   // bipolar -64..63
    CHECK(r.updates[0].param == P_ob && r.updates[0].value == -64);
    r = m.controlChange(8, 127); CHECK(r.updates[0].value == 63);
    r = m.controlChange(8, 64); CHECK(r.updates[0].value == 0);
    r = m.controlChange(1, 90); CHECK(r.updates[0].param == P_mod && r.updates[0].value == 90);
    r = m.controlChange(7, 127); CHECK(r.updates[0].param == P_vol && r.updates[0].value == 127);
    r = m.controlChange(21, 127); CHECK(r.updates[0].param == P_o2r && r.updates[0].value == 24);
    r = m.controlChange(21, 0); CHECK(r.updates[0].value == -24);
    r = m.controlChange(65, 127); CHECK(r.updates[0].param == P_pon && r.updates[0].value == 1);
    r = m.controlChange(65, 63); CHECK(r.updates[0].value == 0);
    r = m.controlChange(64, 127); CHECK(r.sustain == 1 && r.count == 0);
    r = m.controlChange(64, 0); CHECK(r.sustain == 0);
    r = m.controlChange(123, 0); CHECK(r.allNotesOff && r.count == 0);
    r = m.controlChange(120, 0); CHECK(r.allSoundOff);
    r = m.controlChange(121, 0); CHECK(r.resetControllers);
    r = m.controlChange(126, 1); CHECK(r.allNotesOff && r.updates[0].param == P_mono && r.updates[0].value == 1);
    r = m.controlChange(127, 0); CHECK(r.allNotesOff && r.updates[0].param == P_mono && r.updates[0].value == 0);
    r = m.controlChange(3, 5); CHECK(!r.handled());              // not assigned
    r = m.controlChange(500, 5); CHECK(!r.handled());
    r = m.controlChange(74, 9999); CHECK(r.updates[0].value == 127);
    // pan needs MANUAL pan
    int cur[kNumParams]; for (int i = 0; i < kNumParams; ++i) cur[i] = kParams[i].def;
    CHECK(!m.controlChange(10, 100, cur).handled());
    cur[P_ap] = 2;
    r = m.controlChange(10, 127, cur); CHECK(r.updates[0].param == P_al && r.updates[0].value == 63);
    CHECK(!m.controlChange(10, 100, nullptr).handled());
    // every table entry moves its parameter across the full range
    int n; const MidiCcEntry* t = midiCcTable(n);
    for (int i = 0; i < n; ++i) {
        int p = paramIndexById(t[i].paramId);
        CHECK(midiScaleToParam(p, 0, 7) == kParams[p].min && midiScaleToParam(p, 127, 7) == kParams[p].max);
    }
}

TEST(io_midi_nrpn_and_rpn) {
    MidiMapper m;
    // JPurrr's own NRPN: number = parameter index. 14 bit data entry on cutoff (0..127).
    int p = P_fc;
    m.controlChange(99, p >> 7); auto r = m.controlChange(98, p & 127);
    CHECK(!r.handled());
    r = m.controlChange(6, 64);                                   // MSB only: applied as 7 bit
    CHECK(r.count == 1 && r.updates[0].param == P_fc && r.updates[0].value == 64);
    r = m.controlChange(38, 0);                                   // LSB arrives: now 14 bit
    CHECK(r.count == 1 && r.updates[0].value == midiScaleToParam(P_fc, 64 * 128, 14));
    r = m.controlChange(6, 127); CHECK(r.updates[0].value == midiScaleToParam(P_fc, 127 * 128, 14));
    r = m.controlChange(38, 127); CHECK(r.updates[0].value == 127);
    // a 14 bit value reaches the whole range of a wide parameter (master tune 4275..4529)
    p = P_mtu;
    m.controlChange(99, p >> 7); m.controlChange(98, p & 127);
    m.controlChange(6, 127); r = m.controlChange(38, 127);
    CHECK(r.updates[0].param == P_mtu && r.updates[0].value == 4529);
    m.controlChange(6, 0); r = m.controlChange(38, 0); CHECK(r.updates[0].value == 4275);
    // gate pattern has 65536 values: 14 bit gives 4 per step, still inside the range
    p = P_gpat;
    m.controlChange(99, p >> 7); m.controlChange(98, p & 127);
    m.controlChange(6, 127); r = m.controlChange(38, 127);
    CHECK(r.updates[0].value == 65535);
    // data entry with no parameter selected, or an unassigned NRPN, does nothing
    MidiMapper fresh;
    CHECK(!fresh.controlChange(6, 10).handled() && !fresh.controlChange(38, 10).handled());
    fresh.controlChange(99, 127); fresh.controlChange(98, 127);
    CHECK(!fresh.controlChange(6, 10).handled());
    // momentary buttons have no NRPN
    CHECK(midiNrpnForParam(P_write) == -1 && midiParamForNrpn(P_write) == -1);
    CHECK(midiParamForNrpn(kNumParams) == -1 && midiNrpnForParam(P_fc) == P_fc);

    // RPN 0,0 pitch bend sensitivity moves both bend ranges (manual p.192)
    MidiMapper rp;
    rp.controlChange(101, 0); rp.controlChange(100, 0);
    r = rp.controlChange(6, 12);
    CHECK(r.count == 2 && r.updates[0].param == P_bu && r.updates[0].value == 12 && r.updates[1].param == P_bd && r.updates[1].value == -12);
    r = rp.controlChange(38, 0); CHECK(!r.handled());            // LSB ignored
    r = rp.controlChange(6, 99); CHECK(r.updates[0].value == 24 && r.updates[1].value == -24);
    // RPN 0,2 coarse tune = part transpose
    rp.controlChange(101, 0); rp.controlChange(100, 2);
    r = rp.controlChange(6, 64 + 5); CHECK(r.count == 1 && r.updates[0].param == P_ptr && r.updates[0].value == 5);
    // RPN null ends data entry
    rp.controlChange(101, 127); rp.controlChange(100, 127);
    CHECK(!rp.controlChange(6, 12).handled());
    // Reset all controllers clears the selection
    rp.controlChange(101, 0); rp.controlChange(100, 0); rp.controlChange(121, 0);
    CHECK(!rp.controlChange(6, 12).handled());
    CHECK(midiBendToParam(0) == -64 && midiBendToParam(8192) == 0 && midiBendToParam(16383) == 63);
}
