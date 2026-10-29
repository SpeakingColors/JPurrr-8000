#include "jpurrr/block_presets.h"
#include <algorithm>
#include <cctype>
#include "jpurrr/params_gen.h"

namespace jpurrr {
namespace {
const std::vector<BlockPresetEntry> kEmpty;
bool sameName(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char) a[i]) != std::tolower((unsigned char) b[i])) return false;
    return true;
}
}  // namespace

const std::vector<BlockPresetEntry>& BlockPresetStore::list(const std::string& block) const {
    auto it = blocks_.find(block);
    return it == blocks_.end() ? kEmpty : it->second;
}
int BlockPresetStore::find(const std::string& block, const std::string& name) const {
    const auto& l = list(block);
    for (size_t i = 0; i < l.size(); ++i) if (sameName(l[i].name, name)) return int(i);
    return -1;
}
BlockPresetStore::Result BlockPresetStore::save(const std::string& block, const std::string& name0,
                                                const std::vector<std::pair<std::string, int>>& vals, bool overwrite) {
    const std::string name = cleanName(name0);
    if (name.empty()) return Result::EmptyName;
    auto& l = blocks_[block];
    const int at = find(block, name);
    if (at >= 0) {
        if (!overwrite) return Result::Exists;
        l[size_t(at)].name = name; l[size_t(at)].vals = vals;
        return Result::Ok;
    }
    if (int(l.size()) >= kMaxPerBlock) { if (l.empty()) blocks_.erase(block); return Result::Full; }
    l.push_back({ name, vals });
    return Result::Ok;
}
BlockPresetStore::Result BlockPresetStore::rename(const std::string& block, const std::string& oldName, const std::string& newName0) {
    const std::string nn = cleanName(newName0);
    if (nn.empty()) return Result::EmptyName;
    const int at = find(block, oldName);
    if (at < 0) return Result::NotFound;
    const int other = find(block, nn);
    if (other >= 0 && other != at) return Result::Exists;
    blocks_[block][size_t(at)].name = nn;
    return Result::Ok;
}
BlockPresetStore::Result BlockPresetStore::remove(const std::string& block, const std::string& name) {
    const int at = find(block, name);
    if (at < 0) return Result::NotFound;
    auto& l = blocks_[block];
    l.erase(l.begin() + at);
    if (l.empty()) blocks_.erase(block);
    return Result::Ok;
}
std::string BlockPresetStore::freeName(const std::string& block, const std::string& name, const std::vector<std::string>& reserved) const {
    const std::string base = cleanName(name);
    for (int n = 2; n < 1000; ++n) {
        std::string cand = base + " " + std::to_string(n);
        if (int(cand.size()) > kMaxName) cand = base.substr(0, size_t(std::max(1, kMaxName - int(std::to_string(n).size()) - 1))) + " " + std::to_string(n);
        if (find(block, cand) < 0 && !blockNameTaken(reserved, cand)) return cand;
    }
    return base;
}
std::string BlockPresetStore::cleanName(const std::string& s) {
    std::string out; int points = 0;
    for (size_t i = 0; i < s.size() && points < kMaxName; ) {
        const unsigned char c = (unsigned char) s[i];
        size_t len = c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        if (i + len > s.size()) break;
        if (c < 0x20 || c == 0x7F) out += ' '; else out.append(s, i, len);
        i += len; ++points;
    }
    size_t a = 0, b = out.size();
    while (a < b && out[a] == ' ') ++a;
    while (b > a && out[b - 1] == ' ') --b;
    return out.substr(a, b - a);
}

bool blockNameTaken(const std::vector<std::string>& names, const std::string& name) {
    for (const auto& n : names) if (sameName(n, name)) return true;
    return false;
}

std::vector<int> blockPresetValues(const BlockPresetEntry& e, const std::vector<std::string>& ids) {
    std::vector<int> out;
    for (const auto& id : ids) {
        const int pi = paramIndexById(id.c_str());
        int v = pi >= 0 ? kParams[pi].def : 0;
        if (pi >= 0)
            for (const auto& kv : e.vals)
                if (kv.first == id) { if (kv.second >= kParams[pi].min && kv.second <= kParams[pi].max) v = kv.second; break; }
        out.push_back(v);
    }
    return out;
}

bool blockPresetMatches(const BlockPresetEntry& e, const std::vector<std::string>& ids, const std::function<int(const std::string&)>& current) {
    const std::vector<int> v = blockPresetValues(e, ids);
    int gpt = -1;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (current(ids[i]) != v[i]) return false;
        if (ids[i] == "gpt") gpt = v[i];
    }
    if (gpt == 0 && current("gpat") != blockPresetValues(e, { "gpat" })[0]) return false;   // own 16 step pattern: it has to match as well
    return true;
}

}  // namespace jpurrr
