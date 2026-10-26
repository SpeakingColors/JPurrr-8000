// JPurrr-8000 sound engine: voices, allocation, key modes, arpeggiator, controllers.
// Voice DSP lives in dsp/synth_voice.*, primitives in dsp/synth_dsp.h.
#include "jpurrr/synth.h"
#include "dsp/synth_voice.h"
#include <algorithm>
#include <vector>
#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace jpurrr {
using namespace dsp;

namespace {
constexpr int kMaxPoly = 128;
constexpr int kPool = kMaxPoly + 16;           // spare voices let stolen ones fade out
constexpr int kStack = 32;                     // held notes remembered per part in solo modes
constexpr int kMaxUnison = 16;
constexpr int kArpMax = 64;
constexpr float kMasterTrim = 0.89f;           // calibrated: a chord of the default patch peaks near -6 dBFS

// Flush denormals to zero for the duration of a process() call.
struct NoDenormals {
#if defined(__SSE__) || defined(_M_X64)
    unsigned old;
    NoDenormals() : old(_mm_getcsr()) { _mm_setcsr(old | 0x8040u); }
    ~NoDenormals() { _mm_setcsr(old); }
#endif
};

struct Held { int note, vel, ch; };

struct ArpNote { int note, vel, ch; bool down; };
struct Arp {
    ArpNote list[kArpMax]; int n = 0;
    int pos = 0;                 // sequence position
    int curNote = -1, curCh = 0; // sounding note
    double gateLeft = 0;         // samples until the gate closes (0 = closed)
    void clear() { n = 0; pos = 0; }
};

struct Part {
    PatchParams tgt, cur;
    int v[kNumParams] = {};
    bool haveV = false;
    Held stack[kStack]; int nStack = 0;
    int soloIds[kMaxUnison] = {}; int nSolo = 0;
    double lastSemis = 60.0; bool hasLast = false;
    double lfo2Ph = 0.0;
    Arp arp;
};

const double kArpStep[6] = { 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 0.125 };   // beats for 1/4 1/6 1/8 1/12 1/16 1/32
}  // namespace

struct Synth::Impl {
    double sr = 48000.0; int maxBlock = 512; int osFactor = 2; bool prepared = false;
    Live* live = nullptr;
    std::vector<Voice> voices;
    Part parts[2];
    int mainV[kNumParams] = {}; bool haveMain = false;
    int partV[2][kNumParams] = {}; bool havePartV[2] = { false, false };
    bool snapParts = true;
    uint32_t ageCounter = 0;
    int cbPos = 0;
    BlockCtl ctl;
    // mode (from main params)
    int keyMode = 0, splitPoint = 60, voiceCount = 10, voiceAssign = 3;
    float partDetune = 0.f;
    // controllers
    float midiBend = 0.f, uiBend = 0.f, bendCur = 0.f;
    float ccMod = 0.f, uiMod = 0.f, pressure = 0.f, modCur = 0.f;
    bool pedal[16] = {};
    bool keyDown[16][128] = {}; int keysDown = 0;
    float volTgt = 0.787f, volCur = 0.787f;
    // clocks
    double hostBpm = 0.0, paramBpm = 128.0;
    bool playing = false; bool prevPlaying = false;
    double beatPos = 0.0;            // quarter notes, follows the host while it plays
    double stepPhase = 0.0;          // 0..1 inside the current arpeggiator step
    bool stepNow = false;            // fire a step at the start of the next chunk
    // arpeggiator settings (from main params)
    bool arpOn = false, arpHold = false; int arpMode = 0, arpRange = 1, arpPat = 4, arpDest = 0;
    bool arpWasOn = false;
    dsp::Rng arpRng;
    float smoothA = 0.f, bendA = 0.f;
    // output duck around structural parameter changes (waves, filter type, ...) so they do not click
    int duckState = 0; float duckGain = 1.f, duckStep = 0.01f; int duckHold = 0;
    std::vector<float> tmpL, tmpR;

    // ------------------------------------------------------------ setup
    void prepare(double s, int mb) {
        sr = s; maxBlock = std::max(1, mb);
        osFactor = sr < 80000.0 ? 2 : 1;                   // the hardware probably ran at 88.2 kHz inside (unverified)
        voices.assign(kPool, Voice());
        for (auto& v : voices) v.init(sr, osFactor);
        duckStep = (float) (1.0 / (0.0015 * sr));
        smoothA = 1.f - std::exp(-(float) kControlBlock / (float) (sr * 0.008));
        bendA = 1.f - std::exp(-(float) kControlBlock / (float) (sr * 0.004));
        prepared = true;
        reset();
    }
    void reset() {
        for (auto& v : voices) v.reset();
        for (auto& p : parts) { p.nStack = 0; p.nSolo = 0; p.hasLast = false; p.lfo2Ph = 0.0; p.arp.clear(); p.arp.curNote = -1; p.arp.gateLeft = 0; }
        std::memset(keyDown, 0, sizeof keyDown); keysDown = 0;
        std::memset(pedal, 0, sizeof pedal);
        ageCounter = 0; cbPos = 0; stepPhase = 0.0; stepNow = false; beatPos = 0.0; duckState = 0; duckGain = 1.f; duckHold = 0;
        midiBend = ccMod = pressure = 0.f; bendCur = modCur = 0.f;
        arpRng.seed(12345); snapParts = true; arpWasOn = false;
        ctl = BlockCtl();
        volCur = volTgt;
    }

    // ------------------------------------------------------------ parameters
    void rederive() {
        if (!haveMain) return;
        const int* base = mainV;
        keyMode = std::min(2, std::max(0, base[P_km]));
        splitPoint = base[P_sp]; voiceCount = std::min(kMaxPoly, std::max(8, base[P_vc])); voiceAssign = std::min(6, std::max(0, base[P_vas]));
        partDetune = (float) base[P_pdet];
        volTgt = base[P_vol] / 127.f;
        paramBpm = base[P_atp];
        arpOn = base[P_aon] != 0; arpHold = base[P_ahold] != 0; arpMode = base[P_amd]; arpRange = 1 + std::min(3, std::max(0, base[P_arg]));
        arpPat = base[P_apt]; arpDest = base[P_adst];
        uiBend = base[P_bend] >= 0 ? base[P_bend] / 63.f : base[P_bend] / 64.f;
        uiMod = base[P_mod] / 127.f;
        for (int p = 0; p < 2; ++p) {
            const int* pv = (keyMode != 0 && havePartV[p]) ? partV[p] : base;
            const float det = keyMode == 0 ? 0.f : (p == 0 ? partDetune * 0.5f : -partDetune * 0.5f);
            parts[p].tgt.derive(pv, det);
            parts[p].tgt.vc = voiceCount;
        }
        if (snapParts) { parts[0].cur = parts[0].tgt; parts[1].cur = parts[1].tgt; volCur = volTgt; snapParts = false; }
        else for (int p = 0; p < 2; ++p) {                                 // switches and times apply at once, knobs glide
            if (!partSounding(p)) parts[p].cur = parts[p].tgt;
            else parts[p].cur.smoothToward(parts[p].tgt, 0.f, false);
        }
    }
    bool partSounding(int p) const { for (const auto& v : voices) if (v.sounding() && v.part == p) return true; return false; }
    int partChannel(int p) const {
        const int* pv = (keyMode != 0 && havePartV[p]) ? partV[p] : mainV;
        return pv[P_pch];                                  // 0 = OFF = any channel (omni)
    }
    bool partAccepts(int p, int ch) const { const int c = partChannel(p); return c == 0 || c == ch + 1; }

    // ------------------------------------------------------------ voices
    int countParts(int p, int& total) const {
        int cnt = 0; total = 0;
        for (const auto& v : voices) if (v.sounding()) { ++total; if (v.part == p) ++cnt; }
        return cnt;
    }
    int partLimit(int p) const {
        if (keyMode == 0) return voiceCount;
        const int lower = std::min(voiceCount - 1, std::max(1, (int) std::lround(voiceCount * (8 - voiceAssign) / 10.0)));
        return p == 1 ? lower : std::max(1, voiceCount - lower);
    }
    Voice* findIdle() {
        for (auto& v : voices) if (v.st == VState::Idle) return &v;
        return nullptr;
    }
    // Quietest released voice first, then the oldest held one.
    Voice* pickVictim(int preferPart) {
        Voice* best = nullptr; float bestLevel = 1e9f;
        for (auto& v : voices) {
            if (v.st != VState::Released) continue;
            if (preferPart >= 0 && v.part != preferPart) continue;
            if (v.level() < bestLevel) { bestLevel = v.level(); best = &v; }
        }
        if (best) return best;
        uint32_t oldest = 0xFFFFFFFFu;
        for (auto& v : voices) {
            if (v.st != VState::Held || v.solo) continue;
            if (preferPart >= 0 && v.part != preferPart) continue;
            if (v.age < oldest) { oldest = v.age; best = &v; }
        }
        if (best) return best;
        for (auto& v : voices) if (v.st == VState::Held && (preferPart < 0 || v.part == preferPart) && v.age < oldest) { oldest = v.age; best = &v; }
        return best;
    }
    Voice* allocate(int p) {
        int total; int cnt = countParts(p, total);
        const int lim = partLimit(p);
        int guard = kPool;
        while ((cnt >= lim || total >= voiceCount) && guard-- > 0) {
            Voice* vic = pickVictim(cnt >= lim ? p : -1);
            if (!vic) vic = pickVictim(-1);
            if (!vic) break;
            vic->fadeOut();
            cnt = countParts(p, total);
        }
        Voice* v = findIdle();
        if (!v) {                                           // pool exhausted by fading voices: cut the oldest hard
            uint32_t oldest = 0xFFFFFFFFu;
            for (auto& x : voices) if (x.age < oldest) { oldest = x.age; v = &x; }
            if (v) v->reset();
        }
        return v;
    }
    uint32_t nextSeed() { return 0x2545F491u + 7919u * (++ageCounter); }
    void startVoice(Voice& v, int p, int ch, int note, int vel, bool glideFrom, int uniIdx, int uniN, bool solo) {
        Part& pt = parts[p];
        v.uniIdx = uniIdx; v.uniN = uniN; v.uniGain = uniN > 1 ? 1.f / std::sqrt((float) uniN) : 1.f; v.solo = solo;
        const uint32_t seed = nextSeed();
        v.start(pt.cur, p, ch, note, vel, ageCounter, seed, pt.lastSemis, glideFrom && pt.hasLast && pt.cur.porta);
        v.control(pt.cur, ctl, true);
    }

    // ---- polyphonic
    void polyOn(int p, int ch, int note, int vel) {
        Voice* v = allocate(p);
        if (!v) return;
        startVoice(*v, p, ch, note, vel, true, 0, 1, false);
        parts[p].lastSemis = note; parts[p].hasLast = true;
    }
    void polyOff(int p, int ch, int note) {
        for (auto& v : voices)
            if (v.st == VState::Held && !v.solo && v.part == p && v.note == note && v.ch == ch && !v.sustained) {
                if (pedal[ch]) v.sustained = true; else v.release();
            }
    }

    // ---- mono / legato / unison
    void soloOn(int p, int ch, int note, int vel) {
        Part& pt = parts[p]; const PatchParams& P = pt.cur;
        // note stack (last note priority)
        for (int i = 0; i < pt.nStack; ++i) if (pt.stack[i].note == note) { for (int j = i; j + 1 < pt.nStack; ++j) pt.stack[j] = pt.stack[j + 1]; --pt.nStack; break; }
        const bool anyHeld = pt.nStack > 0;
        if (pt.nStack == kStack) { for (int j = 0; j + 1 < kStack; ++j) pt.stack[j] = pt.stack[j + 1]; --pt.nStack; }
        pt.stack[pt.nStack++] = { note, vel, ch };
        const int nUni = P.uni ? std::min(kMaxUnison, std::min(voiceCount, kMaxUnison)) : 1;
        // keep only group voices that are still alive
        int alive = 0;
        for (int i = 0; i < pt.nSolo; ++i) {
            Voice& v = voices[(size_t) pt.soloIds[i]];
            if (v.solo && v.part == p && v.sounding()) pt.soloIds[alive++] = pt.soloIds[i];
        }
        pt.nSolo = alive;
        const bool legatoJoin = P.leg && anyHeld && pt.nSolo > 0;
        for (int i = 0; i < pt.nSolo; ++i) {
            Voice& v = voices[(size_t) pt.soloIds[i]];
            v.setNote(note, vel);
            v.uniN = nUni; v.uniGain = nUni > 1 ? 1.f / std::sqrt((float) nUni) : 1.f;
            if (!legatoJoin) v.retrigger(P, vel, P.analogEnv);
            v.ch = ch;
        }
        while (pt.nSolo < nUni) {
            Voice* v = allocate(p);
            if (!v) break;
            startVoice(*v, p, ch, note, vel, true, pt.nSolo, nUni, true);
            pt.soloIds[pt.nSolo++] = (int) (v - voices.data());
        }
        // stack positions so the spread is symmetric even when voices were stolen
        for (int i = 0; i < pt.nSolo; ++i) voices[(size_t) pt.soloIds[i]].uniIdx = i;
        pt.lastSemis = note; pt.hasLast = true;
    }
    void soloOff(int p, int ch, int note) {
        Part& pt = parts[p]; const PatchParams& P = pt.cur;
        int idx = -1;
        for (int i = 0; i < pt.nStack; ++i) if (pt.stack[i].note == note) { idx = i; break; }
        if (idx < 0) return;
        const bool wasTop = idx == pt.nStack - 1;
        for (int j = idx; j + 1 < pt.nStack; ++j) pt.stack[j] = pt.stack[j + 1];
        --pt.nStack;
        if (pt.nStack == 0) {
            for (int i = 0; i < pt.nSolo; ++i) {
                Voice& v = voices[(size_t) pt.soloIds[i]];
                if (v.st == VState::Held) { if (pedal[ch]) v.sustained = true; else v.release(); }
            }
        } else if (wasTop) {
            const Held& h = pt.stack[pt.nStack - 1];
            for (int i = 0; i < pt.nSolo; ++i) {
                Voice& v = voices[(size_t) pt.soloIds[i]];
                if (!v.sounding()) continue;
                v.setNote(h.note, h.vel);
                if (!P.leg) v.retrigger(P, h.vel, P.analogEnv);
            }
            pt.lastSemis = h.note;
        }
    }

    // ---- part level dispatch
    void partOn(int p, int ch, int note, int vel) {
        if (!partSounding(p)) parts[p].cur = parts[p].tgt;       // nothing to glide from: a new note gets the settings exactly
        const PatchParams& P = parts[p].cur;
        if (P.mono || P.leg || P.uni) soloOn(p, ch, note, vel); else polyOn(p, ch, note, vel);
    }
    void partOff(int p, int ch, int note) {
        polyOff(p, ch, note);
        if (parts[p].nStack > 0) soloOff(p, ch, note);
    }

    // ---- arpeggiator
    bool arpActiveFor(int p) const {
        if (!arpOn) return false;
        if (keyMode == 0) return p == 0;
        return arpDest == 0 || (arpDest == 1 && p == 1) || (arpDest == 2 && p == 0);
    }
    double bpm() const { return hostBpm > 0.0 ? hostBpm : std::min(250.0, std::max(20.0, paramBpm)); }
    double stepBeats() const { return kArpStep[arpPat >= 0 && arpPat < 6 ? arpPat : 4]; }   // other beat patterns need data we do not have: 1/16
    void arpAdd(int p, int ch, int note, int vel) {
        Arp& a = parts[p].arp;
        bool anyDown = false;
        for (int i = 0; i < a.n; ++i) if (a.list[i].down) anyDown = true;
        if (arpHold && !anyDown) { a.n = 0; a.pos = 0; }
        for (int i = 0; i < a.n; ++i) if (a.list[i].note == note) { a.list[i].vel = vel; a.list[i].down = true; a.list[i].ch = ch; return; }
        if (a.n < kArpMax) a.list[a.n++] = { note, vel, ch, true };
        if (!playing && a.n == 1 && a.curNote < 0) stepNow = true, stepPhase = 0.0;   // free running: start at once
    }
    void arpRemove(int p, int note) {
        Arp& a = parts[p].arp;
        for (int i = 0; i < a.n; ++i) if (a.list[i].note == note) {
            if (arpHold) a.list[i].down = false;
            else { for (int j = i; j + 1 < a.n; ++j) a.list[j] = a.list[j + 1]; --a.n; }
            return;
        }
    }
    void arpStop(int p) {
        Arp& a = parts[p].arp;
        if (a.curNote >= 0) { partOff(p, a.curCh, a.curNote); a.curNote = -1; }
        a.gateLeft = 0;
    }
    void arpStepFire(int p) {
        Arp& a = parts[p].arp;
        if (a.curNote >= 0) { partOff(p, a.curCh, a.curNote); a.curNote = -1; }
        if (a.n == 0) return;
        int sorted[kArpMax]; int vels[kArpMax]; int chs[kArpMax];
        for (int i = 0; i < a.n; ++i) { sorted[i] = i; }
        std::sort(sorted, sorted + a.n, [&](int x, int y) { return a.list[x].note < a.list[y].note; });
        const int n = a.n, m = n * arpRange;
        for (int i = 0; i < n; ++i) { vels[i] = a.list[sorted[i]].vel; chs[i] = a.list[sorted[i]].ch; }
        int seqPos;
        switch (arpMode) {
            case 1: seqPos = m - 1 - (a.pos % m); break;                             // DOWN
            case 2: { const int len = m > 1 ? 2 * (m - 1) : 1; const int q = a.pos % len; seqPos = q < m ? q : len - q; break; }   // UP & DOWN
            case 3: seqPos = (int) (arpRng.uni() * m); if (seqPos >= m) seqPos = m - 1; break;          // RANDOM
            default: seqPos = a.pos % m; break;                                        // UP (RPS mode plays UP: no pattern data)
        }
        ++a.pos;
        const int oct = seqPos / n, idx = seqPos % n;
        const int note = std::min(127, a.list[sorted[idx]].note + 12 * oct);
        a.curNote = note; a.curCh = chs[idx];
        partOn(p, chs[idx], note, vels[idx]);
        a.gateLeft = 0.6 * stepBeats() * 60.0 / bpm() * sr;
    }
    // samples until the next arpeggiator event (step or gate end), or a large number
    int arpNextEvent() const {
        if (!arpOn) return 1 << 30;
        double best = 1e18;
        const double spb = 60.0 / bpm() * sr;                 // samples per beat
        best = std::ceil((1.0 - stepPhase) * stepBeats() * spb);
        for (int p = 0; p < 2; ++p) if (parts[p].arp.curNote >= 0 && parts[p].arp.gateLeft > 0) best = std::min(best, std::ceil(parts[p].arp.gateLeft));
        if (stepNow) best = 0;
        return (int) std::max(0.0, std::min(best, 1e9));
    }
    void arpAdvance(int m) {
        const double spb = 60.0 / bpm() * sr;
        if (arpOn) stepPhase += m / (spb * stepBeats());
        for (int p = 0; p < 2; ++p) {
            Arp& a = parts[p].arp;
            if (a.curNote >= 0 && a.gateLeft > 0) { a.gateLeft -= m; if (a.gateLeft <= 0.0) { partOff(p, a.curCh, a.curNote); a.curNote = -1; a.gateLeft = 0; } }
        }
    }
    void arpFire() {
        if (!arpOn) return;
        bool fire = false;
        if (stepNow) { fire = true; stepNow = false; stepPhase = 0.0; }
        else if (stepPhase >= 1.0 - 1e-9) { fire = true; stepPhase -= std::floor(stepPhase); }
        if (!fire) return;
        for (int p = 0; p < 2; ++p) if (arpActiveFor(p)) arpStepFire(p);
    }
    void arpSettingsChanged() {
        if (arpOn != arpWasOn) {
            if (!arpOn) { for (int p = 0; p < 2; ++p) { arpStop(p); parts[p].arp.clear(); } }
            arpWasOn = arpOn;
        }
        if (!arpHold) for (int p = 0; p < 2; ++p) {                      // hold released: drop notes whose key is up
            Arp& a = parts[p].arp;
            for (int i = 0; i < a.n;) if (!a.list[i].down) { for (int j = i; j + 1 < a.n; ++j) a.list[j] = a.list[j + 1]; --a.n; } else ++i;
        }
    }

    // ------------------------------------------------------------ events
    void noteOn(int ch, int note, int vel) {
        if (!prepared) return;
        ch &= 15; note = std::min(127, std::max(0, note)); vel = std::min(127, std::max(1, vel));
        bool use[2] = { false, false };
        if (keyMode == 0) use[0] = partAccepts(0, ch);
        else if (keyMode == 1) { use[0] = partAccepts(0, ch); use[1] = partAccepts(1, ch); }
        else { const int p = note < splitPoint ? 1 : 0; use[p] = partAccepts(p, ch); }
        if (!use[0] && !use[1]) return;
        if (!keyDown[ch][note]) { keyDown[ch][note] = true; ++keysDown; }
        if (live) {
            live->noteOnCount.fetch_add(1, std::memory_order_relaxed);
            live->lastNote.store(note, std::memory_order_relaxed); live->lastVel.store(vel, std::memory_order_relaxed);
        }
        for (int p = 0; p < 2; ++p) if (use[p]) {
            if (arpActiveFor(p)) arpAdd(p, ch, note, vel); else partOn(p, ch, note, vel);
        }
    }
    void noteOff(int ch, int note) {
        if (!prepared) return;
        ch &= 15; note = std::min(127, std::max(0, note));
        if (keyDown[ch][note]) {
            keyDown[ch][note] = false;
            if (--keysDown <= 0) { keysDown = 0; if (live) live->noteOffAllCount.fetch_add(1, std::memory_order_relaxed); }
        }
        for (int p = 0; p < 2; ++p) { arpRemove(p, note); partOff(p, ch, note); }
    }
    void releaseAll(bool hard) {
        for (auto& v : voices) {
            if (hard) v.fadeOut();
            else if (v.st == VState::Held) v.release();
            v.sustained = false;
        }
        for (auto& p : parts) { p.nStack = 0; p.nSolo = 0; p.arp.clear(); p.arp.curNote = -1; p.arp.gateLeft = 0; }
        std::memset(keyDown, 0, sizeof keyDown);
        if (keysDown > 0 && live) live->noteOffAllCount.fetch_add(1, std::memory_order_relaxed);
        keysDown = 0;
    }
    void pedalChange(int ch, bool down) {
        pedal[ch] = down;
        if (down) return;
        for (auto& v : voices) if (v.st == VState::Held && v.sustained && v.ch == ch) { v.sustained = false; if (!isSoloHeld(v)) v.release(); }
        for (int p = 0; p < 2; ++p) if (parts[p].nStack == 0)
            for (int i = 0; i < parts[p].nSolo; ++i) { Voice& v = voices[(size_t) parts[p].soloIds[i]]; if (v.st == VState::Held && v.sustained && v.ch == ch) { v.sustained = false; v.release(); } }
    }
    bool isSoloHeld(const Voice& v) const { return v.solo && parts[v.part].nStack > 0; }

    // ------------------------------------------------------------ render
    void controlBlock() {
        for (int p = 0; p < 2; ++p) {
            if (parts[p].cur.structKey() != parts[p].tgt.structKey()) {
                if (!partSounding(p)) parts[p].cur.copyStruct(parts[p].tgt);
                else if (duckState == 0) duckState = 1;
            }
            parts[p].cur.smoothToward(parts[p].tgt, smoothA, false);
            parts[p].lfo2Ph += kControlBlock / sr * parts[p].cur.l2Hz;
            parts[p].lfo2Ph -= std::floor(parts[p].lfo2Ph);
            ctl.l2[p] = (float) std::sin(kTwoPi * parts[p].lfo2Ph);
        }
        const float bt = std::min(1.f, std::max(-1.f, midiBend + uiBend));
        bendCur += (bt - bendCur) * bendA;
        const float mt = std::min(1.f, std::max(std::max(ccMod, uiMod), pressure));
        modCur += (mt - modCur) * bendA;
        ctl.bend = bendCur; ctl.mod = modCur;
        ctl.bpm = bpm(); ctl.playing = playing; ctl.ppq = beatPos;
        for (auto& v : voices) if (v.sounding()) v.control(parts[v.part].cur, ctl, false);
    }
    // master stage for k samples: volume, trim, duck, limiter
    void masterStage(float* L, float* R, int k) {
        const float va = 1.f - std::exp(-1.f / (float) (sr * 0.005));
        for (int i = 0; i < k; ++i) {
            volCur += (volTgt - volCur) * va;
            if (duckState == 1) {
                duckGain -= duckStep;
                if (duckGain <= 0.f) { duckGain = 0.f; for (auto& pt : parts) pt.cur.copyStruct(pt.tgt); duckState = 3; duckHold = 2 * kControlBlock; }
            } else if (duckState == 3) { if (--duckHold <= 0) duckState = 2; }
            else if (duckState == 2) { duckGain += duckStep; if (duckGain >= 1.f) { duckGain = 1.f; duckState = 0; } }
            const float g = volCur * kMasterTrim * duckGain;
            float l = L[i] * g, r = R[i] * g;
            if (!std::isfinite(l)) l = 0.f;
            if (!std::isfinite(r)) r = 0.f;
            L[i] = softLimit(l); R[i] = softLimit(r);
        }
    }
    void renderChunk(float* L, float* R, int m) {
        std::memset(L, 0, sizeof(float) * (size_t) m); std::memset(R, 0, sizeof(float) * (size_t) m);
        int done = 0;
        while (done < m) {
            if (cbPos == 0) controlBlock();
            const int k = std::min(m - done, kControlBlock - cbPos);
            for (auto& v : voices) if (v.st != VState::Idle) v.render(k, L + done, R + done);
            masterStage(L + done, R + done, k);
            cbPos += k; if (cbPos >= kControlBlock) cbPos = 0;
            done += k;
        }
        if (playing) beatPos += m * bpm() / 60.0 / sr;
    }
    void process(float* L, float* R, int n) {
        NoDenormals nd;
        if (!prepared) { std::memset(L, 0, sizeof(float) * (size_t) n); std::memset(R, 0, sizeof(float) * (size_t) n); return; }
        rederive();
        arpSettingsChanged();
        if (playing && !prevPlaying) { stepPhase = 0.0; if (arpOn) stepNow = true; }
        prevPlaying = playing;
        while (n > 0) {
            arpFire();
            int m = n;
            const int ev = arpNextEvent();
            if (ev < m) m = ev;
            if (m < 1) m = 1;
            renderChunk(L, R, m);
            arpAdvance(m);
            L += m; R += m; n -= m;
        }
        arpFire();
        if (live) {
            int act = 0; for (const auto& v : voices) if (v.st != VState::Idle) ++act;
            live->activeVoices.store(act, std::memory_order_relaxed);
        }
    }
};

// ====================================================================== public API
Synth::Synth() : impl_(new Impl) {}
Synth::~Synth() = default;
void Synth::prepare(double sr, int mb) { impl_->prepare(sr, mb); }
void Synth::reset() { impl_->reset(); }
void Synth::setParams(const int* v) {
    Impl& s = *impl_;
    if (!v) return;
    if (!s.haveMain || std::memcmp(s.mainV, v, sizeof(int) * kNumParams) != 0) {
        std::memcpy(s.mainV, v, sizeof(int) * kNumParams);
        for (int i = 0; i < kNumParams; ++i) s.mainV[i] = std::min(kParams[i].max, std::max(kParams[i].min, s.mainV[i]));
        s.haveMain = true;
        s.rederive();
    }
}
void Synth::setPartParams(int part, const int* v) {
    Impl& s = *impl_;
    if (part < 0 || part > 1) return;
    if (!v) { s.havePartV[part] = false; s.rederive(); return; }
    if (!s.havePartV[part] || std::memcmp(s.partV[part], v, sizeof(int) * kNumParams) != 0) {
        std::memcpy(s.partV[part], v, sizeof(int) * kNumParams);
        for (int i = 0; i < kNumParams; ++i) s.partV[part][i] = std::min(kParams[i].max, std::max(kParams[i].min, s.partV[part][i]));
        s.havePartV[part] = true;
        s.rederive();
    }
}
void Synth::noteOn(int ch, int note, int vel) { if (vel <= 0) impl_->noteOff(ch, note); else impl_->noteOn(ch, note, vel); }
void Synth::noteOff(int ch, int note) { impl_->noteOff(ch, note); }
void Synth::allNotesOff() { impl_->releaseAll(false); }
void Synth::pitchBend(int, int value14) {
    const int v = std::min(8191, std::max(-8192, value14));
    impl_->midiBend = v >= 0 ? v / 8191.f : v / 8192.f;
}
void Synth::controller(int ch, int cc, int value) {
    Impl& s = *impl_;
    ch &= 15; value = std::min(127, std::max(0, value));
    switch (cc) {
        case 1: s.ccMod = value / 127.f; break;
        case 64: s.pedalChange(ch, value >= 64); break;
        case 120: s.releaseAll(true); break;
        case 121: s.midiBend = 0.f; s.ccMod = 0.f; s.pressure = 0.f; for (int c = 0; c < 16; ++c) if (s.pedal[c]) s.pedalChange(c, false); break;
        case 123: s.releaseAll(false); break;
        default: break;
    }
}
void Synth::channelPressure(int, int value) { impl_->pressure = std::min(127, std::max(0, value)) / 127.f; }
void Synth::setTempo(double bpm) { impl_->hostBpm = bpm > 1.0 ? bpm : 0.0; }
void Synth::setTransport(bool playing, double ppq) {
    Impl& s = *impl_;
    s.playing = playing;
    if (playing) {
        s.beatPos = ppq;
        // follow the host grid: resync the step phase only when it drifted
        const double sb = s.stepBeats();
        double hp = ppq / sb; hp -= std::floor(hp);
        double d = hp - s.stepPhase; d -= std::floor(d + 0.5);
        if (s.prevPlaying && std::fabs(d) > 0.01) s.stepPhase = hp;
        else if (!s.prevPlaying) s.stepPhase = hp;
    }
}
void Synth::setLive(Live* l) { impl_->live = l; }
void Synth::process(float* l, float* r, int n) { impl_->process(l, r, n); }
int Synth::activeVoices() const { int c = 0; for (const auto& v : impl_->voices) if (v.st != VState::Idle) ++c; return c; }
int Synth::heldVoices() const { int c = 0; for (const auto& v : impl_->voices) if (v.st == VState::Held) ++c; return c; }
int Synth::oversampling() const { return impl_->osFactor; }

}  // namespace jpurrr
