#include "fx_drive.h"
JPURRR_FX_FAST
namespace jpurrr {
namespace fxdsp {

void DriveShape::set(int dv, int dx, int df) {
    wd = std::min(1.0, dv / 127.0 * 1.5); wt = dx / 127.0; wf = df / 127.0;
    k1 = 1 + dv / 127.0 * 8; k2 = 1 + dx / 127.0 * 6; k3 = 2 + df / 127.0 * 14;
    n1 = 1.0 / std::tanh(k1); n2 = 1.0 / std::tanh(k2); n3 = 1.0 / std::tanh(k3);
}

void ShaperTable::build(int dv_, int dx_, int df_) {
    dv = dv_; dx = dx_; df = df_;
    DriveShape s; s.set(dv, dx, df);
    const double h = H;
    for (int i = 0; i <= N; ++i) f[i] = s(-R + i * h);
    F1[0] = 0; F2[0] = 0;
    // Simpson on each cell for F1; the cell midpoint of F1 follows from the Hermite form, then Simpson for F2.
    for (int i = 0; i < N; ++i) {
        double xm = -R + (i + 0.5) * h, fm = s(xm);
        F1[i + 1] = F1[i] + h / 6.0 * (f[i] + 4.0 * fm + f[i + 1]);
    }
    for (int i = 0; i < N; ++i) {
        double F1m = 0.5 * (F1[i] + F1[i + 1]) + h / 8.0 * (f[i] - f[i + 1]);
        F2[i + 1] = F2[i] + h / 6.0 * (F1[i] + 4.0 * F1m + F1[i + 1]);
    }
}

// ------------------------------------------------------------------ drive stack
namespace {
struct Adaa2State {
    double x1 = 0, x2 = 0, F2x1 = 0, dprev = 0;   // dprev = D1(x1, x2)
    void reset(const ShaperTable& T) { x1 = x2 = 0; F2x1 = T.evalF2(0.0); dprev = T.evalF1(0.0); }
};
inline double adaa2(const ShaperTable& T, Adaa2State& s, double x0) {
    x0 = x0 < -ShaperTable::R ? -ShaperTable::R : (x0 > ShaperTable::R ? ShaperTable::R : x0);
    const double F2x0 = T.evalF2(x0);
    const double d01 = x0 - s.x1;
    const double D = std::fabs(d01) > 1e-5 ? (F2x0 - s.F2x1) / d01 : T.evalF1(0.5 * (x0 + s.x1));
    const double d02 = x0 - s.x2;
    double y = std::fabs(d02) > 1e-5 ? 2.0 * (D - s.dprev) / d02 : T.evalF((x0 + 2.0 * s.x1 + s.x2) * 0.25);
    s.x2 = s.x1; s.x1 = x0; s.F2x1 = F2x0; s.dprev = D;
    return y;
}
}  // namespace

struct DriveStack::Impl {
    double sr = 48000;
    Oversampler4 os[2];
    Ring dry[2];
    ShaperTable* tab[2] = { nullptr, nullptr };     // [0] current, [1] the one fading in
    std::unique_ptr<ShaperTable> storeA, storeB;
    Adaa2State st[2][2];                           // [channel][table slot]
    int tdv = 30, tdx = 25, tdf = 0;               // target curve parameters
    bool xfading = false; int xfPos = 0; static constexpr int xfLen = 256;
    float w = 0.f, wTarget = 0.f;                  // wet weight (on * mix)
    float wStep = 0.f;
    bool hasTable = false;
    void startTables() {
        tab[0]->build(tdv, tdx, tdf); xfading = false; hasTable = true;
        for (int c = 0; c < 2; ++c) { st[c][0].reset(*tab[0]); st[c][1].reset(*tab[0]); }
    }
};

DriveStack::DriveStack() : p_(new Impl) {}
DriveStack::~DriveStack() = default;

void DriveStack::prepare(double sr) {
    OsKernels::get();
    p_->sr = sr;
    p_->storeA.reset(new ShaperTable); p_->storeB.reset(new ShaperTable);
    p_->tab[0] = p_->storeA.get(); p_->tab[1] = p_->storeB.get();
    p_->wStep = (float) (1.0 / (sr * 0.012));
    for (int c = 0; c < 2; ++c) { p_->os[c].init(); p_->dry[c].alloc(128); }
    reset();
}
void DriveStack::reset() {
    for (int c = 0; c < 2; ++c) { p_->os[c].clear(); p_->dry[c].clear(); }
    p_->hasTable = false; p_->xfading = false; p_->w = p_->wTarget; asleep_ = true;
}
void DriveStack::setParams(bool on, int dv, int dx, int df, int mix) {
    p_->wTarget = on ? (float) (mix / 127.0) : 0.f;
    p_->tdv = dv; p_->tdx = dx; p_->tdf = df;
}

void DriveStack::process(float* l, float* r, int n) {
    Impl& s = *p_;
    const float w0 = s.w;
    float w1 = w0;
    if (w1 < s.wTarget) w1 = std::min(s.wTarget, w1 + s.wStep * n); else if (w1 > s.wTarget) w1 = std::max(s.wTarget, w1 - s.wStep * n);
    s.w = w1;
    const int D = latency();
    float* ch[2] = { l, r };
    const bool active = (w0 > 0.f || w1 > 0.f);
    if (!active) {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) { s.dry[c].push(ch[c][i]); ch[c][i] = s.dry[c].at(D); }
        asleep_ = true; return;
    }
    if (asleep_) {
        for (int c = 0; c < 2; ++c) s.os[c].clear();
        s.startTables();
        asleep_ = false;
    }
    // table update: rebuild into the spare slot, then crossfade over xfLen samples
    if (s.xfading && s.xfPos >= Impl::xfLen) { std::swap(s.tab[0], s.tab[1]); s.xfading = false; for (int c = 0; c < 2; ++c) s.st[c][0] = s.st[c][1]; }
    if (!s.xfading && (s.tab[0]->dv != s.tdv || s.tab[0]->dx != s.tdx || s.tab[0]->df != s.tdf)) {
        s.tab[1]->build(s.tdv, s.tdx, s.tdf);
        s.xfading = true; s.xfPos = 0;
        for (int c = 0; c < 2; ++c) s.st[c][1] = s.st[c][0];
    }
    const float inv = n > 0 ? 1.f / n : 0.f;
    for (int c = 0; c < 2; ++c) {
        const ShaperTable& A = *s.tab[0]; const ShaperTable& B = *s.tab[1];
        Adaa2State& sa = s.st[c][0]; Adaa2State& sb = s.st[c][1];
        const bool xf = s.xfading; int xp = s.xfPos;
        for (int i = 0; i < n; ++i) {
            const float x = ch[c][i];
            s.dry[c].push(x);
            const float dryD = s.dry[c].at(D);
            float wet;
            if (!xf) {
                wet = s.os[c].run(x, [&](float* v) { for (int k = 0; k < 4; ++k) v[k] = (float) adaa2(A, sa, (double) v[k]); });
            } else {
                const float a = std::min(1.f, (float) (xp + i) / Impl::xfLen);
                wet = s.os[c].run(x, [&](float* v) {
                    for (int k = 0; k < 4; ++k) { double ya = adaa2(A, sa, (double) v[k]), yb = adaa2(B, sb, (double) v[k]); v[k] = (float) (ya + (yb - ya) * a); }
                });
            }
            const float wt = w0 + (w1 - w0) * (i + 1) * inv;
            ch[c][i] = dryD + (wet - dryD) * wt;
        }
    }
    if (s.xfading) s.xfPos += n;
}

// ------------------------------------------------------------------ soft clipper
namespace {
// ln(cosh(u)) for u >= 0 as a Hermite table on [0, 20] (step 1/32, derivative tanh); beyond 20 it is u - ln 2 to double precision.
struct LnCoshTable {
    static constexpr int N = 640;
    static constexpr double H = 1.0 / 32.0;
    double f[N + 1], d[N + 1];
    LnCoshTable() {
        for (int i = 0; i <= N; ++i) { double u = i * H; f[i] = u + std::log1p(std::exp(-2.0 * u)) - 0.69314718055994531; d[i] = std::tanh(u); }
    }
    inline double operator()(double u) const {
        u = std::fabs(u);
        if (u >= N * H) return u - 0.69314718055994531;
        const double x = u * 32.0; const int i = (int) x; const double t = x - i, t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * f[i] + (t3 - 2 * t2 + t) * H * d[i] + (-2 * t3 + 3 * t2) * f[i + 1] + (t3 - t2) * H * d[i + 1];
    }
};
const LnCoshTable& lnCosh() { static const LnCoshTable t; return t; }
}
struct SoftClipper::Impl {
    double sr = 48000;
    Oversampler4 os[2];
    Ring dry[2];
    double x1[2] = { 0, 0 };
    float w = 0.f, wTarget = 0.f, wStep = 0.f;
    Smooth ck;                                      // clip hardness 1..4
    double ckTarget = 2.0;
    bool asleep = true;
};
SoftClipper::SoftClipper() : p_(new Impl) {}
SoftClipper::~SoftClipper() = default;
void SoftClipper::prepare(double sr) {
    OsKernels::get(); lnCosh();
    p_->sr = sr; p_->wStep = (float) (1.0 / (sr * 0.012));
    p_->ck.setTime(0.02, sr / kSub);
    for (int c = 0; c < 2; ++c) { p_->os[c].init(); p_->dry[c].alloc(128); }
    reset();
}
void SoftClipper::reset() {
    for (int c = 0; c < 2; ++c) { p_->os[c].clear(); p_->dry[c].clear(); p_->x1[c] = 0; }
    p_->w = p_->wTarget; p_->asleep = true; p_->ck.snap(p_->ckTarget);
}
void SoftClipper::setParams(bool on, int amount) {
    p_->wTarget = on ? 1.f : 0.f;
    p_->ckTarget = 1.0 + amount / 127.0 * 3.0;
    p_->ck.tgt = p_->ckTarget;
}
void SoftClipper::process(float* l, float* r, int n) {
    Impl& s = *p_;
    const float w0 = s.w;
    float w1 = w0;
    if (w1 < s.wTarget) w1 = std::min(s.wTarget, w1 + s.wStep * n); else if (w1 > s.wTarget) w1 = std::max(s.wTarget, w1 - s.wStep * n);
    s.w = w1;
    const int D = latency();
    float* ch[2] = { l, r };
    if (w0 <= 0.f && w1 <= 0.f) {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) { s.dry[c].push(ch[c][i]); ch[c][i] = s.dry[c].at(D); }
        s.asleep = true; return;
    }
    if (s.asleep) { for (int c = 0; c < 2; ++c) { s.os[c].clear(); s.x1[c] = 0; } s.ck.snap(s.ckTarget); s.asleep = false; }
    const double ck = s.ck.tick();
    const double norm = 1.0 / std::tanh(ck), invck = norm / ck;
    const LnCoshTable& LC = lnCosh();
    const float inv = n > 0 ? 1.f / n : 0.f;
    for (int c = 0; c < 2; ++c) {
        double xm = s.x1[c], lcm = LC(ck * xm);
        for (int i = 0; i < n; ++i) {
            const float x = ch[c][i];
            s.dry[c].push(x);
            const float dryD = s.dry[c].at(D);
            const float wet = s.os[c].run(x, [&](float* v) {
                for (int k = 0; k < 4; ++k) {
                    const double x0 = v[k], d = x0 - xm, lc0 = LC(ck * x0);
                    double y;
                    if (std::fabs(d) > 1e-6) y = (lc0 - lcm) * invck / d;
                    else y = std::tanh(ck * 0.5 * (x0 + xm)) * norm;
                    xm = x0; lcm = lc0; v[k] = (float) y;
                }
            });
            const float wt = w0 + (w1 - w0) * (i + 1) * inv;
            ch[c][i] = dryD + (wet - dryD) * wt;
        }
        s.x1[c] = xm;
    }
}

}  // namespace fxdsp
}  // namespace jpurrr
