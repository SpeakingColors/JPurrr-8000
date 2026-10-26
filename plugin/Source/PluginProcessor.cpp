#include "PluginProcessor.h"
#include "Editor/EditorFactory.h"
using namespace jpurrr;

static juce::String optText(int idx, int v) {
    const ParamDef& d = kParams[idx];
    if (d.optCount > 0 && v >= d.min && v - d.min < d.optCount) return kOptionText[d.optStart + (v - d.min)];
    return juce::String(v);
}

juce::AudioProcessorValueTreeState::ParameterLayout JPurrr8000AudioProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumParams; ++i) {
        const ParamDef& d = kParams[i];
        if (d.flags & (kNoSave | kMomentary)) continue;
        juce::String name = juce::String(d.label).trim();
        // labels repeat (WAVEFORM, A, D...): make host names unique and readable
        name += " [" + juce::String(d.id) + "]";
        juce::ParameterID pid { d.id, 1 };
        if (d.type == PType::Switch)
            layout.add(std::make_unique<juce::AudioParameterBool>(pid, name, d.def != 0));
        else
            layout.add(std::make_unique<juce::AudioParameterInt>(pid, name, d.min, d.max, d.def,
                juce::AudioParameterIntAttributes().withStringFromValueFunction([i](int v, int) { return optText(i, v); })));
    }
    return layout;
}

JPurrr8000AudioProcessor::JPurrr8000AudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "JPURRR", makeLayout()) {
    for (int i = 0; i < kNumParams; ++i) {
        uiVal[(size_t) i].store(kParams[i].def);
        hostParams[(size_t) i] = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(kParams[i].id));
    }
    synth.setLive(&live); fx.setLive(&live);
    presetName = presets.cur.name;
}
JPurrr8000AudioProcessor::~JPurrr8000AudioProcessor() = default;

bool JPurrr8000AudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void JPurrr8000AudioProcessor::prepareToPlay(double sampleRate, int block) {
    sr = sampleRate; synth.prepare(sampleRate, block); soundFx.prepare(sampleRate, block); fx.prepare(sampleRate, block);
    setLatencySamples(synth.latencySamples() + soundFx.latencySamples() + fx.latencySamples());
}

int JPurrr8000AudioProcessor::getInt(int idx) const {
    if (auto* p = hostParams[(size_t) idx]) return (int) std::lround(p->convertFrom0to1(p->getValue()));
    return uiVal[(size_t) idx].load();
}
void JPurrr8000AudioProcessor::setInt(int idx, int v) {
    const ParamDef& d = kParams[idx];
    v = juce::jlimit(d.min, d.max, v);
    if (auto* p = hostParams[(size_t) idx]) p->setValueNotifyingHost(p->convertTo0to1((float) v));
    else uiVal[(size_t) idx].store(v);
}
void JPurrr8000AudioProcessor::beginGesture(int idx) { if (auto* p = hostParams[(size_t) idx]) p->beginChangeGesture(); }
void JPurrr8000AudioProcessor::endGesture(int idx)   { if (auto* p = hostParams[(size_t) idx]) p->endChangeGesture(); }
void JPurrr8000AudioProcessor::setAll(const int* v) {
    for (int i = 0; i < kNumParams; ++i) if (!(kParams[i].flags & (kNoSave | kMomentary))) setInt(i, v[i]);
}
void JPurrr8000AudioProcessor::getAll(int* v) const { for (int i = 0; i < kNumParams; ++i) v[i] = getInt(i); }

void JPurrr8000AudioProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    int vals[kNumParams];
    for (int i = 0; i < kNumParams; ++i) vals[i] = getInt(i);
    synth.setParams(vals); soundFx.setParams(vals); fx.setParams(vals);
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) {
        const double bpm = pos->getBpm().orFallback(120.0);
        synth.setTempo(bpm); soundFx.setTempo(bpm); fx.setTempo(bpm);
        const bool playing = pos->getIsPlaying(); const double ppq = pos->getPpqPosition().orFallback(0.0);
        synth.setTransport(playing, ppq); fx.setTransport(playing, ppq);
    }
    float* l = buf.getWritePointer(0); float* r = buf.getNumChannels() > 1 ? buf.getWritePointer(1) : l;
    int done = 0;
    auto renderTo = [&](int end) {
        if (end > done) { synth.process(l + done, r + done, end - done); soundFx.process(l + done, r + done, end - done); fx.process(l + done, r + done, end - done); done = end; }
    };
    for (const auto meta : midi) {
        renderTo(meta.samplePosition);
        const auto m = meta.getMessage(); const int ch = m.getChannel() - 1;
        if (m.isNoteOn()) synth.noteOn(ch, m.getNoteNumber(), m.getVelocity());
        else if (m.isNoteOff()) synth.noteOff(ch, m.getNoteNumber());
        else if (m.isPitchWheel()) synth.pitchBend(ch, m.getPitchWheelValue() - 8192);
        else if (m.isController()) synth.controller(ch, m.getControllerNumber(), m.getControllerValue());
        else if (m.isChannelPressure()) synth.channelPressure(ch, m.getChannelPressureValue());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff();
    }
    renderTo(n);
    midi.clear();
}

// State: the host parameters by id (so sessions keep loading when parameters are added), the sound name and where it
// came from, the editor width. kStateVersion changes only when the meaning of stored values changes.
static constexpr int kStateVersion = 1;
void JPurrr8000AudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = apvts.copyState();
    state.setProperty("stateVersion", kStateVersion, nullptr);
    state.setProperty("presetName", presets.cur.name, nullptr);
    state.setProperty("presetBank", presets.cur.bank, nullptr);
    state.setProperty("presetSlot", presets.cur.slot, nullptr);
    state.setProperty("presetFactory", presets.cur.factory, nullptr);
    state.setProperty("presetType", presets.cur.type, nullptr);
    state.setProperty("presetDirty", presets.cur.dirty, nullptr);
    state.setProperty("uiWidth", uiWidth, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}
void JPurrr8000AudioProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        auto st = juce::ValueTree::fromXml(*xml);
        if (!st.isValid()) return;
        presets.cur.name = st.getProperty("presetName", "Init").toString();
        presets.cur.bank = st.getProperty("presetBank", "A").toString();
        presets.cur.slot = (int) st.getProperty("presetSlot", 1);
        presets.cur.factory = (bool) st.getProperty("presetFactory", true);
        presets.cur.type = st.getProperty("presetType", "Lead").toString();
        presets.cur.dirty = (bool) st.getProperty("presetDirty", false);
        uiWidth = (int) st.getProperty("uiWidth", 0);
        presetName = presets.cur.name;
        apvts.replaceState(st);
    }
}

juce::AudioProcessorEditor* JPurrr8000AudioProcessor::createEditor() { return createJPurrrEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JPurrr8000AudioProcessor(); }
