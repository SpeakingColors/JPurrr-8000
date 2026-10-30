#include "jpurrr/keyboard.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace jpurrr {

CkAction computerKeyAction(uint32_t c) {
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    auto note = [](int o) { return CkAction { CkCmd::Note, o }; };
    switch (c) {
    case 'a': return note(0);  case 'w': return note(1);  case 's': return note(2);  case 'e': return note(3);
    case 'd': return note(4);  case 'f': return note(5);  case 't': return note(6);  case 'g': return note(7);
    case 'y': return note(8);  case 'h': return note(9);  case 'u': return note(10); case 'j': return note(11);
    case 'k': return note(12); case 'o': return note(13); case 'l': return note(14); case 'p': return note(15);
    case ';': return note(16); case '\'': return note(17);
    case 'z': return { CkCmd::OctaveDown, 0 };  case 'x': return { CkCmd::OctaveUp, 0 };
    case 'c': return { CkCmd::VelocityDown, 0 }; case 'v': return { CkCmd::VelocityUp, 0 };
    default: return {};
    }
}

int ckStepOctave(int base, int dir) {
    const int b = base + (dir > 0 ? 12 : dir < 0 ? -12 : 0);
    return b < kCkBaseMin ? kCkBaseMin : b > kCkBaseMax ? kCkBaseMax : b;
}
static const int kCkVel[7] = { 20, 40, 60, 80, 100, 120, 127 };
int ckStepVelocity(int vel, int dir) {
    int best = 0;
    for (int i = 1; i < 7; ++i) if (std::abs(kCkVel[i] - vel) < std::abs(kCkVel[best] - vel)) best = i;
    best += dir > 0 ? 1 : dir < 0 ? -1 : 0;
    return kCkVel[best < 0 ? 0 : best > 6 ? 6 : best];
}
int ckNoteName(int note, char* out, int cap) {
    static const char* const nm[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int m = ((note % 12) + 12) % 12;
    return std::snprintf(out, (size_t) cap, "%s%d", nm[m], (int) std::floor(note / 12.0) - 1);
}
int ckKeysText(int base, char* out, int cap) {
    char a[16], b[16]; ckNoteName(base, a, 16); ckNoteName(base + kCkSpan - 1, b, 16);
    return std::snprintf(out, (size_t) cap, "%s - %s", a, b);
}

float clamp01(float n) { return n < 0.f ? 0.f : n > 1.f ? 1.f : n; }
static int roundHalfUp(float v) { return (int) std::floor(v + 0.5f); }          // JavaScript Math.round

float bendWheelNorm(int b) { return b >= 0 ? 0.5f + b / 63.f / 2.f : 0.5f + b / 64.f / 2.f; }
int bendFromWheelNorm(float n) {
    n = clamp01(n); const float t = (n - 0.5f) * 2.f;
    int v = roundHalfUp(t >= 0.f ? t * 63.f : t * 64.f);
    return v < -64 ? -64 : v > 63 ? 63 : v;
}
float modWheelNorm(int m) { return m / 127.f; }
int modFromWheelNorm(float n) { const int v = roundHalfUp(clamp01(n) * 127.f); return v < 0 ? 0 : v > 127 ? 127 : v; }
float bendWheelCents(int b, int up, int down) { return b >= 0 ? b / 63.f * up * 100.f : b / 64.f * (-down) * 100.f; }

float wheelDragNorm(float startNorm, float travelUp, float h) { return h > 0.f ? clamp01(startNorm + travelUp / (h * 0.8f)) : startNorm; }
float bendWheelStep(float n, int dir, bool page) { return clamp01(n + dir * (page ? 0.25f : 0.08f)); }
float modWheelScroll(float n, int dir, bool fine) { return clamp01(n + dir * (fine ? 1.f : 6.f) / 127.f); }
float modWheelKey(float n, int dir, bool page, bool fine) { return clamp01(n + dir * (page ? 16.f : fine ? 1.f : 4.f) / 127.f); }

int bendSpringValue(int v0, float tMs) {
    const float k = tMs >= kSpringMs ? 1.f : tMs <= 0.f ? 0.f : tMs / kSpringMs;
    int v = roundHalfUp(v0 * (1.f - k));
    return v < -64 ? -64 : v > 63 ? 63 : v;
}

// cubic-bezier(0, 0, 0.58, 1) (CSS ease-out), solved for x by bisection
float handleEase(float from, float to, float tMs) {
    float x = tMs / kHandleMs; x = x < 0.f ? 0.f : x > 1.f ? 1.f : x;
    float lo = 0.f, hi = 1.f, s = x;
    for (int i = 0; i < 24; ++i) {
        s = 0.5f * (lo + hi);
        const float u = 1.f - s, bx = 3.f * u * s * s * 0.58f + s * s * s;
        if (bx < x) lo = s; else hi = s;
    }
    const float u = 1.f - s, by = 3.f * s * s * u + s * s * s;
    return from + (to - from) * by;
}

int wheelBubbleText(bool mod, int value, int up, int down, char* out, int cap) {
    if (mod) return std::snprintf(out, (size_t) cap, "MOD  %d", value);
    const int c = roundHalfUp(bendWheelCents(value, up, down));
    return std::snprintf(out, (size_t) cap, "BEND  %s%.1f st", c > 0 ? "+" : "", c / 100.0);
}

int keyAtPoint(const KeyBox* keys, int n, float x, float y) {
    for (int pass = 1; pass >= 0; --pass)                       // black keys first
        for (int i = 0; i < n; ++i) {
            const KeyBox& k = keys[i];
            if ((k.black ? 1 : 0) != pass) continue;
            if (x >= k.x && x < k.x + k.w && y >= k.y && y < k.y + k.h) return k.note;
        }
    return -1;
}

}
