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
    initParts();
    hostParams[(size_t) P_km]->addListener(this); hostParams[(size_t) P_ps]->addListener(this);
}
JPurrr8000AudioProcessor::~JPurrr8000AudioProcessor() {
    hostParams[(size_t) P_km]->removeListener(this); hostParams[(size_t) P_ps]->removeListener(this);
}

// ------------------------------------------------------------------ the two parts
static CurrentPreset metaOfFactory(const PresetEntry& e) {
    CurrentPreset m; m.name = e.name; m.bank = e.bank; m.slot = e.slot; m.factory = e.factory; m.type = e.type; m.genres = e.genres; m.dirty = false; return m;
}
void JPurrr8000AudioProcessor::initParts() {
    // UPPER = the sound on the panel, LOWER = the sound the mockup starts with (factory slot 9). Only used once KEY MODE is DUAL or SPLIT.
    for (int i = 0; i < kNumParams; ++i) { partVals[0][(size_t) i].store(kParams[i].def); partVals[1][(size_t) i].store(kParams[i].def); }
    presets.cur.genres = presets.size() > 0 ? presets.at(0).genres : juce::StringArray();
    partMetas[0] = presets.cur; partMetas[1] = presets.cur;
    if (presets.size() > 8) {
        const PresetEntry& e = presets.at(8);
        for (int i = 0; i < kNumParams; ++i) partVals[1][(size_t) i].store(e.v[(size_t) i]);
        partMetas[1] = metaOfFactory(e);
    }
    editPartA.store(kUpper);
}
const CurrentPreset& JPurrr8000AudioProcessor::partMeta(int part) const { return part == editPart() ? presets.cur : partMetas[juce::jlimit(0, 1, part)]; }
int JPurrr8000AudioProcessor::partValue(int part, int idx) const { return part == editPart() ? getInt(idx) : partVals[juce::jlimit(0, 1, part)][(size_t) idx].load(); }
void JPurrr8000AudioProcessor::storePanelPart() {
    const int e = editPart();
    for (int i = 0; i < kNumParams; ++i) if (isPartParam(i)) partVals[e][(size_t) i].store(getInt(i));
    partMetas[e] = presets.cur;
}
void JPurrr8000AudioProcessor::putPart(int part, const int* v, const CurrentPreset& meta) {
    part = juce::jlimit(0, 1, part);
    for (int i = 0; i < kNumParams; ++i) if (isPartParam(i)) partVals[part][(size_t) i].store(v[i]);
    partMetas[part] = meta; partMetas[part].dirty = false;
    if (part == editPart()) presets.cur = partMetas[part];
}
void JPurrr8000AudioProcessor::loadPartToPanel() {
    const int e = editPart();
    partBusy.store(true);
    for (int i = 0; i < kNumParams; ++i) if (isPartParam(i)) setInt(i, partVals[e][(size_t) i].load());
    presets.cur = partMetas[e]; presetName = presets.cur.name;
    partBusy.store(false);
}
bool JPurrr8000AudioProcessor::syncEditPart() {
    const int t = jpurrr::effectiveEditPart(getInt(P_km), getInt(P_ps), editPart());
    if (t == editPart()) return false;
    storePanelPart();
    editPartA.store(t);
    loadPartToPanel();
    return true;
}
void JPurrr8000AudioProcessor::mirrorPartValue(int idx, int value) {
    if (!isPartParam(idx) || getInt(P_km) == 0 || getInt(P_ps) != 2) return;
    const int o = 1 - editPart();
    partVals[o][(size_t) idx].store(value);
    partMetas[o].dirty = true;
}
void JPurrr8000AudioProcessor::markPartDirty(int part) { if (part == editPart()) presets.cur.dirty = true; else partMetas[juce::jlimit(0, 1, part)].dirty = true; }

bool JPurrr8000AudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void JPurrr8000AudioProcessor::prepareToPlay(double sampleRate, int block) {
    sr = sampleRate; jpurrr::midiMapInit();
    synth.prepare(sampleRate, block); soundFx.prepare(sampleRate, block); fx.prepare(sampleRate, block);
    int vals[kNumParams]; getAll(vals); fx.setParams(vals);
    reportedLatency = synth.latencySamples() + soundFx.latencySamples() + fx.latencySamples();
    wantedLatency.store(reportedLatency);
    setLatencySamples(reportedLatency);
    for (auto& m : mappers) m.reset();
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

void JPurrr8000AudioProcessor::pushStrip(StripEvent e) {
    int s1, n1, s2, n2; stripFifo.prepareToWrite(1, s1, n1, s2, n2);
    if (n1 > 0) { stripData[(size_t) s1] = e; stripFifo.finishedWrite(1); }
    else if (e.kind == 1) stripPanic.store(true);                     // a lost note off or all-off must never leave a note hanging
}
void JPurrr8000AudioProcessor::stripNoteOn(int note, int velocity) { pushStrip({ 0, (std::int8_t) juce::jlimit(0, 127, note), (std::int8_t) juce::jlimit(1, 127, velocity) }); }
void JPurrr8000AudioProcessor::stripNoteOff(int note) { pushStrip({ 1, (std::int8_t) juce::jlimit(0, 127, note) }); }

void JPurrr8000AudioProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    int vals[kNumParams];
    for (int i = 0; i < kNumParams; ++i) vals[i] = getInt(i);
    // The two parts: the panel's values for the edited part, the stored values for the other. The voice engine and the effects section
    // follow UPPER (one shared effect). While the panel is being switched to the other part the last sound is kept for this block.
    if (!partBusy.load()) {
        int st[2][kNumParams];
        for (int p = 0; p < 2; ++p) for (int i = 0; i < kNumParams; ++i) st[p][i] = partVals[p][(size_t) i].load(std::memory_order_relaxed);
        jpurrr::composePartVectors(vals, editPartA.load(), st, lastPv);
    }
    synth.setParams(lastPv[0]); synth.setPartParams(0, lastPv[0]); synth.setPartParams(1, lastPv[1]);
    soundFx.setParams(lastPv[0]); fx.setParams(vals);
    // Tempo: the host's when it gives one, else the ARPEGGIATOR TEMPO knob (Standalone has a play head without a tempo).
    {
        double bpm = juce::jlimit(20.0, 250.0, (double) vals[P_atp]); bool playing = false; double ppq = 0.0;
        if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) {
            bpm = pos->getBpm().orFallback(bpm);
            playing = pos->getIsPlaying(); ppq = pos->getPpqPosition().orFallback(0.0);
        }
        synth.setTempo(bpm); soundFx.setTempo(bpm); fx.setTempo(bpm);
        synth.setTransport(playing, ppq); fx.setTransport(playing, ppq);
    }
    float* l = buf.getWritePointer(0); float* r = buf.getNumChannels() > 1 ? buf.getWritePointer(1) : l;
    int done = 0;
    auto renderTo = [&](int end) {
        if (end > done) {
            const int len = end - done;
            synth.process(l + done, r + done, len);
            fx.processPreGate(l + done, r + done, len);      // gate position BEFORE FX: before the Sound page effects
            soundFx.process(l + done, r + done, len);
            fx.process(l + done, r + done, len);
            done = end;
        }
    };
    // notes from the editor's keyboard strip play first, at the start of the block
    if (stripPanic.exchange(false)) synth.allNotesOff();
    {
        int s1, n1, s2, n2; stripFifo.prepareToRead(stripFifo.getNumReady(), s1, n1, s2, n2);
        auto play = [&](const StripEvent& e) {
            if (e.kind == 0) { if (synth.heldVoices() == 0) fx.restartGate(); synth.stripNoteOn(e.note, e.vel); }
            else synth.stripNoteOff(e.note);
        };
        for (int i = 0; i < n1; ++i) play(stripData[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) play(stripData[(size_t) (s2 + i)]);
        stripFifo.finishedRead(n1 + n2);
    }
    bool wake = false;
    auto pushUpdate = [&](int param, int value) {
        int s1, n1, s2, n2; midiFifo.prepareToWrite(1, s1, n1, s2, n2);
        if (n1 > 0) { midiFifoData[(size_t) s1] = { param, value }; midiFifo.finishedWrite(1); wake = true; }
    };
    for (const auto meta : midi) {
        renderTo(meta.samplePosition);
        const auto m = meta.getMessage(); const int ch = m.getChannel() - 1;
        if (m.isNoteOn()) { if (synth.heldVoices() == 0) fx.restartGate(); synth.noteOn(ch, m.getNoteNumber(), m.getVelocity()); }
        else if (m.isNoteOff()) synth.noteOff(ch, m.getNoteNumber());
        else if (m.isPitchWheel()) synth.pitchBend(ch, m.getPitchWheelValue() - 8192);
        else if (m.isChannelPressure()) synth.channelPressure(ch, m.getChannelPressureValue());
        else if (m.isController()) {
            // The map turns CCs, NRPN and RPN into parameter changes (CC 1 moves the mod lever, which the synth already reads).
            const auto res = mappers[(size_t) juce::jlimit(0, 15, ch)].controlChange(m.getControllerNumber(), m.getControllerValue(), vals);
            for (int i = 0; i < res.count; ++i) { pushUpdate(res.updates[i].param, res.updates[i].value); vals[res.updates[i].param] = res.updates[i].value; }
            if (res.sustain >= 0) synth.controller(ch, 64, res.sustain ? 127 : 0);
            if (res.allSoundOff) synth.controller(ch, 120, 0);
            if (res.resetControllers) synth.controller(ch, 121, 0);
            if (res.allNotesOff) synth.controller(ch, 123, 0);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff();
    }
    renderTo(n);
    midi.clear();
    const int want = synth.latencySamples() + soundFx.latencySamples() + fx.latencySamples();
    if (want != wantedLatency.load()) { wantedLatency.store(want); wake = true; }
    if (wake) triggerAsyncUpdate();
}

void JPurrr8000AudioProcessor::handleAsyncUpdate() {
    int s1, n1, s2, n2; const int ready = midiFifo.getNumReady();
    midiFifo.prepareToRead(ready, s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) setInt(midiFifoData[(size_t) (s1 + i)].param, midiFifoData[(size_t) (s1 + i)].value);
    for (int i = 0; i < n2; ++i) setInt(midiFifoData[(size_t) (s2 + i)].param, midiFifoData[(size_t) (s2 + i)].value);
    midiFifo.finishedRead(n1 + n2);
    const int want = wantedLatency.load();
    if (want != reportedLatency) { reportedLatency = want; setLatencySamples(want); }
    syncEditPart();                                  // KEY MODE or PANEL SELECT moved by the host or a MIDI controller
}

// State: the host parameters by id (so sessions keep loading when parameters are added), the sound name and where it
// came from, the editor width. kStateVersion changes only when the meaning of stored values changes.
static constexpr int kStateVersion = 2;       // 2: both part sounds are stored (children PART0 and PART1). Version 1 sessions load as before: both parts get the one sound.

static juce::ValueTree partNode(int part, const CurrentPreset& m, const std::function<int(int)>& value) {
    juce::ValueTree n("PART" + juce::String(part));
    n.setProperty("name", m.name, nullptr); n.setProperty("bank", m.bank, nullptr); n.setProperty("slot", m.slot, nullptr);
    n.setProperty("factory", m.factory, nullptr); n.setProperty("type", m.type, nullptr); n.setProperty("dirty", m.dirty, nullptr);
    n.setProperty("genres", m.genres.joinIntoString("|"), nullptr);
    juce::StringArray vs;                                          // PART values by id, so sessions keep loading when parameters are added
    for (int i = 0; i < kNumParams; ++i) if (isPartParam(i)) vs.add(juce::String(kParams[i].id) + "=" + juce::String(value(i)));
    n.setProperty("v", vs.joinIntoString(","), nullptr);
    return n;
}

void JPurrr8000AudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = apvts.copyState();
    state.setProperty("stateVersion", kStateVersion, nullptr);
    state.setProperty("presetName", presets.cur.name, nullptr);
    state.setProperty("presetBank", presets.cur.bank, nullptr);
    state.setProperty("presetSlot", presets.cur.slot, nullptr);
    state.setProperty("presetFactory", presets.cur.factory, nullptr);
    state.setProperty("presetType", presets.cur.type, nullptr);
    state.setProperty("presetDirty", presets.cur.dirty, nullptr);
    state.setProperty("presetGenres", presets.cur.genres.joinIntoString("|"), nullptr);
    state.setProperty("uiWidth", uiWidth, nullptr);
    state.setProperty("editPart", editPart(), nullptr);
    for (int p = 0; p < 2; ++p) state.appendChild(partNode(p, partMeta(p), [this, p](int i) { return partValue(p, i); }), nullptr);
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
        presets.cur.genres.clear(); presets.cur.genres.addTokens(st.getProperty("presetGenres", "").toString(), "|", ""); presets.cur.genres.removeEmptyStrings();
        uiWidth = (int) st.getProperty("uiWidth", 0);
        presetName = presets.cur.name;
        // the part nodes are not host parameters: take them out before the parameter tree is replaced
        juce::ValueTree pn[2] = { st.getChildWithName("PART0"), st.getChildWithName("PART1") };
        const bool hasParts = pn[0].isValid() && pn[1].isValid();
        for (auto& n : pn) if (n.isValid()) st.removeChild(n, nullptr);
        partBusy.store(true);
        apvts.replaceState(st);
        if (hasParts) {
            editPartA.store(juce::jlimit(0, 1, (int) st.getProperty("editPart", 0)));
            for (int p = 0; p < 2; ++p) {
                for (int i = 0; i < kNumParams; ++i) if (isPartParam(i)) partVals[p][(size_t) i].store(kParams[i].def);
                juce::StringArray vs; vs.addTokens(pn[p].getProperty("v", "").toString(), ",", "");
                for (auto& kv : vs) {
                    const int eq = kv.indexOfChar('='); if (eq < 1) continue;
                    const int idx = paramIndexById(kv.substring(0, eq).toRawUTF8());
                    if (idx >= 0 && isPartParam(idx)) partVals[p][(size_t) idx].store(juce::jlimit(kParams[idx].min, kParams[idx].max, kv.substring(eq + 1).getIntValue()));
                }
                CurrentPreset m;
                m.name = pn[p].getProperty("name", "Init").toString(); m.bank = pn[p].getProperty("bank", "A").toString(); m.slot = (int) pn[p].getProperty("slot", 1);
                m.factory = (bool) pn[p].getProperty("factory", true); m.type = pn[p].getProperty("type", "Lead").toString(); m.dirty = (bool) pn[p].getProperty("dirty", false);
                m.genres.addTokens(pn[p].getProperty("genres", "").toString(), "|", ""); m.genres.removeEmptyStrings();
                partMetas[p] = m;
            }
            presets.cur = partMetas[editPart()];                // the edited part's info is the current sound; its values are already on the panel
            presetName = presets.cur.name;
        } else {
            // a session saved before the parts existed: one sound, both parts play it
            const int e = jpurrr::effectiveEditPart(getInt(P_km), getInt(P_ps), kUpper);
            editPartA.store(e);
            for (int p = 0; p < 2; ++p) { for (int i = 0; i < kNumParams; ++i) if (isPartParam(i)) partVals[p][(size_t) i].store(getInt(i)); partMetas[p] = presets.cur; }
        }
        partBusy.store(false);
    }
}

juce::AudioProcessorEditor* JPurrr8000AudioProcessor::createEditor() { return createJPurrrEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JPurrr8000AudioProcessor(); }
