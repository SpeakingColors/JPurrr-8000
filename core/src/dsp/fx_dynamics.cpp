#include "fx_dynamics.h"
JPURRR_FX_FAST
namespace jpurrr {
namespace fxdsp {

static inline float peakOf(float a, float b) { return std::max(std::fabs(a), std::fabs(b)); }

// ====================================================================== transient shaper
// Two peak followers (fast: 1 ms attack, 20 ms release; slow: 30 ms attack, 200 ms release). Their difference in dB is
// level independent: positive at note starts (ATTACK acts), negative while a note dies away (SUSTAIN acts).
// 6 dB of difference gives the full effect. Gain moves are smoothed with a 1 ms time constant.
void TransientShaper::prepare(double sr) {
    prepareBase(sr);
    fast_.set(0.001, 0.020, sr); slow_.set(0.030, 0.200, sr);
    kg_ = 1.0 - std::exp(-1.0 / (0.001 * sr));
    resetState();
}
void TransientShaper::resetState() { fast_.clear(); slow_.clear(); gain_ = 1.0; }
void TransientShaper::setParams(bool on, int attack, int sustain) { setOn(on); aDb_ = attack / 2.0; sDb_ = sustain / 2.0; }
void TransientShaper::run(float* l, float* r, int n) {
    for (int i = 0; i < n; ++i) {
        const float lvl = peakOf(l[i], r[i]);
        const float f = (float) fast_.run(lvl), s = (float) slow_.run(lvl);
        float gdb = 0.f;
        if (s > 1e-5f && f > 1e-7f) {
            const float d = linToDb(f / s);
            gdb = (float) aDb_ * std::min(1.f, std::max(0.f, d * (1.f / 6.f))) + (float) sDb_ * std::min(1.f, std::max(0.f, -d * (1.f / 6.f)));
        }
        const double tgt = (aDb_ == 0.0 && sDb_ == 0.0) ? 1.0 : (double) dbToLin(gdb);
        gain_ += (tgt - gain_) * kg_;
        l[i] = (float) (l[i] * gain_); r[i] = (float) (r[i] * gain_);
    }
}

// ====================================================================== resonance tamer
// Dynamic EQ. A narrow band pass (Q 2.5) and a wide one (Q 0.5) on the mid signal give two envelopes; the narrow one sticking out
// of the wide one (calibrated so that white noise gives 0 dB) opens a bell cut at the focus frequency, at most DEPTH * 6 dB deep.
static constexpr double kTamerCal = 7.0;           // dB, narrow over wide for white noise (measured, see tests)
void Tamer::prepare(double sr) {
    prepareBase(sr);
    b_[0].f0 = 4000.0; b_[1].f0 = 8000.0;
    for (auto& b : b_) {
        Rbj d(b.f0, sr);
        b.narrowC = d.bandpass(2.5); b.wideC = d.bandpass(0.5);
        b.eN.set(0.002, 0.040, sr); b.eW.set(0.050, 0.200, sr);
        b.kAtk = 1.0 - std::exp(-(double) kSub / (0.003 * sr)); b.kRel = 1.0 - std::exp(-(double) kSub / (0.080 * sr));
    }
    resetState();
}
void Tamer::resetState() {
    for (auto& b : b_) { b.narrow.clear(); b.wide.clear(); b.eN.clear(); b.eW.clear(); b.bell[0].clear(); b.bell[1].clear(); b.redDb = 0; }
}
void Tamer::wake() {}
void Tamer::setParams(bool on, int focus, int depth) {
    setOn(on);
    use_[0] = focus != 1; use_[1] = focus != 0;
    maxDb_ = depth / 127.0 * 6.0;
}
void Tamer::run(float* l, float* r, int n) {
    BqCoef bc[2]; bool act[2];
    for (int k = 0; k < 2; ++k) {
        Band& b = b_[k];
        double tgt = 0.0;
        if (use_[k] && b.eW.env > 1e-6) {
            const double ex = 20.0 * std::log10(std::max(b.eN.env, 1e-9) / b.eW.env) + kTamerCal;
            tgt = maxDb_ * clampd((ex - 3.0) / 6.0, 0.0, 1.0);
        }
        b.redDb += (tgt - b.redDb) * (tgt > b.redDb ? b.kAtk : b.kRel);
        act[k] = b.redDb > 0.005;
        if (act[k]) bc[k] = Rbj(b.f0, sr_).peaking(2.5, -b.redDb);
        else { b.bell[0].clear(); b.bell[1].clear(); }       // a bell that is not running holds no stale state when it starts again
    }
    for (int i = 0; i < n; ++i) {
        const double sc = 0.5 * ((double) l[i] + (double) r[i]);
        double xl = l[i], xr = r[i];
        for (int k = 0; k < 2; ++k) {
            Band& b = b_[k];
            b.eN.run(std::fabs(b.narrow.run(b.narrowC, sc)));
            b.eW.run(std::fabs(b.wide.run(b.wideC, sc)));
            if (act[k]) { xl = b.bell[0].run(bc[k], xl); xr = b.bell[1].run(bc[k], xr); }
        }
        l[i] = (float) xl; r[i] = (float) xr;
    }
}

// ====================================================================== opto leveller
// Own model of an electro-optical leveller in the style of the LA-2A, not a copy of any circuit:
//  - level detector: peak follower, 1 ms attack, 30 ms release;
//  - gain computer: the mockup's threshold -6 - 30*PR dB, ratio 3 (COMPRESS) or 20 (LIMIT), knee 12 or 2 dB;
//  - cell: the gain reduction is the larger of a fast cell (10 ms attack, 60 ms release) and a slow cell that follows half of the
//    wanted reduction (0.4 s attack, 1 s to 4 s release, longer the more it has been working). This gives the two stage release
//    of the real thing: a quick recovery of the first part and a long tail for the rest, program dependent.
//  - make up: (GAIN - 50) * 0.3 dB + PR * 4.2 dB, as the mockup preview. Wet and dry are mixed after the make up.
void OptoLeveller::prepare(double sr) {
    prepareBase(sr);
    det_.set(0.001, 0.030, sr);
    a1A_ = 1.0 - std::exp(-1.0 / (0.010 * sr)); a1R_ = 1.0 - std::exp(-1.0 / (0.060 * sr));
    a2A_ = 1.0 - std::exp(-1.0 / (0.400 * sr));
    mix_.setTime(0.02, sr / kSub); makeup_.setTime(0.02, sr / kSub);
    resetState();
}
void OptoLeveller::resetState() { det_.clear(); c1_ = c2_ = 0; gr_ = 0.f; }
void OptoLeveller::wake() { mix_.snap(mix_.tgt); makeup_.snap(makeup_.tgt); }
void OptoLeveller::setParams(bool on, bool limitMode, int peakRed, int gain, int mix) {
    setOn(on);
    const double pr = peakRed / 100.0;
    thr_ = (float) (-6.0 - pr * 30.0); ratio_ = limitMode ? 20.f : 3.f; knee_ = limitMode ? 2.f : 12.f;
    mix_.tgt = mix / 127.0;
    makeup_.tgt = (gain - 50) * 0.3 + pr * 4.2;
}
void OptoLeveller::run(float* l, float* r, int n) {
    const double mx = mix_.tick(); const float mk = dbToLin((float) makeup_.tick());
    const double tau2 = 1.0 + 3.0 * std::min(1.0, c2_ / 6.0);
    const double a2R = 1.0 - std::exp(-1.0 / (tau2 * sr_));
    for (int i = 0; i < n; ++i) {
        const float lvl = peakOf(l[i], r[i]);
        const float e = (float) det_.run(lvl);
        const float t = e > 1e-7f ? -downwardDb(linToDb(e), thr_, ratio_, knee_) : 0.f;
        c1_ += ((double) t - c1_) * (t > c1_ ? a1A_ : a1R_);
        const double t2 = 0.5 * t;
        c2_ += (t2 - c2_) * (t2 > c2_ ? a2A_ : a2R);
        const double gr = std::max(c1_, c2_);
        const float g = dbToLin((float) -gr) * mk;
        const float gm = (float) (1.0 - mx + mx * g);
        l[i] *= gm; r[i] *= gm;
        gr_ = (float) -gr;
    }
}

// ====================================================================== OTT squeeze
// Three bands split at 88 Hz and 2.5 kHz with Linkwitz-Riley 24 dB crossovers (allpass matched). Per band a downward and an upward
// compressor in one gain: the mockup's mapping for the downward part (threshold -4 - 34*d dB, ratio 1 + 11*d, make up 5*d dB, attack
// 2..32 ms and release 50..400 ms from TIME); the upward part lifts levels below -34 dB with a ratio of 1 + 5*u (6:1 at full), up to
// 24 dB, and fades out under -80 dB so that silence is not blown up. DEPTH blends the squeezed bands with the unprocessed bands.
void OttSqueeze::prepare(double sr) {
    prepareBase(sr);
    c1_ = lr4Design(88.0, sr); c2_ = lr4Design(2500.0, sr); apc_ = apDesign(2500.0, sr);
    const double tr = sr / kSub;
    depth_.setTime(0.02, tr);
    for (int i = 0; i < 3; ++i) { dnS_[i].setTime(0.02, tr); upS_[i].setTime(0.02, tr); lvS_[i].setTime(0.02, tr); b_[i].dn.prepare(sr); b_[i].dn.knee = 8.f; }
    resetState();
}
void OttSqueeze::resetState() {
    for (int c = 0; c < 2; ++c) { x1_[c].clear(); x2_[c].clear(); ap_[c].clear(); }
    for (auto& b : b_) { b.dn.clear(); b.upDet.clear(); b.upGainDb = 0; }
    for (int i = 0; i < 3; ++i) gr_[i] = 0.f;
}
void OttSqueeze::wake() {
    depth_.snap(depth_.tgt);
    for (int i = 0; i < 3; ++i) { dnS_[i].snap(dnS_[i].tgt); upS_[i].snap(upS_[i].tgt); lvS_[i].snap(lvS_[i].tgt); }
    update();
}
void OttSqueeze::setParams(bool on, const int* down, const int* up, const int* level, int depth, int time) {
    setOn(on);
    depth_.tgt = depth / 127.0; tt_ = time / 127.0;
    for (int i = 0; i < 3; ++i) { dnS_[i].tgt = down[i] / 127.0; upS_[i].tgt = up[i] / 127.0; lvS_[i].tgt = level[i] / 2.0; }
}
void OttSqueeze::update() {
    for (int i = 0; i < 3; ++i) {
        const double dn = dnS_[i].cur, up = upS_[i].cur;
        b_[i].dn.set((float) (-4.0 - dn * 34.0), (float) (1.0 + dn * 11.0), 8.f, 0.002 + tt_ * 0.03, 0.05 + tt_ * 0.35);
        b_[i].upRatio = (float) (1.0 + 5.0 * up);
        mkDb_[i] = lvS_[i].cur + dn * 5.0;
    }
    upAtt_ = 0.003; upRel_ = 0.12 + tt_ * 0.2;
    for (auto& b : b_) b.upDet.set(upAtt_, upRel_, sr_);
}
void OttSqueeze::run(float* l, float* r, int n) {
    depth_.tick(); for (int i = 0; i < 3; ++i) { dnS_[i].tick(); upS_[i].tick(); lvS_[i].tick(); }
    update();
    const double dep = depth_.cur;
    float mk[3];
    for (int i = 0; i < 3; ++i) mk[i] = dbToLin((float) mkDb_[i]);
    for (int s = 0; s < n; ++s) {
        double band[3][2];
        for (int c = 0; c < 2; ++c) {
            const double x = c == 0 ? l[s] : r[s];
            double lo, hi, mid, high;
            x1_[c].split(c1_, x, lo, hi);
            x2_[c].split(c2_, hi, mid, high);
            band[0][c] = ap_[c].run(apc_, lo); band[1][c] = mid; band[2][c] = high;
        }
        double out[2] = { 0, 0 };
        for (int k = 0; k < 3; ++k) {
            Band& b = b_[k];
            const float lvl = (float) std::max(std::fabs(band[k][0]), std::fabs(band[k][1]));
            float g = b.dn.run(lvl);
            if (b.upRatio > 1.001f) {
                const float eu = (float) b.upDet.run(lvl);
                if (eu > 1e-7f) {
                    const float xdb = linToDb(eu);
                    float ud = upwardDb(xdb, b.upThr, b.upRatio, 8.f, 24.f);
                    if (xdb < -72.f) ud *= std::max(0.f, (xdb + 80.f) * 0.125f);
                    g *= dbToLin(ud);
                }
            }
            const float G = g * mk[k];
            const double m = 1.0 - dep + dep * G;
            out[0] += band[k][0] * m; out[1] += band[k][1] * m;
            if (s == n - 1) gr_[k] = b.dn.grDb;
        }
        l[s] = (float) out[0]; r[s] = (float) out[1];
    }
}

// ====================================================================== multiband compressor and limiter
// Four bands, Linkwitz-Riley 24 dB crossovers (the lower bands pass matching allpasses, so the unprocessed sum is flat in magnitude).
// Per band: compressor (threshold, ratio, attack, release or AUTO, boost), a zero latency peak limiter at the band OUTPUT ceiling,
// mute and solo. MIX blends with the unprocessed sum of the bands (not the raw input), so the blend is phase coherent.
// AUTO compressor release: 30 ms (isolated peaks) up to 300 ms (steady reduction), times 1.6, 1.0, 0.7, 0.5 for bands 1 to 4.
// AUTO limiter release: 30 ms up to 250 ms by how long the limiter has been working.
void MultibandDyn::prepare(double sr) {
    prepareBase(sr);
    const double tr = sr / kSub;
    for (int i = 0; i < 3; ++i) fx_[i].setTime(0.03, tr);
    mix_.setTime(0.02, tr);
    static const double scale[4] = { 1.6, 1.0, 0.7, 0.5 };
    for (int b = 0; b < 4; ++b) {
        comp_[b].prepare(sr); comp_[b].relScale = scale[b]; comp_[b].knee = 6.f;
        lim_[b].prepare(sr);
        boost_[b].setTime(0.02, tr); thr_[b].setTime(0.02, tr); ratio_[b].setTime(0.02, tr); ceil_[b].setTime(0.02, tr);
    }
    resetState();
}
void MultibandDyn::resetState() {
    for (int c = 0; c < 2; ++c) { sa_[c].clear(); sb_[c].clear(); sc_[c].clear(); ap1a_[c].clear(); ap1b_[c].clear(); ap2_[c].clear(); }
    for (int b = 0; b < 4; ++b) { comp_[b].clear(); lim_[b].clear(); cgr_[b] = lgr_[b] = 0.f; }
}
void MultibandDyn::wake() {
    mix_.snap(mix_.tgt);
    for (int i = 0; i < 3; ++i) fx_[i].snap(fx_[i].tgt);
    for (int b = 0; b < 4; ++b) { boost_[b].snap(boost_[b].tgt); thr_[b].snap(thr_[b].tgt); ratio_[b].snap(ratio_[b].tgt); ceil_[b].snap(ceil_[b].tgt); gate_[b] = gateT_[b]; }
}
void MultibandDyn::setParams(bool on, int mix, const int* x, const Band* bands) {
    setOn(on);
    mix_.tgt = mix / 127.0;
    double f1 = hzv(x[0], 40, 400), f2 = std::max(hzv(x[1], 300, 3000), f1 * 1.5), f3 = std::max(hzv(x[2], 4000, 16000), f2 * 1.5);
    fx_[0].tgt = std::log(f1); fx_[1].tgt = std::log(f2); fx_[2].tgt = std::log(f3);
    bool solo = false;
    for (int b = 0; b < 4; ++b) { prm_[b] = bands[b]; solo = solo || bands[b].solo; }
    for (int b = 0; b < 4; ++b) {
        const Band& p = prm_[b];
        bandOn_[b] = p.on; limOn_[b] = p.on && p.limOn;
        thr_[b].tgt = p.on ? mbThr(p.thr) : 0.0;
        ratio_[b].tgt = p.on ? std::min(20.0, mbRat(p.ratio)) : 1.0;
        boost_[b].tgt = p.on ? (double) p.boost : 0.0;
        ceil_[b].tgt = limOn_[b] ? (double) p.limOut : 24.0;
        gateT_[b] = (p.mute || (solo && !p.solo)) ? 0.f : 1.f;
    }
    configure();
}
void MultibandDyn::configure() {
    for (int b = 0; b < 4; ++b) {
        const Band& p = prm_[b];
        comp_[b].tauMin = 0.03; comp_[b].tauMax = 0.30;
        comp_[b].set(0.f, 1.f, 6.f, std::max(0.00005, mbAtkMs(p.att) / 1000.0), mbRelMs(p.rel) / 1000.0);
    }
}
void MultibandDyn::updateCoefs() {
    const double f1 = std::exp(fx_[0].cur), f2 = std::exp(fx_[1].cur), f3 = std::exp(fx_[2].cur);
    ca_ = lr4Design(f1, sr_); cb_ = lr4Design(f2, sr_); cc_ = lr4Design(f3, sr_);
    apf2_ = apDesign(f2, sr_); apf3_ = apDesign(f3, sr_);
}
void MultibandDyn::run(float* l, float* r, int n) {
    for (int i = 0; i < 3; ++i) fx_[i].tick();
    updateCoefs();
    const double mx = (mix_.tick());
    float thrC[4], ratC[4], bst[4], ceilC[4];
    for (int b = 0; b < 4; ++b) {
        thrC[b] = (float) thr_[b].tick(); ratC[b] = (float) ratio_[b].tick();
        bst[b] = dbToLin((float) boost_[b].tick()); ceilC[b] = dbToLin((float) ceil_[b].tick());
        comp_[b].thr = thrC[b]; comp_[b].ratio = std::max(1.f, ratC[b]);
        comp_[b].tick(); lim_[b].ceil = ceilC[b]; lim_[b].tick();
        // limiter release is set in configure for fixed values
        const double lr = mbLimRelMs(prm_[b].limRel) / 1000.0;
        lim_[b].autoRel = lr <= 0.0; if (!lim_[b].autoRel) lim_[b].aR = std::exp(-1.0 / (lr * sr_));
    }
    const float gstep = (float) (n / (0.008 * sr_));
    for (int b = 0; b < 4; ++b) {
        if (gate_[b] < gateT_[b]) gate_[b] = std::min(gateT_[b], gate_[b] + gstep); else if (gate_[b] > gateT_[b]) gate_[b] = std::max(gateT_[b], gate_[b] - gstep);
    }
    for (int s = 0; s < n; ++s) {
        double bd[4][2];
        for (int c = 0; c < 2; ++c) {
            const double x = c == 0 ? l[s] : r[s];
            double lo1, hi1, lo2, hi2, lo3, hi3;
            sa_[c].split(ca_, x, lo1, hi1);
            sb_[c].split(cb_, hi1, lo2, hi2);
            sc_[c].split(cc_, hi2, lo3, hi3);
            bd[0][c] = ap1b_[c].run(apf3_, ap1a_[c].run(apf2_, lo1));
            bd[1][c] = ap2_[c].run(apf3_, lo2);
            bd[2][c] = lo3; bd[3][c] = hi3;
        }
        double dry[2] = { 0, 0 }, wet[2] = { 0, 0 };
        for (int b = 0; b < 4; ++b) {
            dry[0] += bd[b][0] * gate_[b]; dry[1] += bd[b][1] * gate_[b];
            float yl = (float) bd[b][0], yr = (float) bd[b][1];
            if (bandOn_[b]) {
                const float g = comp_[b].run(std::max(std::fabs(yl), std::fabs(yr))) * bst[b];
                yl *= g; yr *= g;
            }
            if (limOn_[b]) {
                const float g = lim_[b].run(std::max(std::fabs(yl), std::fabs(yr)));
                yl *= g; yr *= g;
            }
            wet[0] += (double) yl * gate_[b]; wet[1] += (double) yr * gate_[b];
        }
        l[s] = (float) (dry[0] * (1.0 - mx) + wet[0] * mx);
        r[s] = (float) (dry[1] * (1.0 - mx) + wet[1] * mx);
    }
    for (int b = 0; b < 4; ++b) { cgr_[b] = bandOn_[b] ? comp_[b].grDb : 0.f; lgr_[b] = limOn_[b] ? lim_[b].grDb : 0.f; }
}

// ====================================================================== glue compressor with parallel path
// Main: threshold -30*(1-v/127) dB, ratio 1.5/2/3/4, attack 10..100 ms, knee 6 dB, release AUTO between 150 and 450 ms (the preview used
// 250 ms fixed). Parallel: the input through a heavy compressor (-36 dB, 10:1, 5 ms, 120 ms) added at PARALLEL * 0.5.
void GlueComp::prepare(double sr) {
    prepareBase(sr);
    main_.prepare(sr); par_.prepare(sr);
    main_.tauMin = 0.15; main_.tauMax = 0.45;
    par_.set(-36.f, 10.f, 6.f, 0.005, 0.12);
    const double tr = sr / kSub;
    thr_.setTime(0.02, tr); ratio_.setTime(0.02, tr); pg_.setTime(0.02, tr);
    resetState();
}
void GlueComp::resetState() { main_.clear(); par_.clear(); }
void GlueComp::wake() { thr_.snap(thr_.tgt); ratio_.snap(ratio_.tgt); pg_.snap(pg_.tgt); }
void GlueComp::setParams(bool on, int ratioIdx, int thresh, int attack, int parallel) {
    setOn(on);
    static const double rt[4] = { 1.5, 2.0, 3.0, 4.0 };
    ratio_.tgt = rt[ratioIdx < 0 ? 0 : (ratioIdx > 3 ? 3 : ratioIdx)];
    thr_.tgt = -30.0 * (1.0 - thresh / 127.0);
    attSec_ = 0.010 * std::pow(10.0, attack / 127.0);
    pg_.tgt = parallel / 127.0 * 0.5;
}
void GlueComp::run(float* l, float* r, int n) {
    const float th = (float) thr_.tick(), ra = (float) ratio_.tick(), pg = (float) pg_.tick();
    main_.set(th, ra, 6.f, attSec_, 0.0);
    main_.tick(); par_.tick();
    for (int i = 0; i < n; ++i) {
        const float lvl = peakOf(l[i], r[i]);
        const float g = main_.run(lvl);
        const float gp = pg > 0.f ? par_.run(lvl) * pg : 0.f;
        const float m = g + gp;
        l[i] *= m; r[i] *= m;
    }
}

}  // namespace fxdsp
}  // namespace jpurrr
