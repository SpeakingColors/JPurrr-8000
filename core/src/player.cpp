#include "jpurrr/player.h"
#include <algorithm>
#include <cmath>

namespace jpurrr {

void Player::setPiece(const PlayView& v) { pending_ = v; hasPending_ = true; if (playing_) restart_ = true; }

void Player::startOver() {
    if (hasPending_) { v_ = pending_; hasPending_ = false; }
    loop_ = v_.beats >= 4.0 ? v_.beats : 4.0;
    lap_ = 0; idx_ = 0; pos_ = 0.0; baseBeats_ = 0.0; baseCount_ = count_ = 0; lastSr_ = lastBpm_ = 0.0;
}

int Player::flush(Out* out, int cap) {
    int k = 0;
    for (int i = 0; i < ns_ && k < cap; ++i) out[k++] = { 0, false, snd_[i].note, 0 };
    ns_ = 0;
    return k;
}

int Player::process(int n, double sr, Out* out, int cap) {
    int k = 0;
    if (stopping_ || restart_) {                      // a stop, or a new piece: end the sounding notes first
        k += flush(out, cap);
        stopping_ = false;
        if (restart_) { startOver(); restart_ = false; }
    }
    if (!playing_ || !v_.ev || v_.n <= 0 || sr <= 0.0 || n <= 0) return k;
    if (sr != lastSr_ || bpm_ != lastBpm_) {                 // tempo or sample rate changed: carry on from where we are
        baseBeats_ = beatsAt(count_); baseCount_ = count_; lastSr_ = sr; lastBpm_ = bpm_; perSample_ = bpm_ / 60.0 / sr;
    }
    const double perSample = perSample_, start = beatsAt(count_), end = beatsAt(count_ + n);
    const int first = k;
    // note ons first, so a short note that starts and ends inside this block is also ended inside it
    for (;;) {
        const PlayEvent& e = v_.ev[idx_];
        const double at = (double) lap_ * loop_ + e.beat;
        if (at >= end - 1e-9) break;
        if (at >= start - 1e-9) {
            const int off = std::max(0, std::min(n - 1, (int) std::floor((at - start) / perSample + 1e-6)));
            int note = (int) e.note + transpose_; note = note < 0 ? 0 : note > 127 ? 127 : note;
            // the same note still sounding ends first
            for (int i = 0; i < ns_; ++i) if (snd_[i].note == note) {
                if (k < cap) out[k++] = { off, false, (std::uint8_t) note, 0 };
                snd_[i] = snd_[--ns_]; break;
            }
            if (k < cap) out[k++] = { off, true, (std::uint8_t) note, e.vel };
            const double offAt = at + std::max(0.02, (double) e.len);
            if (ns_ < kMaxSounding) snd_[ns_++] = { (std::uint8_t) note, offAt, off + 1 };
            else if (k < cap) out[k++] = { std::min(n - 1, off + 1), false, (std::uint8_t) note, 0 };   // no room to remember it: end it again at once
        }
        if (++idx_ >= v_.n) { idx_ = 0; ++lap_; }
    }
    // note offs due in this block (never at or before the note's own start)
    for (int i = 0; i < ns_;) {
        if (snd_[i].offAt < end - 1e-9) {
            const int off = std::max(snd_[i].minOff, (int) std::floor((snd_[i].offAt - start) / perSample + 1e-6));
            if (off <= n - 1) {
                if (k < cap) out[k++] = { off, false, snd_[i].note, 0 };
                snd_[i] = snd_[--ns_]; continue;
            }
        }
        ++i;
    }
    for (int i = 0; i < ns_; ++i) snd_[i].minOff = 0;
    count_ += n; pos_ = end;
    // sort this block's events by offset, offs before ons at the same offset
    std::stable_sort(out + first, out + k, [](const Out& a, const Out& b) { return a.offset != b.offset ? a.offset < b.offset : (!a.on && b.on); });
    return k;
}

// ---- standard MIDI file -------------------------------------------------------------------------------
namespace {
struct Rd {
    const std::uint8_t* d; std::size_t n, p; bool bad = false;
    std::uint32_t u8() { if (p >= n) { bad = true; return 0; } return d[p++]; }
    std::uint32_t u16() { const std::uint32_t a = u8(); return (a << 8) | u8(); }
    std::uint32_t u32() { const std::uint32_t a = u16(); return (a << 16) | u16(); }
    std::uint32_t vlq() { std::uint32_t v = 0; for (int i = 0; i < 4; ++i) { const std::uint32_t b = u8(); v = (v << 7) | (b & 127); if (!(b & 128)) break; } return v; }
};
bool fail(std::string* e, const char* m) { if (e) *e = m; return false; }
}  // namespace

bool parseMidiFile(const std::uint8_t* data, std::size_t size, LoadedPiece& out, std::string* err) {
    out = LoadedPiece{};
    Rd r{ data, size, 0 };
    if (size < 14 || data[0] != 'M' || data[1] != 'T' || data[2] != 'h' || data[3] != 'd') return fail(err, "not a MIDI file");
    r.p = 4; const std::uint32_t hl = r.u32(); const std::uint32_t fmt = r.u16(), ntr = r.u16(), div = r.u16();
    if (hl < 6 || r.bad) return fail(err, "damaged header");
    if (div & 0x8000u || div == 0) return fail(err, "this time format is not supported");
    if (fmt > 1) return fail(err, "only MIDI file format 0 and 1 are supported");
    r.p = 8 + hl;
    struct Raw { double beat, len; std::uint8_t note, vel; };
    std::vector<Raw> raw; double bpm = 0.0;
    for (std::uint32_t t = 0; t < ntr && r.p + 8 <= size; ++t) {
        if (data[r.p] != 'M' || data[r.p + 1] != 'T' || data[r.p + 2] != 'r' || data[r.p + 3] != 'k') break;
        r.p += 4; const std::uint32_t len = r.u32();
        const std::size_t end = std::min<std::size_t>(size, r.p + len);
        std::uint64_t tick = 0; std::uint32_t run = 0;
        struct On { bool on = false; std::uint64_t tick = 0; std::uint8_t vel = 0; } on[16][128];
        while (r.p < end && !r.bad) {
            tick += r.vlq();
            std::uint32_t s = r.u8();
            if (s < 0x80) { if (!run) { r.bad = true; break; } --r.p; s = run; } else if (s < 0xF0) run = s;
            if (s == 0xFF) {
                const std::uint32_t ty = r.u8(), l = r.vlq();
                if (ty == 0x51 && l == 3 && bpm == 0.0 && r.p + 3 <= size) { const std::uint32_t us = (data[r.p] << 16) | (data[r.p + 1] << 8) | data[r.p + 2]; if (us) bpm = 60000000.0 / us; }
                r.p += l;
            } else if (s == 0xF0 || s == 0xF7) { r.p += r.vlq(); }
            else {
                const std::uint32_t c = s & 0xF0, ch = s & 15;
                if (c == 0xC0 || c == 0xD0) { r.u8(); continue; }
                const std::uint32_t a = r.u8() & 127, b = r.u8() & 127;
                if (ch == 9) continue;                                   // channel 10: drums
                if (c == 0x90 && b > 0) { if (!on[ch][a].on) on[ch][a] = { true, tick, (std::uint8_t) b }; }
                else if (c == 0x80 || (c == 0x90 && b == 0)) {
                    On& o = on[ch][a];
                    if (o.on) { raw.push_back({ (double) o.tick / div, std::max(0.05, (double) (tick - o.tick) / div), (std::uint8_t) a, o.vel }); o.on = false; }
                }
            }
            if (r.p > end) break;
        }
        r.p = end; r.bad = false;
    }
    if (raw.empty()) return fail(err, "no notes found");
    std::stable_sort(raw.begin(), raw.end(), [](const Raw& a, const Raw& b) { return a.beat < b.beat; });
    const double t0 = std::floor(raw.front().beat / 4.0) * 4.0;
    double last = 0.0;
    for (const Raw& e : raw) {
        if ((int) out.ev.size() >= kMaxLoadedEvents) { out.truncated = true; break; }
        out.ev.push_back({ (float) (e.beat - t0), (float) e.len, e.note, e.vel });
        last = std::max(last, e.beat - t0 + e.len);
    }
    out.beats = std::max(4.0, std::ceil(last / 4.0 - 1e-6) * 4.0);
    out.bpm = bpm > 0.0 ? bpm : 120.0;
    return true;
}

}  // namespace jpurrr
