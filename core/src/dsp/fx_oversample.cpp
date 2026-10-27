#include "fx_oversample.h"
JPURRR_FX_FAST
namespace jpurrr {
namespace fxdsp {

static double besselI0(double x) {
    double sum = 1.0, term = 1.0, q = x * x * 0.25;
    for (int k = 1; k < 60; ++k) { term *= q / ((double) k * k); sum += term; if (term < 1e-18 * sum) break; }
    return sum;
}

void HbKernel::design(int centre, double beta) {
    M = centre; pc = M & 1; pd = 1 - pc;
    const int J = (M % 2 == 1) ? M : M - 1;                // largest odd offset that fits in 0..2M
    n = (J + 1) / 2;
    L = (2 * n + 3) & ~3;
    std::vector<float> half((size_t) n);
    const double i0b = besselI0(beta);
    for (int i = 0; i < n; ++i) {
        const int j = 1 + 2 * i;
        const double r = (double) j / M;
        const double w = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0b;
        half[(size_t) i] = (float) (std::sin(kPi * j / 2.0) / (kPi * j) * w);
    }
    // palindromic 2n taps: first half is the mirror image of half[], second half is half[]
    cUp.assign((size_t) L, 0.f); cDown.assign((size_t) L, 0.f);
    const int pad = L - 2 * n;
    for (int i = 0; i < n; ++i) {
        cDown[(size_t) (pad + i)] = half[(size_t) (n - 1 - i)]; cDown[(size_t) (pad + n + i)] = half[(size_t) i];
    }
    for (int i = 0; i < L; ++i) cUp[(size_t) i] = 2.f * cDown[(size_t) i];
    ucd = (M - pc) / 2;
    dmin = (M - 1 + pd) / 2 - (n - 1); dcd = (M + pc) / 2;
    N = 16; while (N < L + dmin + 4 || N < M + 4) N <<= 1;
}

OsKernels::OsKernels() { s1.design(35, 9.0); s2.design(12, 9.0); }
const OsKernels& OsKernels::get() { static const OsKernels k; return k; }
int OsKernels::roundTripDelay() { const OsKernels& k = get(); return k.s1.M + k.s2.M / 2; }

void TruePeakProbe::init() {
    // y(t + tau) = sum_{i=1-kHalf..kHalf} x[t+i] * s(i - tau), s(u) = sinc(u) * Kaiser(u / kHalf)
    const double beta = 6.0, i0b = besselI0(beta);
    l1_ = 1.f;
    for (int p = 0; p < kPhases; ++p) {
        const double tau = (double) p / kPhases;
        double sum = 0.0;
        for (int i = 0; i < 2 * kHalf; ++i) {
            const double u = (i - (kHalf - 1)) - tau;
            const double r = u / kHalf;
            const double w = std::fabs(r) >= 1.0 ? 0.0 : besselI0(beta * std::sqrt(1.0 - r * r)) / i0b;
            const double s = std::fabs(u) < 1e-12 ? 1.0 : std::sin(kPi * u) / (kPi * u);
            taps_[p][i] = (float) (s * w);
            sum += std::fabs(s * w);
        }
        l1_ = std::max(l1_, (float) sum);
    }
    l1_ *= 1.01f;
    clear();
}

}  // namespace fxdsp
}  // namespace jpurrr
