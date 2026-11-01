// Melody generator (see include/jpurrr/melody_gen.h). A line by line port of tools/melody_gen.src.js, so both sides give the
// same notes for the same settings. The JavaScript side computes one seed expression in floating point ((seed * 2654435761 +
// tries * 40503) >>> 0), so this file repeats that in double on purpose.
#include "jpurrr/melody_gen.h"
#include "melody_gen_data.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace jpurrr {
namespace {
using gendata::Rhythm; using gendata::Step; using gendata::Prog;

constexpr int MINOR[7] = { 0, 2, 3, 5, 7, 8, 10 }, MAJOR[7] = { 0, 2, 4, 5, 7, 9, 11 };
constexpr int LO = 72, HI = 98, WR = 12;
const char* const NAMES_MIN[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
const char* const NAMES_MAJ[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const char* const STYLES[6] = { "Any", "Anthem", "Run", "Roller", "Stabs", "Sparse" };
constexpr int GATE[5] = { 92, 92, 80, 60, 92 };            // anthem run roller stabs sparse
constexpr int SHAPES[4][8] = { { 0, 0, 2, 0, 5, 7, 9, 2 }, { 0, 2, 0, 3, 6, 8, 6, 2 }, { 0, 0, 0, 0, 6, 6, 8, 0 }, { 0, 1, 2, 3, 6, 8, 9, 3 } };

struct Rng {
    uint32_t x;
    explicit Rng(uint32_t seed) { x = seed ? seed : 0x9E3779B9u; for (int i = 0; i < 4; ++i) next(); }
    uint32_t next() { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; }
    int n(int k) { return (int) ((next() >> 8) % (uint32_t) k); }
    int pick(const int* w, int m) { int t = 0; for (int i = 0; i < m; ++i) t += w[i]; int q = n(t); for (int i = 0; i < m; ++i) { if (q < w[i]) return i; q -= w[i]; } return m - 1; }
};
inline int mod12(int v) { return ((v % 12) + 12) % 12; }
inline int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

struct Chord { int root; bool minor; int tones[3]; };
Chord chordInfo(int key, int off, bool m) { Chord c; c.root = mod12(key + off); c.minor = m; c.tones[0] = c.root; c.tones[1] = mod12(c.root + (m ? 3 : 4)); c.tones[2] = mod12(c.root + 7); return c; }
inline bool inTones(const Chord& c, int pc) { return pc == c.tones[0] || pc == c.tones[1] || pc == c.tones[2]; }

struct Note { int b, k, s, n, abs, p; bool anchor; };

struct Attempt {
    int fam = 0;                    // 0..4
    Chord chords[8];
    bool minor = true; int key = 0;
    Note N[136]; int nN = 0;
    int ladder[8][28]; bool isCT[8][28]; int ladN[8];   // at most 27 notes of LO..HI fit the scale plus a chromatic chord
};

int nearestIdx(const int* L, int n, int v) { int bi = 0, bd = 1 << 30; for (int i = 0; i < n; ++i) { const int d = std::abs(L[i] - v); if (d < bd) { bd = d; bi = i; } } return bi; }
int indexOf(const int* L, int n, int v) { for (int i = 0; i < n; ++i) if (L[i] == v) return i; return -1; }

bool attempt(uint32_t seed, int key, bool minor, int style, Attempt& r) {
    Rng R(seed);
    r.minor = minor; r.key = key;
    int scale[7]; for (int i = 0; i < 7; ++i) scale[i] = mod12(key + (minor ? MINOR : MAJOR)[i]);
    auto inScale = [&](int pc) { for (int i = 0; i < 7; ++i) if (scale[i] == pc) return true; return false; };
    // chords: 8 bars
    const Prog* pool = minor ? gendata::kProg_minor : gendata::kProg_major;
    const int poolN = minor ? gendata::kNumProg_minor : gendata::kNumProg_major;
    const Prog& base = pool[R.n(poolN)];
    signed char off[8]; bool qm[8]; int progN = base.n;
    for (int i = 0; i < progN; ++i) { off[i] = base.c[i].off; qm[i] = base.c[i].minor; }
    if (progN == 4) {
        for (int i = 0; i < 4; ++i) { off[4 + i] = off[i]; qm[4 + i] = qm[i]; }
        if (R.n(100) < 60) {
            static const signed char tn[4][2] = { { 10, 0 }, { 7, 0 }, { 8, 0 }, { 7, 1 } };
            static const signed char tj[3][2] = { { 7, 0 }, { 5, 0 }, { 9, 1 } };
            if (minor) { const auto& t = tn[R.n(4)]; off[7] = t[0]; qm[7] = t[1] != 0; }
            else { const auto& t = tj[R.n(3)]; off[7] = t[0]; qm[7] = t[1] != 0; }
        }
        progN = 8;
    }
    for (int b = 0; b < 8; ++b) r.chords[b] = chordInfo(key, off[b], qm[b]);
    // rhythm
    int fam;
    if (style >= 1) fam = style - 1;
    else { int w[5]; for (int f = 0; f < 5; ++f) { w[f] = 0; for (int i = 0; i < gendata::kRhythmCount[f]; ++i) w[f] += gendata::kRhythms[f][i].w; } fam = R.pick(w, 5); }
    r.fam = fam;
    const Rhythm* bank = gendata::kRhythms[fam]; const int bankN = gendata::kRhythmCount[fam];
    int bw[40]; for (int i = 0; i < bankN; ++i) bw[i] = bank[i].w;
    const int ai = R.pick(bw, bankN); int bi = ai;
    for (int i = 0; i < 6 && bi == ai && bankN > 1; ++i) bi = R.pick(bw, bankN);
    const int famEndPc = fam == 0 ? 80 : fam == 1 ? 60 : 0;
    const bool endAny = R.n(100) < famEndPc;
    const int ei = endAny ? R.n(gendata::kNumEndings) : -1;
    int tpl[8];                                             // -1 = ending, otherwise bank index
    for (int b = 0; b < 8; ++b) {
        int t = (b % 4 == 3) ? bi : ai;
        if (b >= 4 && b <= 6 && bi != ai && R.n(100) < 35) t = (t == ai) ? bi : ai;
        if (b == 7 && ei >= 0) t = -1;
        tpl[b] = t;
    }
    auto rhythmOf = [&](int t) -> const Rhythm& { return t < 0 ? gendata::kEndings[ei] : bank[t]; };
    // register and shape
    const int* shape = SHAPES[R.n(4)]; const int center0 = 82 + R.n(5) - 2;
    const int pedalType = fam == 2 ? R.n(2) : 0;
    const int pedalPc = R.n(2) ? key : mod12(key + 7);
    // notes
    r.nN = 0;
    for (int b = 0; b < 8; ++b) { const Rhythm& t = rhythmOf(tpl[b]); for (int k = 0; k < t.n; ++k) { Note& o = r.N[r.nN++]; o = { b, k, t.st[k].s, t.st[k].n, b * 16 + t.st[k].s, -1, false }; } }
    const int lastI = r.nN - 1;
    for (int b = 0; b < 8; ++b) {
        int& ln = r.ladN[b]; ln = 0;
        for (int m = LO; m <= HI; ++m) { const int pc = mod12(m); const bool c = inTones(r.chords[b], pc); if (c || inScale(pc)) { r.ladder[b][ln] = m; r.isCT[b][ln] = c; ++ln; } }
    }
    // pass 1: anchors
    int prev = -1, d8[8], centers[8];
    for (int b = 0; b < 8; ++b) d8[b] = R.n(7) - 3;
    for (int b = 0; b < 8; ++b) centers[b] = center0 + shape[b] + (R.n(3) - 1);
    for (int i = 0; i < r.nN; ++i) {
        Note& o = r.N[i];
        if (!(o.s == 0 || o.s == 8 || i == lastI)) continue;
        o.anchor = true;
        const int* L = r.ladder[o.b]; const bool* C = r.isCT[o.b]; const int ln = r.ladN[o.b];
        const int tgt = centers[o.b] + (o.s >= 8 ? d8[o.b] : 0) + (i == lastI ? -2 : 0);
        int cand[28], cn = 0;
        for (int k = 0; k < ln; ++k) if (C[k]) cand[cn++] = L[k];
        std::sort(cand, cand + cn, [&](int x, int y) { const int dx = std::abs(x - tgt), dy = std::abs(y - tgt); return dx != dy ? dx < dy : x < y; });
        int list[28], lnn = 0;
        if (prev >= 0) {
            for (int k = 0; k < cn; ++k) if (std::abs(cand[k] - prev) <= 9) list[lnn++] = cand[k];
            if (lnn == 0) for (int k = 0; k < cn; ++k) if (std::abs(cand[k] - prev) <= 12) list[lnn++] = cand[k];
        }
        if (lnn == 0) { for (int k = 0; k < cn; ++k) list[lnn++] = cand[k]; }
        static const int pw[3] = { 6, 3, 1 };
        o.p = list[std::min(R.pick(pw, 3), lnn - 1)]; prev = o.p;
    }
    // pass 2
    const int pReps[5] = { 6, 8, 0, 4, 4 }; const int pRep = pReps[fam];
    int copyFrom[8]; signed char rel[8][20];
    for (int b = 0; b < 8; ++b) { copyFrom[b] = -1; for (int k = 0; k < 20; ++k) rel[b][k] = 127; }
    for (int b = 1; b < 8; ++b) {
        int cand[2][2]; int cn = 0;
        if (tpl[b - 1] == tpl[b]) { cand[cn][0] = b - 1; cand[cn][1] = 60; ++cn; }
        if (b >= 4 && tpl[b - 4] == tpl[b]) { cand[cn][0] = b - 4; cand[cn][1] = 25; ++cn; }
        for (int c = 0; c < cn; ++c) if (copyFrom[b] < 0 && R.n(100) < cand[c][1]) copyFrom[b] = cand[c][0];
    }
    int lastP = -1;
    auto anchorAfter = [&](int i) -> const Note* { for (int j = i + 1; j < r.nN; ++j) if (r.N[j].anchor) return &r.N[j]; return nullptr; };
    auto anchorBefore = [&](int i) -> const Note* { for (int j = i - 1; j >= 0; --j) if (r.N[j].anchor) return &r.N[j]; return nullptr; };
    int idx = 0;
    for (int b = 0; b < 8; ++b) {
        const int* L = r.ladder[b]; const int ln = r.ladN[b];
        int barAnchorIdx = -1;
        while (idx < r.nN && r.N[idx].b == b) {
            Note& o = r.N[idx]; const int i = idx; ++idx;
            if (o.anchor) { barAnchorIdx = nearestIdx(L, ln, o.p); o.p = L[barAnchorIdx]; if (o.k < 20) rel[b][o.k] = 127; lastP = o.p; continue; }
            int p = -1;
            if (copyFrom[b] >= 0 && barAnchorIdx >= 0 && o.k < 20 && rel[copyFrom[b]][o.k] != 127) {
                const int j = barAnchorIdx + rel[copyFrom[b]][o.k];
                if (j >= 0 && j < ln) p = L[j];
            }
            if (p < 0) {
                const Note* aB = anchorBefore(i); const Note* aA = anchorAfter(i);
                const int pa = aB ? aB->p : aA->p, pb = aA ? aA->p : pa;
                const int sa = aB ? aB->abs : aA->abs, sb = aA ? aA->abs : sa + 1;
                if (fam == 2 && (o.s % 2 == 1)) {
                    const int pc = pedalType ? mod12(r.chords[b].root) : pedalPc;
                    int best = -1, bd = 1 << 30; const int c0 = centers[b] + 2;
                    for (int m = LO; m <= HI; ++m) if (mod12(m) == pc && std::abs(m - c0) < bd) { bd = std::abs(m - c0); best = m; }
                    p = best;
                } else {
                    const int t100 = sb > sa ? (int) std::lround(100.0 * (o.abs - sa) / (sb - sa)) : 0;
                    const int tgt = (int) std::lround(pa + (double) (pb - pa) * t100 / 100.0);
                    int j = nearestIdx(L, ln, tgt);
                    const int li = indexOf(L, ln, lastP);
                    if (lastP >= 0 && R.n(100) < pRep && li >= 0) j = li;
                    else {
                        static const int sw[5] = { 2, 3, 2, 3, 2 };
                        static const int moves[5] = { -2, -1, 0, 1, 2 };
                        j += moves[R.pick(sw, 5)];
                        if (R.n(100) < 45) { int bj = j, bd = 99; for (int q = std::max(0, j - 3); q <= std::min(ln - 1, j + 3); ++q) if (r.isCT[b][q] && std::abs(q - j) < bd) { bd = std::abs(q - j); bj = q; } j = bj; }
                        if (L[clampi(j, 0, ln - 1)] == lastP) { const int dir = (aA && aA->p != lastP) ? (aA->p > lastP ? 1 : -1) : (R.n(2) ? 1 : -1); j += dir * (1 + R.n(2)); }
                    }
                    const Note* nx = (idx < r.nN) ? &r.N[idx] : nullptr;
                    if (nx && nx->anchor && nx->b == b && R.n(100) < 60) { const int aj = nearestIdx(L, ln, nx->p); j = aj + (lastP >= nx->p ? 1 : -1); }
                    j = clampi(j, 0, ln - 1); p = L[j];
                }
                const int lim = fam == 2 ? 12 : 9;
                if (lastP >= 0 && std::abs(p - lastP) > lim) p = L[nearestIdx(L, ln, lastP + (p > lastP ? lim : -lim))];
            }
            o.p = p; lastP = p;
            if (o.k < 20) rel[b][o.k] = (signed char) (barAnchorIdx >= 0 ? nearestIdx(L, ln, p) - barAnchorIdx : 127);
        }
    }
    return true;
}

bool validateA(const Attempt& r) {
    int scale[7]; for (int i = 0; i < 7; ++i) scale[i] = mod12(r.key + (r.minor ? MINOR : MAJOR)[i]);
    auto inScale = [&](int pc) { for (int i = 0; i < 7; ++i) if (scale[i] == pc) return true; return false; };
    int top = -1, topBar = -1, mn = 999, prev = -1, maxLeap = 0;
    for (int i = 0; i < r.nN; ++i) {
        const Note& o = r.N[i]; const int pc = mod12(o.p);
        if (!inScale(pc) && !inTones(r.chords[o.b], pc)) return false;
        if ((o.s == 0 || o.s == 8) && !inTones(r.chords[o.b], pc)) return false;
        if (o.p > top) { top = o.p; topBar = o.b + 1; }
        mn = std::min(mn, o.p); if (prev >= 0) maxLeap = std::max(maxLeap, std::abs(o.p - prev)); prev = o.p;
    }
    if (topBar < 5 || topBar > 7) return false;
    if (top - mn > 24 || maxLeap > 12) return false;
    if (r.nN < 14) return false;
    return true;
}
}  // namespace

const char* melodyStyleName(int s) { return STYLES[clampi(s, 0, 5)]; }
const char* melodyKeyName(int key, bool minorNames) { const char* const* t = minorNames ? NAMES_MIN : NAMES_MAJ; return t[mod12(key)]; }

MelodyGenResult melodyGenerate(int seedIn, int keyIn, int mode, int styleIn) {
    MelodyGenResult out;
    const int seed = (int) (((uint32_t) seedIn) % 16777216u) ? (int) (((uint32_t) seedIn) % 16777216u) : 1;
    const int key = mod12(keyIn), style = clampi(styleIn, 0, 5);
    Attempt r; bool found = false; int tries = 0;
    for (; tries < 64; ++tries) {
        // the JavaScript side computes this in floating point; repeated here so both sides agree
        const double d = (double) seed * 2654435761.0 + (double) (tries * 40503);
        const uint32_t s0 = (uint32_t) (uint64_t) std::fmod(d, 4294967296.0);
        Rng R0(s0);
        const bool minor = mode == 0 ? true : mode == 1 ? false : R0.n(100) < 85;
        const uint32_t sd = (uint32_t) seed ^ ((uint32_t) tries * 0x9E3779B1u);
        if (attempt(sd, key, minor, style, r) && validateA(r)) { found = true; break; }
    }
    if (!found) return out;
    out.ok = true; out.seed = seed; out.key = key; out.minor = r.minor; out.style = r.fam + 1; out.tries = tries;
    static const int kBpms[3] = { 136, 138, 140 };
    out.bpm = kBpms[Rng((uint32_t) (seed + 7)).n(3)];
    out.beats = 32.0;
    char cj[104]; int cl = 0; char csp[104]; int cs = 0;
    const char* const* names = r.minor ? NAMES_MIN : NAMES_MAJ;   // two statements: gcc's ubsan miscompiles the ternary inside the subscript
    for (int b = 0; b < 8; ++b) {
        const char* nm = names[r.chords[b].root];
        cl += std::snprintf(cj + cl, sizeof(cj) - (size_t) cl, "%s%s%s", b ? "-" : "", nm, r.chords[b].minor ? "m" : "");
        cs += std::snprintf(csp + cs, sizeof(csp) - (size_t) cs, "%s%s%s", b ? " " : "", nm, r.chords[b].minor ? "m" : "");
    }
    std::snprintf(out.name, sizeof(out.name), "Generated #%d, %s", seed, cj);
    std::snprintf(out.chords, sizeof(out.chords), "%s", csp);
    out.n = 0;
    const int gate = GATE[r.fam];
    for (int i = 0; i < r.nN && out.n < 136; ++i) {
        const Note& o = r.N[i];
        const bool accent = o.s == 0 || o.s == 8;
        int vel = accent ? 108 : (o.n >= 6 ? 100 : 90);
        if (o.b >= 4 && o.b <= 6) vel = std::min(120, vel + 8);
        const int g = r.fam == 3 ? gate : (o.n == 1 ? 80 : gate);
        PlayEvent& e = out.ev[out.n++];
        e.beat = (float) (o.b * 4 + o.s * 0.25);
        e.len = (float) (std::round((double) o.n * g * 2.5) / 1000.0);
        e.note = (std::uint8_t) clampi(o.p - WR, 0, 127);
        e.vel = (std::uint8_t) clampi(vel, 1, 127);
    }
    return out;
}

}  // namespace jpurrr
