#include "jpurrr/fxchain.h"
#include "jpurrr/trancegate.h"
#include "dsp/fx_common.h"
#include "dsp/fx_drive.h"
#include "dsp/fx_dynamics.h"
#include "dsp/fx_limiter.h"
#include "dsp/fx_stereo.h"
JPURRR_FX_FAST
namespace jpurrr {
using namespace fxdsp;

namespace {
constexpr double kMaxLookaheadSec = 0.0055;

struct Chain {
    DriveStack drive; PocketHpf hpf; LowMono lowMono; MsEq msEq; TransientShaper trans; Tamer tamer; OptoLeveller opto;
    OttSqueeze ott; MultibandDyn mb; GlueComp glue; Widener wid; AutoPan pan; SoftClipper clip; TruePeakLimiter lim;
    Smooth boostDb, trimDb;
    double sr = 48000;
    void prepare(double s) {
        sr = s;
        drive.prepare(s); hpf.prepare(s); lowMono.prepare(s); msEq.prepare(s); trans.prepare(s); tamer.prepare(s); opto.prepare(s);
        ott.prepare(s); mb.prepare(s); glue.prepare(s); wid.prepare(s); pan.prepare(s); clip.prepare(s); lim.prepare(s, kMaxLookaheadSec);
        boostDb.setTime(0.02, s / kSub); trimDb.setTime(0.02, s / kSub);
    }
    void reset() {
        drive.reset(); hpf.reset(); lowMono.reset(); msEq.reset(); trans.reset(); tamer.reset(); opto.reset();
        ott.reset(); mb.reset(); glue.reset(); wid.reset(); pan.reset(); clip.reset(); lim.reset();
        boostDb.snap(boostDb.tgt); trimDb.snap(trimDb.tgt);
    }
};
}  // namespace

struct FxChain::Impl {
    double sr = 48000; Live* live = nullptr;
    int maxBlock = 512;
    int prm[kNumParams]; bool havePrm = false;
    TranceGate gate; int preGated = 0;
    Chain ch;
    // bypass / alignment history of the gated input
    std::vector<float> hist[2]; unsigned hmask = 0, hw = 0;
    Fade qon; bool chainAsleep = true;
    int lat = 0;
    float peakL = 0.f, peakR = 0.f;

    // limLat is TruePeakLimiter::latency(), which already contains the true peak probe delay
    int latencyFor(int limLat) const { return DriveStack::latency() + SoftClipper::latency() + limLat; }

    void applyParams(const int* v) {
        std::memcpy(prm, v, sizeof(prm)); havePrm = true;
        gate.setParams(v);
        qon.set(v[P_qon] != 0);
        Chain& c = ch;
        c.drive.setParams(v[P_qdon] != 0, v[P_qdv], v[P_qdx], v[P_qdf], v[P_qdm]);
        c.hpf.setParams(v[P_qlon] != 0, v[P_qhp]);
        c.lowMono.setParams(v[P_qlon] != 0, v[P_qlm]);
        c.msEq.setParams(v[P_qeon] != 0, v[P_qmp], v[P_qsh], v[P_qai]);
        c.trans.setParams(v[P_qtn] != 0, v[P_qta], v[P_qts]);
        c.tamer.setParams(v[P_qrn] != 0, v[P_qrf], v[P_qrd]);
        c.opto.setParams(v[P_qoon] != 0, v[P_qomd] == 1, v[P_qopr], v[P_qogn], v[P_qomx]);
        const int dn[3] = { v[P_qvdl], v[P_qvdm], v[P_qvdh] }, up[3] = { v[P_qvul], v[P_qvum], v[P_qvuh] }, lv[3] = { v[P_qvl], v[P_qvm], v[P_qvh] };
        c.ott.setParams(v[P_qvon] != 0, dn, up, lv, v[P_qvd], v[P_qvt]);
        MultibandDyn::Band bands[4];
        for (int b = 0; b < 4; ++b) {
            const int* q = v + P_qm1o + 11 * b;       // o u s t r a l b x y z
            MultibandDyn::Band& m = bands[b];
            m.on = q[0] != 0; m.mute = q[1] != 0; m.solo = q[2] != 0; m.thr = q[3]; m.ratio = q[4]; m.att = q[5]; m.rel = q[6];
            m.boost = q[7]; m.limOn = q[8] != 0; m.limRel = q[9]; m.limOut = q[10];
        }
        const int xo[3] = { v[P_qmx1], v[P_qmx2], v[P_qmx3] };
        c.mb.setParams(v[P_qmon] != 0, v[P_qmmx], xo, bands);
        c.glue.setParams(v[P_qcn] != 0, v[P_qcr], v[P_qct], v[P_qca], v[P_qcm]);
        c.wid.setParams(v[P_qwn] != 0, v[P_qwm], v[P_qwa]);
        c.pan.setParams(v[P_qpn] != 0, v[P_qpr], v[P_qpd]);
        c.boostDb.tgt = v[P_qll] / 127.0 * 12.0;
        c.clip.setParams(v[P_qko] != 0, v[P_qka]);
        const int look = (int) std::lround(v[P_qla] * 0.0005 * sr);
        c.lim.setParams(v[P_qlk] != 0, -3.0 + v[P_qlc] / 10.0, 0.010 * std::pow(50.0, v[P_qlr] / 127.0), look);
        c.trimDb.tgt = v[P_qgn];
        lat = latencyFor(c.lim.latency());
    }

    inline void tap(int t, const float* l, const float* r, int n) {
        if (!live) return;
        TapBuf& b = live->tap[t];
        for (int i = 0; i < n; ++i) b.push(0.5f * (l[i] + r[i]));
    }

    // the whole chain on one chunk (n <= kSub), in place; publishes taps when the live pointer is set and pub is true
    void runChain(float* l, float* r, int n, bool pub) {
        Chain& c = ch;
        const bool P = pub && live;
        if (P) { tap(kTapIn, l, r, n); }
        c.drive.process(l, r, n);
        if (P) tap(kTapDrive, l, r, n);
        c.hpf.process(l, r, n);
        c.lowMono.process(l, r, n);
        c.msEq.process(l, r, n);
        c.trans.process(l, r, n);
        c.tamer.process(l, r, n);
        if (P) tap(kTapEq, l, r, n);
        c.opto.process(l, r, n);
        if (P) tap(kTapOpto, l, r, n);
        c.ott.process(l, r, n);
        if (P) tap(kTapOtt, l, r, n);
        c.mb.process(l, r, n);
        if (P) tap(kTapMb, l, r, n);
        c.glue.process(l, r, n);
        if (P) tap(kTapGlue, l, r, n);
        c.wid.process(l, r, n);
        if (P) { for (int i = 0; i < n; ++i) { live->preL.push(l[i]); live->preR.push(r[i]); } }
        c.pan.process(l, r, n);
        // loudness boost, smoothed
        {
            const float g0 = dbToLin((float) c.boostDb.cur), g1 = dbToLin((float) c.boostDb.tick());
            const float inv = 1.f / (float) n;
            for (int i = 0; i < n; ++i) { const float g = g0 + (g1 - g0) * (float) (i + 1) * inv; l[i] *= g; r[i] *= g; }
        }
        c.clip.process(l, r, n);
        c.lim.process(l, r, n);
        {
            const float g0 = dbToLin((float) c.trimDb.cur), g1 = dbToLin((float) c.trimDb.tick());
            const float inv = 1.f / (float) n;
            for (int i = 0; i < n; ++i) { const float g = g0 + (g1 - g0) * (float) (i + 1) * inv; l[i] *= g; r[i] *= g; }
        }
    }

    void wakeChain() {
        ch.reset();
        // replay the last `lat` input samples through the chain so that its internal delays hold real signal
        float tl[kSub], tr[kSub];
        int remaining = lat;
        unsigned pos = hw - (unsigned) remaining;
        while (remaining > 0) {
            const int m = std::min(remaining, (int) kSub);
            for (int i = 0; i < m; ++i) { tl[i] = hist[0][(pos + (unsigned) i) & hmask]; tr[i] = hist[1][(pos + (unsigned) i) & hmask]; }
            runChain(tl, tr, m, false);
            pos += (unsigned) m; remaining -= m;
        }
        chainAsleep = false;
    }

    void publish(const float* l, const float* r, int n) {
        if (!live) return;
        for (int i = 0; i < n; ++i) { peakL = std::max(peakL, std::fabs(l[i])); peakR = std::max(peakR, std::fabs(r[i])); }
    }
};

FxChain::FxChain() : impl_(new Impl) {}
FxChain::~FxChain() = default;

void FxChain::prepare(double sr, int maxBlock) {
    Impl& s = *impl_;
    s.sr = sr; s.maxBlock = maxBlock;
    s.gate.prepare(sr);
    s.ch.prepare(sr);
    unsigned n = 1024; while ((int) n < s.latencyFor(TruePeakProbe::delay() + (int) std::ceil(kMaxLookaheadSec * sr) + 2) + 4 * kSub) n <<= 1;
    for (auto& h : s.hist) h.assign(n, 0.f);
    s.hmask = n - 1; s.hw = 0;
    s.qon.setRate(sr);
    s.havePrm = false;
    int v[kNumParams]; for (int i = 0; i < kNumParams; ++i) v[i] = kParams[i].def;
    s.applyParams(v); s.havePrm = false;
    s.qon.snap(v[P_qon] != 0);
    s.chainAsleep = true;
    s.ch.reset();
    s.lat = s.latencyFor(s.ch.lim.latency());
}
void FxChain::reset() {
    Impl& s = *impl_;
    for (auto& h : s.hist) std::fill(h.begin(), h.end(), 0.f);
    s.hw = 0; s.gate.reset(); s.ch.reset(); s.chainAsleep = true; s.qon.snap(s.qon.tgt > 0.f);
}
void FxChain::setParams(const int* v) {
    Impl& s = *impl_;
    if (s.havePrm && std::memcmp(s.prm, v, sizeof(s.prm)) == 0) return;
    s.applyParams(v);
}
void FxChain::setTempo(double bpm) { impl_->gate.setTempo(bpm); }
void FxChain::setTransport(bool playing, double ppq) { impl_->gate.setTransport(playing, ppq); }
void FxChain::setLive(Live* l) { impl_->live = l; }
int FxChain::latencySamples() const { return impl_->lat; }
void FxChain::restartGate() { impl_->gate.restart(); }

void FxChain::processPreGate(float* l, float* r, int n) {
    Impl& s = *impl_;
    if (!s.havePrm || s.prm[P_gpre] != 0) return;
    ScopedNoDenormals nd;
    s.gate.process(l, r, n);
    s.preGated += n;
}

double FxChain::driveCurve(int drive, int trans, int fuzz, double x) { DriveShape d; d.set(drive, trans, fuzz); return d(x); }

void FxChain::process(float* left, float* right, int n) {
    Impl& s = *impl_;
    if (!s.havePrm) { int v[kNumParams]; for (int i = 0; i < kNumParams; ++i) v[i] = kParams[i].def; s.applyParams(v); }
    ScopedNoDenormals nd;
    const bool gateDone = s.prm[P_gpre] == 0 && s.preGated >= n;
    if (gateDone) s.preGated -= n; else s.preGated = 0;
    if (s.live) { s.peakL = s.peakR = 0.f; }
    for (int off = 0; off < n; off += kSub) {
        const int m = std::min((int) kSub, n - off);
        float* l = left + off; float* r = right + off;
        for (int i = 0; i < m; ++i) {                    // never let NaN or inf in
            if (!(std::fabs(l[i]) < 1e3f)) l[i] = std::isfinite(l[i]) ? (l[i] > 0 ? 1e3f : -1e3f) : 0.f;
            if (!(std::fabs(r[i]) < 1e3f)) r[i] = std::isfinite(r[i]) ? (r[i] > 0 ? 1e3f : -1e3f) : 0.f;
        }
        if (!gateDone) s.gate.process(l, r, m);
        // write the gated input to the history, read the delayed copy
        const unsigned w0 = s.hw;
        for (int i = 0; i < m; ++i) { s.hist[0][(w0 + (unsigned) i) & s.hmask] = l[i]; s.hist[1][(w0 + (unsigned) i) & s.hmask] = r[i]; }
        s.hw += (unsigned) m;
        float bl[kSub], br[kSub];
        for (int i = 0; i < m; ++i) { const unsigned rd = (w0 + (unsigned) i - (unsigned) s.lat) & s.hmask; bl[i] = s.hist[0][rd]; br[i] = s.hist[1][rd]; }
        const float a0 = s.qon.advance(m), a1 = s.qon.cur;
        if (a0 <= 0.f && a1 <= 0.f) {
            s.chainAsleep = true;
            if (s.live) { s.tap(kTapIn, l, r, m); s.tap(kTapOut, bl, br, m); }
            std::memcpy(l, bl, sizeof(float) * (size_t) m); std::memcpy(r, br, sizeof(float) * (size_t) m);
        } else {
            if (s.chainAsleep) {
                // the history now contains this chunk; replay only what came before it
                s.hw -= (unsigned) m; s.wakeChain(); s.hw += (unsigned) m;
            }
            float cl[kSub], cr[kSub];
            std::memcpy(cl, l, sizeof(float) * (size_t) m); std::memcpy(cr, r, sizeof(float) * (size_t) m);
            s.runChain(cl, cr, m, true);
            if (a0 >= 1.f && a1 >= 1.f) {
                std::memcpy(l, cl, sizeof(float) * (size_t) m); std::memcpy(r, cr, sizeof(float) * (size_t) m);
            } else {
                const float inv = 1.f / (float) m;
                for (int i = 0; i < m; ++i) {
                    const float a = a0 + (a1 - a0) * (float) (i + 1) * inv;
                    l[i] = bl[i] + (cl[i] - bl[i]) * a; r[i] = br[i] + (cr[i] - br[i]) * a;
                }
            }
        }
        for (int i = 0; i < m; ++i) {                    // last line of defence
            if (!(std::fabs(l[i]) < 1e4f)) l[i] = 0.f;
            if (!(std::fabs(r[i]) < 1e4f)) r[i] = 0.f;
        }
        if (s.live) {
            s.publish(l, r, m);
            for (int i = 0; i < m; ++i) { s.live->outL.push(l[i]); s.live->outR.push(r[i]); }
            if (a1 > 0.f || a0 > 0.f) s.tap(kTapOut, l, r, m);
        }
    }
    if (s.live) {
        Live& lv = *s.live;
        lv.outPeakL.store(s.peakL, std::memory_order_relaxed); lv.outPeakR.store(s.peakR, std::memory_order_relaxed);
        const bool on = s.qon.cur > 0.f;
        lv.gluGR.store(on ? s.ch.glue.grDb() : 0.f); lv.optoGR.store(on ? s.ch.opto.grDb() : 0.f);
        for (int i = 0; i < 3; ++i) lv.ottGR[i].store(on ? s.ch.ott.grDb(i) : 0.f);
        for (int i = 0; i < 4; ++i) { lv.mbCompGR[i].store(on ? s.ch.mb.compGrDb(i) : 0.f); lv.mbLimGR[i].store(on ? s.ch.mb.limGrDb(i) : 0.f); }
        lv.gatePhase.store(s.prm[P_gon] ? s.gate.step() : -1);
    }
}

}  // namespace jpurrr
