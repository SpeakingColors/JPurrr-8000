#include "jpurrr/synth.h"
#include <cstring>
namespace jpurrr {
struct Synth::Impl { double sr = 44100; };
Synth::Synth() : impl_(new Impl) {} Synth::~Synth() = default;
void Synth::prepare(double sr, int) { impl_->sr = sr; }
void Synth::reset() {}
void Synth::setParams(const int*) {}
void Synth::noteOn(int, int, int) {} void Synth::noteOff(int, int) {} void Synth::allNotesOff() {}
void Synth::pitchBend(int, int) {} void Synth::controller(int, int, int) {} void Synth::channelPressure(int, int) {}
void Synth::setTempo(double) {} void Synth::setTransport(bool, double) {}
void Synth::process(float* l, float* r, int n) { std::memset(l, 0, sizeof(float) * n); std::memset(r, 0, sizeof(float) * n); }
}
