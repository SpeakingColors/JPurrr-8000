#include "jpurrr/keyboard.h"
#include <cmath>
#include <cstdio>

namespace jpurrr {

int keyboardNoteForChar(uint32_t c) {
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    switch (c) {
    case 'z': return 48; case 's': return 49; case 'x': return 50; case 'd': return 51; case 'c': return 52; case 'v': return 53;
    case 'g': return 54; case 'b': return 55; case 'h': return 56; case 'n': return 57; case 'j': return 58; case 'm': return 59;
    case ',': return 60; case 'q': return 60; case '2': return 61; case 'w': return 62; case '3': return 63; case 'e': return 64;
    case 'r': return 65; case '5': return 66; case 't': return 67; case '6': return 68; case 'y': return 69; case '7': return 70;
    case 'u': return 71; case 'i': return 72;
    default: return -1;
    }
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
