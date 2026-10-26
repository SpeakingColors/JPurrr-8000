// Drive stack (three stage tanh curve of the mockup, anti-aliased) and the output soft clipper.
// Both run 4x oversampled with half band linear phase filters; the drive curve also uses 2nd order ADAA
// (antiderivative anti-aliasing, Parker et al. 2016) on a table, the clipper uses 1st order ADAA in closed form.
#pragma once
#include "fx_common.h"
#include "fx_oversample.h"
#include <memory>

namespace jpurrr {
namespace fxdsp {

// The static curve the DRIVE / TRANS. / FUZZ knobs draw in the mockup (shapeCurve), evaluated in double.
struct DriveShape {
    double wd = 0, wt = 0, wf = 0, k1 = 1, k2 = 1, k3 = 2, n1 = 1, n2 = 1, n3 = 1;
    void set(int dv, int dx, int df);
    inline double operator()(double x) const {
        double y1 = x + (std::tanh(k1 * x) * n1 - x) * wd;
        double y2 = y1 + (std::tanh(k2 * (y1 + 0.12 * y1 * y1)) * n2 - y1) * wt;
        return y2 + (std::tanh(k3 * y2) * n3 - y2) * wf;
    }
};

// Table of the curve f, its antiderivative F1 and second antiderivative F2 on [-R, R], Hermite interpolated.
struct ShaperTable {
    static constexpr int N = 2048;
    static constexpr double R = 2.5;
    static constexpr double H = 2.0 * R / N;
    int dv = -1, dx = -1, df = -1;
    double f[N + 1], F1[N + 1], F2[N + 1];
    void build(int dv_, int dx_, int df_);
    static inline void locate(double x, int& i, double& t) {
        x = x < -R ? -R : (x > R ? R : x);
        double u = (x + R) * (1.0 / H); i = (int) u; if (i >= N) i = N - 1; t = u - i;
    }
    inline double evalF2(double x) const {
        int i; double t; locate(x, i, t); double t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * F2[i] + (t3 - 2 * t2 + t) * H * F1[i] + (-2 * t3 + 3 * t2) * F2[i + 1] + (t3 - t2) * H * F1[i + 1];
    }
    inline double evalF1(double x) const {
        int i; double t; locate(x, i, t); double t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * F1[i] + (t3 - 2 * t2 + t) * H * f[i] + (-2 * t3 + 3 * t2) * F1[i + 1] + (t3 - t2) * H * f[i + 1];
    }
    inline double evalF(double x) const { int i; double t; locate(x, i, t); return f[i] + (f[i + 1] - f[i]) * t; }
};

class DriveStack {
public:
    DriveStack();
    ~DriveStack();
    void prepare(double sr);
    void reset();
    void setParams(bool on, int dv, int dx, int df, int mix);   // mix 0..127
    static int latency() { return OsKernels::roundTripDelay(); }
    void process(float* l, float* r, int n);                    // n <= kSub
    bool asleep() const { return asleep_; }
private:
    struct Impl; std::unique_ptr<Impl> p_;
    bool asleep_ = true;
};

class SoftClipper {
public:
    SoftClipper();
    ~SoftClipper();
    void prepare(double sr);
    void reset();
    void setParams(bool on, int amount);                        // amount 0..127 (qka)
    static int latency() { return OsKernels::roundTripDelay(); }
    void process(float* l, float* r, int n);
private:
    struct Impl; std::unique_ptr<Impl> p_;
};

}  // namespace fxdsp
}  // namespace jpurrr
