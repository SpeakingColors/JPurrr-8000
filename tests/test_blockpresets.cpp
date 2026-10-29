// Tests for the user's own FX block presets.
#include "test_util.h"
#include "jpurrr/block_presets.h"
#include "jpurrr/params_gen.h"
#include <map>
#include <string>
using namespace jpurrr;

namespace {
using Vals = std::vector<std::pair<std::string, int>>;
const Vals kA = { { "gpat", 21845 }, { "gpt", 3 } };
}

TEST(bp_save_overwrite_rename_delete) {
    BlockPresetStore s;
    CHECK(s.save("gate", "Rolling", kA, false) == BlockPresetStore::Result::Ok);
    CHECK(s.save("gate", "rolling", kA, false) == BlockPresetStore::Result::Exists);
    Vals b = { { "gpat", 1 } };
    CHECK(s.save("gate", "ROLLING", b, true) == BlockPresetStore::Result::Ok);
    CHECK(s.list("gate").size() == 1 && s.list("gate")[0].name == "ROLLING" && s.list("gate")[0].vals == b);
    CHECK(s.save("gate", "  ", kA, false) == BlockPresetStore::Result::EmptyName);
    CHECK(s.save("gate", "Two", kA, false) == BlockPresetStore::Result::Ok);
    CHECK(s.rename("gate", "Two", "ROLLING") == BlockPresetStore::Result::Exists);
    CHECK(s.rename("gate", "Two", "Three") == BlockPresetStore::Result::Ok);
    CHECK(s.rename("gate", "Nope", "x") == BlockPresetStore::Result::NotFound);
    CHECK(s.find("gate", "three") == 1);
    CHECK(s.freeName("gate", "Three") == "Three 2");
    CHECK(s.remove("gate", "Three") == BlockPresetStore::Result::Ok);
    CHECK(s.remove("gate", "Three") == BlockPresetStore::Result::NotFound);
    CHECK(s.remove("gate", "ROLLING") == BlockPresetStore::Result::Ok);
    CHECK(s.empty());
}

TEST(bp_limits_and_name_hygiene) {
    BlockPresetStore s;
    CHECK(BlockPresetStore::cleanName("  a\tb  ") == "a b");
    CHECK(BlockPresetStore::cleanName(std::string(100, 'x')).size() == 40);
    for (int i = 0; i < BlockPresetStore::kMaxPerBlock; ++i)
        CHECK(s.save("drive", "p" + std::to_string(i), kA, false) == BlockPresetStore::Result::Ok);
    CHECK(s.save("drive", "one more", kA, false) == BlockPresetStore::Result::Full);
    CHECK(s.save("drive", "p3", kA, true) == BlockPresetStore::Result::Ok);   // overwrite still works when full
}

TEST(bp_json_roundtrip_and_bad_input) {
    BlockPresetStore s;
    s.save("gate", "Rolling \"16th\"", kA, false);
    s.save("gate", "Ünï", kA, false);
    s.save("ott", "Big", { { "ott_d", 80 } }, false);
    const std::string j = s.toJson();
    BlockPresetStore t; std::string err;
    CHECK(t.fromJson(j, err));
    CHECK(t.toJson() == j);
    CHECK(t.list("gate").size() == 2 && t.list("ott").size() == 1);
    CHECK(t.list("gate")[0].name == "Rolling \"16th\"" && t.list("gate")[0].vals == kA);
    CHECK(!t.fromJson("", err) && !err.empty());
    CHECK(!t.fromJson("{\"app\":\"Other\"}", err));
    CHECK(!t.fromJson("[1]", err));
    CHECK(t.list("gate").size() == 2);            // a failed read keeps the old content
    CHECK(t.fromJson("{\"app\":\"JPurrr-8000\",\"format\":1,\"blocks\":{\"gate\":[{\"n\":\"ok\",\"v\":{\"a\":1,\"b\":\"x\"}},{\"n\":5},{\"n\":\"OK\",\"v\":{}},\"junk\"],\"x\":3}}", err));
    CHECK(t.list("gate").size() == 1 && t.list("gate")[0].vals.size() == 1 && t.list("x").empty());
    CHECK(t.fromJson("{\"app\":\"JPurrr-8000\",\"format\":1}", err) && t.empty());
    BlockPresetStore e;
    CHECK(e.fromJson(e.toJson(), err) && e.empty());
}

TEST(bp_values_and_matching) {
    const std::vector<std::string> ids = { "grate", "gdep", "gsm", "gdc", "gsw", "gpt" };
    BlockPresetEntry e; e.name = "x"; e.vals = { { "grate", 2 }, { "gpt", 3 }, { "gdep", 99999 }, { "nope", 1 } };
    const auto v = blockPresetValues(e, ids);
    CHECK(v.size() == 6 && v[0] == 2 && v[5] == 3);
    CHECK(v[1] == kParams[paramIndexById("gdep")].def);              // out of range takes the default
    CHECK(v[2] == kParams[paramIndexById("gsm")].def);               // missing takes the default
    std::map<std::string, int> cur;
    for (size_t i = 0; i < ids.size(); ++i) cur[ids[i]] = v[i];
    cur["gpat"] = 1234;
    auto get = [&](const std::string& id) { return cur[id]; };
    CHECK(blockPresetMatches(e, ids, get));                          // built in pattern: gpat is not compared
    cur["gdc"] += 1;
    CHECK(!blockPresetMatches(e, ids, get));
    cur["gdc"] -= 1;
    e.vals = { { "gpt", 0 }, { "gpat", 21845 } };                    // own pattern: gpat has to match
    cur["gpt"] = 0; cur["grate"] = kParams[paramIndexById("grate")].def; cur["gpat"] = 21845;
    for (auto id : { "gdep", "gsm", "gdc", "gsw" }) cur[id] = kParams[paramIndexById(id)].def;
    CHECK(blockPresetMatches(e, ids, get));
    cur["gpat"] = 21844;
    CHECK(!blockPresetMatches(e, ids, get));
    // a block without gpt never looks at gpat
    BlockPresetEntry d; d.name = "d"; d.vals = { { "qdv", 12 } };
    const std::vector<std::string> dids = { "qdv", "qdx" };
    cur["qdv"] = 12; cur["qdx"] = kParams[paramIndexById("qdx")].def;
    CHECK(blockPresetMatches(d, dids, get));
}

TEST(bp_free_name_skips_factory_names) {
    BlockPresetStore s;
    s.save("drive", "Warm", kA, false);
    CHECK(s.freeName("drive", "Warm") == "Warm 2");
    CHECK(s.freeName("drive", "Warm", { "warm 2" }) == "Warm 3");
    CHECK(blockNameTaken({ "Default", "Warm Push" }, "warm push"));
    CHECK(!blockNameTaken({ "Default" }, "Defaults"));
}
