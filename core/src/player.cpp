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

std::vector<std::uint8_t> writeMidiFile(const PlayView& piece, double bpm, int transpose, const std::string& name) {
    constexpr int kPpq = 480;
    const double loop = piece.beats > 0 ? piece.beats : 4.0;
    const long loopTicks = std::lround(loop * kPpq);
    struct E { long t; int on; int note; int vel; };
    std::vector<E> ev;
    ev.reserve((size_t) piece.n * 2);
    for (int i = 0; i < piece.n; ++i) {
        const PlayEvent& e = piece.ev[i];
        const long a = std::lround((double) e.beat * kPpq);
        if (a >= loopTicks) continue;
        const long end = std::min(loopTicks, a + std::max(1L, std::lround((double) e.len * kPpq)));
        const int note = std::max(0, std::min(127, (int) e.note + transpose));
        const int vel = std::max(1, std::min(127, (int) e.vel));
        ev.push_back({ a, 1, note, vel });
        ev.push_back({ std::max(a + 1, end), 0, note, 0 });
    }
    std::stable_sort(ev.begin(), ev.end(), [](const E& x, const E& y) { return x.t != y.t ? x.t < y.t : x.on < y.on; });   // note offs before note ons at one tick
    std::vector<std::uint8_t> trk;
    auto vlq = [&](long v) {
        std::uint8_t b[5]; int n = 0; b[n++] = (std::uint8_t) (v & 127);
        while ((v >>= 7) > 0) b[n++] = (std::uint8_t) ((v & 127) | 128);
        while (n > 0) trk.push_back(b[--n]);
    };
    auto meta = [&](long delta, std::uint8_t type, const std::vector<std::uint8_t>& d) {
        vlq(delta); trk.push_back(0xFF); trk.push_back(type); vlq((long) d.size()); trk.insert(trk.end(), d.begin(), d.end());
    };
    const std::string title = "JPurrr-8000 " + name;
    meta(0, 0x03, std::vector<std::uint8_t>(title.begin(), title.begin() + (long) std::min<size_t>(title.size(), 100)));
    meta(0, 0x58, { 4, 2, 24, 8 });
    const long us = std::lround(60000000.0 / std::max(1.0, bpm));
    meta(0, 0x51, { (std::uint8_t) ((us >> 16) & 255), (std::uint8_t) ((us >> 8) & 255), (std::uint8_t) (us & 255) });
    long t = 0;
    for (const E& e : ev) {
        vlq(e.t - t); trk.push_back(e.on ? 0x90 : 0x80); trk.push_back((std::uint8_t) e.note); trk.push_back((std::uint8_t) e.vel); t = e.t;
    }
    meta(std::max(0L, loopTicks - t), 0x2F, {});
    std::vector<std::uint8_t> out = { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, (std::uint8_t) (kPpq >> 8), (std::uint8_t) (kPpq & 255), 'M', 'T', 'r', 'k',
                                      (std::uint8_t) (trk.size() >> 24), (std::uint8_t) (trk.size() >> 16), (std::uint8_t) (trk.size() >> 8), (std::uint8_t) trk.size() };
    out.insert(out.end(), trk.begin(), trk.end());
    return out;
}

}  // namespace jpurrr
