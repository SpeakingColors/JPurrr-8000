// Own preset format (JSON) and bank files. Same layout the mockup exports and imports:
//   {"app":"JPurrr-8000","format":1,"name":..,"type":..,"genres":[..],"v":{"<id>":<int>,..}}
// Bank: {"app":"JPurrr-8000","format":1,"bank":name,"patches":[{name,type,genres,v},..]}
// Strict small JSON parser written for this file: no dependencies, depth limited, never throws.
#include "jpurrr/patch.h"
#include "jpurrr/block_presets.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace jpurrr {
namespace {

constexpr int kFormatVersion = 1;
constexpr int kMaxDepth = 32;
constexpr int kMaxName = 40;
constexpr int kMaxGenres = 16;
const char* const kTypes[] = { "Lead", "Bass", "Keys", "Pad", "Pluck", "Chord", "Brass", "Bell", "Drum", "FX" };

// ------------------------------------------------------------------ value tree
struct JV {
    enum T : unsigned char { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<JV> a;
    std::vector<std::pair<std::string, JV>> o;
    const JV* find(const char* key) const {      // last duplicate wins, like JSON.parse
        const JV* r = nullptr;
        for (const auto& kv : o) if (kv.first == key) r = &kv.second;
        return r;
    }
};

struct Parser {
    const char* p; const char* end; const char* begin;
    std::string err;

    bool fail(const std::string& msg) {
        if (!err.empty()) return false;
        int line = 1, col = 1;
        for (const char* q = begin; q < p && q < end; ++q) { if (*q == '\n') { ++line; col = 1; } else ++col; }
        err = "Not valid preset text (line " + std::to_string(line) + ", column " + std::to_string(col) + "): " + msg + ".";
        return false;
    }
    void ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p; }

    static void putUtf8(std::string& s, unsigned cp) {
        if (cp < 0x80) s += char(cp);
        else if (cp < 0x800) { s += char(0xC0 | (cp >> 6)); s += char(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { s += char(0xE0 | (cp >> 12)); s += char(0x80 | ((cp >> 6) & 0x3F)); s += char(0x80 | (cp & 0x3F)); }
        else { s += char(0xF0 | (cp >> 18)); s += char(0x80 | ((cp >> 12) & 0x3F)); s += char(0x80 | ((cp >> 6) & 0x3F)); s += char(0x80 | (cp & 0x3F)); }
    }
    bool hex4(unsigned& v) {
        if (end - p < 4) return fail("short \\u escape");
        v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = *p++; v <<= 4;
            if (c >= '0' && c <= '9') v |= unsigned(c - '0');
            else if (c >= 'a' && c <= 'f') v |= unsigned(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= unsigned(c - 'A' + 10);
            else return fail("bad \\u escape");
        }
        return true;
    }
    bool str(std::string& out) {
        ++p;                                                     // opening quote
        out.clear();
        while (true) {
            if (p >= end) return fail("text ends inside a string");
            unsigned char c = (unsigned char)*p++;
            if (c == '"') return true;
            if (c < 0x20) return fail("control character inside a string");
            if (c != '\\') { out += char(c); continue; }
            if (p >= end) return fail("text ends inside a string");
            char e = *p++;
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned u = 0; if (!hex4(u)) return false;
                if (u >= 0xD800 && u <= 0xDBFF) {
                    unsigned lo = 0;
                    if (end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                        const char* save = p; p += 2;
                        if (!hex4(lo)) return false;
                        if (lo >= 0xDC00 && lo <= 0xDFFF) u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                        else { p = save; u = 0xFFFD; }
                    } else u = 0xFFFD;
                } else if (u >= 0xDC00 && u <= 0xDFFF) u = 0xFFFD;
                putUtf8(out, u);
                break;
            }
            default: return fail("unknown escape in a string");
            }
        }
    }
    bool num(double& out) {
        const char* s = p;
        bool neg = false;
        if (p < end && *p == '-') { neg = true; ++p; }
        if (p >= end) return fail("number ends early");
        double v = 0; int digits = 0; int exp10 = 0;
        if (*p == '0') { ++p; ++digits; }
        else if (*p >= '1' && *p <= '9') { while (p < end && *p >= '0' && *p <= '9') { if (digits < 18) { v = v * 10 + (*p - '0'); ++digits; } else ++exp10; ++p; } }
        else return fail("expected a digit");
        if (p < end && *p == '.') {
            ++p; int fd = 0;
            while (p < end && *p >= '0' && *p <= '9') { if (digits < 18) { v = v * 10 + (*p - '0'); ++digits; --exp10; } ++p; ++fd; }
            if (!fd) return fail("expected digits after the decimal point");
        }
        if (p < end && (*p == 'e' || *p == 'E')) {
            ++p; bool en = false;
            if (p < end && (*p == '+' || *p == '-')) { en = *p == '-'; ++p; }
            int e = 0, ed = 0;
            while (p < end && *p >= '0' && *p <= '9') { if (e < 100000) e = e * 10 + (*p - '0'); ++p; ++ed; }
            if (!ed) return fail("expected digits in the exponent");
            exp10 += en ? -e : e;
        }
        (void)s;
        if (v != 0 && exp10 != 0) v *= std::pow(10.0, double(exp10 > 400 ? 400 : (exp10 < -400 ? -400 : exp10)));
        if (!std::isfinite(v)) return fail("number is too large");
        out = neg ? -v : v;
        return true;
    }
    bool lit(const char* w) {
        size_t n = std::strlen(w);
        if (size_t(end - p) < n || std::memcmp(p, w, n) != 0) return fail("unexpected text");
        p += n; return true;
    }
    bool value(JV& v, int depth) {
        if (depth > kMaxDepth) return fail("nested too deeply");
        ws();
        if (p >= end) return fail("text ends early");
        char c = *p;
        if (c == '{') {
            ++p; v.t = JV::Obj; ws();
            if (p < end && *p == '}') { ++p; return true; }
            while (true) {
                ws();
                if (p >= end || *p != '"') return fail("expected a quoted name");
                std::string k; if (!str(k)) return false;
                ws();
                if (p >= end || *p != ':') return fail("expected ':'");
                ++p;
                JV child; if (!value(child, depth + 1)) return false;
                v.o.emplace_back(std::move(k), std::move(child));
                ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == '}') { ++p; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            ++p; v.t = JV::Arr; ws();
            if (p < end && *p == ']') { ++p; return true; }
            while (true) {
                JV child; if (!value(child, depth + 1)) return false;
                v.a.push_back(std::move(child));
                ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == ']') { ++p; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { v.t = JV::Str; return str(v.s); }
        if (c == 't') { v.t = JV::Bool; v.b = true; return lit("true"); }
        if (c == 'f') { v.t = JV::Bool; v.b = false; return lit("false"); }
        if (c == 'n') { v.t = JV::Null; return lit("null"); }
        if (c == '-' || (c >= '0' && c <= '9')) { v.t = JV::Num; return num(v.n); }
        return fail("unexpected character");
    }
    bool document(JV& v) {
        if (end - p >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;
        if (!value(v, 0)) return false;
        ws();
        if (p != end) return fail("extra text after the preset");
        return true;
    }
};

bool parseText(const std::string& text, JV& v, std::string& err) {
    Parser ps; ps.begin = text.data(); ps.p = text.data(); ps.end = text.data() + text.size();
    if (!ps.document(v)) { err = ps.err; return false; }
    return true;
}

// ------------------------------------------------------------------ text hygiene
// Keeps valid UTF-8, turns anything else into '?', control characters into spaces, trims, limits code points.
std::string cleanText(const std::string& in, int maxPoints) {
    std::string out; int points = 0; size_t i = 0;
    while (i < in.size() && points < maxPoints) {
        unsigned char c = (unsigned char)in[i];
        size_t len = 1;
        if (c < 0x80) { out += c < 0x20 || c == 0x7F ? ' ' : char(c); }
        else {
            size_t need = (c >= 0xF0 && c <= 0xF4) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC2 && c < 0xE0) ? 1 : 0;
            bool ok = need > 0 && i + need < in.size();
            for (size_t k = 1; ok && k <= need; ++k) if (((unsigned char)in[i + k] & 0xC0) != 0x80) ok = false;
            if (ok) { out.append(in, i, need + 1); len = need + 1; }
            else out += '?';
        }
        i += len; ++points;
    }
    size_t a = 0, b = out.size();
    while (a < b && out[a] == ' ') ++a;
    while (b > a && out[b - 1] == ' ') --b;
    return out.substr(a, b - a);
}

bool isType(const std::string& s) {
    for (const char* t : kTypes) if (s == t) return true;
    return false;
}

// ------------------------------------------------------------------ reading
bool checkApp(const JV& o, std::string& err) {
    const JV* app = o.find("app");
    if (!app || app->t != JV::Str || (app->s != "JPurrr-8000" && app->s != "JPurr-8000")) { err = "This is not a JPurrr-8000 preset."; return false; }
    const JV* fmt = o.find("format");
    if (fmt && fmt->t == JV::Num && fmt->n > kFormatVersion) {
        err = "This preset was made by a newer version of JPurrr-8000 (format " + std::to_string(int(fmt->n)) + "). Please update the plugin.";
        return false;
    }
    return true;
}

std::string numText(double n) {
    char b[48];
    if (std::fabs(n) < 1e15 && n == std::floor(n)) std::snprintf(b, sizeof b, "%.0f", n);
    else std::snprintf(b, sizeof b, "%g", n);
    return b;
}

bool readPatch(const JV& o, Patch& out, std::string& err) {
    if (o.t != JV::Obj) { err = "This is not a JPurrr-8000 preset."; return false; }
    const JV* name = o.find("name"); const JV* v = o.find("v");
    if (!name || name->t != JV::Str || !v || v->t != JV::Obj) { err = "This is not a JPurrr-8000 preset."; return false; }
    Patch p;
    p.name = cleanText(name->s, kMaxName);
    if (p.name.empty()) p.name = "Untitled";
    const JV* type = o.find("type");
    p.type = (type && type->t == JV::Str && isType(type->s)) ? type->s : "Lead";
    const JV* genres = o.find("genres");
    if (genres && genres->t == JV::Arr) {
        for (const JV& g : genres->a) {
            if (g.t != JV::Str || int(p.genres.size()) >= kMaxGenres) continue;
            std::string c = cleanText(g.s, kMaxName);
            if (!c.empty()) p.genres.push_back(c);
        }
    }
    for (const auto& kv : v->o) {
        int idx = paramIndexById(kv.first.c_str());
        if (idx < 0 || (kParams[idx].flags & kNoSave)) continue;              // unknown keys and live-only controls are ignored
        const JV& x = kv.second;
        const ParamDef& d = kParams[idx];
        if (x.t != JV::Num) { err = "The value of '" + kv.first + "' (" + d.label + ") is not a number."; return false; }
        if (x.n < d.min || x.n > d.max) {
            err = "The value " + numText(x.n) + " for '" + kv.first + "' (" + d.label + ") is outside its range " + std::to_string(d.min) + " to " + std::to_string(d.max) + ".";
            return false;
        }
        int iv = int(std::floor(x.n + 0.5));
        if (iv < d.min) iv = d.min;
        if (iv > d.max) iv = d.max;
        p.v[idx] = iv;
    }
    out = std::move(p);
    return true;
}

// ------------------------------------------------------------------ writing
void esc(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
            else out += char(c);
        }
    }
    out += '"';
}
void pad(std::string& out, int n) { out.append(size_t(n), ' '); }   // JSON.stringify(x, null, 1) style: one space per level

void writePatch(std::string& out, const Patch& p, int lvl, bool top) {
    out += "{\n";
    if (top) { pad(out, lvl + 1); out += "\"app\": \"JPurrr-8000\",\n"; pad(out, lvl + 1); out += "\"format\": " + std::to_string(kFormatVersion) + ",\n"; }
    pad(out, lvl + 1); out += "\"name\": "; esc(out, p.name); out += ",\n";
    pad(out, lvl + 1); out += "\"type\": "; esc(out, p.type.empty() ? std::string("Lead") : p.type); out += ",\n";
    pad(out, lvl + 1); out += "\"genres\": ";
    if (p.genres.empty()) out += "[]";
    else {
        out += "[\n";
        for (size_t i = 0; i < p.genres.size(); ++i) { pad(out, lvl + 2); esc(out, p.genres[i]); out += i + 1 < p.genres.size() ? ",\n" : "\n"; }
        pad(out, lvl + 1); out += "]";
    }
    out += ",\n";
    pad(out, lvl + 1); out += "\"v\": ";
    bool any = false;
    for (int i = 0; i < kNumParams; ++i) {
        if ((kParams[i].flags & kNoSave) || p.v[i] == kParams[i].def) continue;
        out += any ? ",\n" : "{\n"; any = true;
        pad(out, lvl + 2); esc(out, kParams[i].id); out += ": " + std::to_string(p.v[i]);
    }
    if (any) { out += "\n"; pad(out, lvl + 1); out += "}"; } else out += "{}";
    out += "\n"; pad(out, lvl); out += "}";
}

}  // namespace

std::string patchToJson(const Patch& p) {
    std::string out; out.reserve(2048);
    writePatch(out, p, 0, true);
    out += "\n";
    return out;
}

bool patchFromJson(const std::string& text, Patch& out, std::string& err) {
    JV root;
    if (!parseText(text, root, err)) return false;
    if (root.t != JV::Obj) { err = "This is not a JPurrr-8000 preset."; return false; }
    if (root.find("patches") && !root.find("v")) { err = "This file is a bank of presets, not a single preset. Load it as a bank."; return false; }
    if (!checkApp(root, err)) return false;
    return readPatch(root, out, err);
}

std::string bankToJson(const std::vector<Patch>& patches, const std::string& bankName) {
    std::string out; out.reserve(patches.size() * 1500 + 128);
    out += "{\n \"app\": \"JPurrr-8000\",\n \"format\": " + std::to_string(kFormatVersion) + ",\n \"bank\": ";
    esc(out, cleanText(bankName, kMaxName));
    out += ",\n \"patches\": ";
    if (patches.empty()) out += "[]";
    else {
        out += "[\n";
        for (size_t i = 0; i < patches.size(); ++i) {
            pad(out, 2); writePatch(out, patches[i], 2, false);
            out += i + 1 < patches.size() ? ",\n" : "\n";
        }
        out += " ]";
    }
    out += "\n}\n";
    return out;
}

bool bankFromJson(const std::string& text, std::vector<Patch>& out, std::string& bankName, std::string& err) {
    JV root;
    if (!parseText(text, root, err)) return false;
    if (root.t != JV::Obj) { err = "This is not a JPurrr-8000 preset bank."; return false; }
    if (!checkApp(root, err)) return false;
    std::vector<Patch> got; std::string name;
    const JV* list = root.find("patches");
    if (list) {
        if (list->t != JV::Arr) { err = "This is not a JPurrr-8000 preset bank."; return false; }
        const JV* b = root.find("bank");
        if (b && b->t == JV::Str) name = cleanText(b->s, kMaxName);
        got.reserve(list->a.size());
        for (size_t i = 0; i < list->a.size(); ++i) {
            Patch p; std::string e;
            if (!readPatch(list->a[i], p, e)) { err = "Preset " + std::to_string(i + 1) + " in the bank: " + e; return false; }
            got.push_back(std::move(p));
        }
    } else {                                                   // a single preset file loads as a bank of one
        Patch p;
        if (!readPatch(root, p, err)) return false;
        got.push_back(std::move(p));
    }
    out = std::move(got); bankName = std::move(name);
    return true;
}


// ------------------------------------------------------------------ block presets (own FX block presets)
std::string BlockPresetStore::toJson() const {
    std::string out = "{\n \"app\": \"JPurrr-8000\",\n \"format\": " + std::to_string(kFormatVersion) + ",\n \"blocks\": {";
    bool firstBlock = true;
    for (const auto& b : blocks_) {
        if (b.second.empty()) continue;
        out += firstBlock ? "\n  " : ",\n  "; firstBlock = false;
        esc(out, b.first); out += ": [";
        for (size_t i = 0; i < b.second.size(); ++i) {
            const auto& e = b.second[i];
            out += i ? ",\n   {" : "\n   {";
            out += "\"n\": "; esc(out, e.name); out += ", \"v\": {";
            for (size_t k = 0; k < e.vals.size(); ++k) { if (k) out += ", "; esc(out, e.vals[k].first); out += ": " + std::to_string(e.vals[k].second); }
            out += "}}";
        }
        out += "\n  ]";
    }
    out += firstBlock ? "}\n}\n" : "\n }\n}\n";
    return out;
}
bool BlockPresetStore::fromJson(const std::string& text, std::string& err) {
    JV root;
    if (!parseText(text, root, err)) return false;
    if (root.t != JV::Obj || !checkApp(root, err)) { if (err.empty()) err = "This is not a JPurrr-8000 file."; return false; }
    std::map<std::string, std::vector<BlockPresetEntry>> nb;
    const JV* bl = root.find("blocks");
    if (bl && bl->t == JV::Obj) {
        for (const auto& kv : bl->o) {
            if (kv.second.t != JV::Arr || kv.first.empty() || kv.first.size() > 16) continue;
            auto& list = nb[kv.first];
            for (const JV& e : kv.second.a) {
                if (e.t != JV::Obj || int(list.size()) >= kMaxPerBlock) continue;
                const JV* n = e.find("n"); const JV* v = e.find("v");
                if (!n || n->t != JV::Str || !v || v->t != JV::Obj) continue;
                BlockPresetEntry ent; ent.name = cleanName(n->s);
                if (ent.name.empty()) continue;
                bool dup = false;
                for (const auto& x : list) { if (x.name.size() == ent.name.size() && std::equal(x.name.begin(), x.name.end(), ent.name.begin(), [](char a, char b) { return std::tolower((unsigned char) a) == std::tolower((unsigned char) b); })) dup = true; }
                if (dup) continue;
                for (const auto& pv : v->o) {
                    if (pv.second.t != JV::Num || pv.first.empty() || pv.first.size() > 12) continue;
                    if (!(pv.second.n > -1e9 && pv.second.n < 1e9)) continue;
                    ent.vals.emplace_back(pv.first, int(std::lround(pv.second.n)));
                }
                list.push_back(std::move(ent));
            }
            if (list.empty()) nb.erase(kv.first);
        }
    }
    blocks_ = std::move(nb);
    return true;
}

}  // namespace jpurrr
