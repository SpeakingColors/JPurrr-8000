#include "jpurrr/fxchain.h"
namespace jpurrr {
struct FxChain::Impl { double sr = 44100; Live* live = nullptr; };
FxChain::FxChain() : impl_(new Impl) {} FxChain::~FxChain() = default;
void FxChain::prepare(double sr, int) { impl_->sr = sr; }
void FxChain::reset() {} void FxChain::setParams(const int*) {} void FxChain::setTempo(double) {}
void FxChain::setTransport(bool, double) {} void FxChain::process(float*, float*, int) {}
int FxChain::latencySamples() const { return 0; }
void FxChain::setLive(Live* l) { impl_->live = l; }
}
