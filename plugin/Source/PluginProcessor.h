#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "jpurrr/params_gen.h"
#include "jpurrr/synth.h"
#include "jpurrr/fxchain.h"
#include "jpurrr/soundfx.h"
#include "jpurrr/live.h"
#include "jpurrr/midi_map.h"
#include "jpurrr/keyboard.h"
#include "jpurrr/parts.h"
#include "PresetManager.h"

// Parameter values are plain integers in the units of jpurrr::kParams (same as the mockup).
// Params flagged kNoSave in kParams (levers, display buttons, preview) are UI state, not host parameters:
// use getInt/setInt for every index, the processor routes it.
class JPurrr8000AudioProcessor : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::AudioProcessorParameter::Listener {
public:
    JPurrr8000AudioProcessor();
    ~JPurrr8000AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "JPurrr-8000"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // --- value access for the editor and the preset code (message thread) ---
    int  getInt(int idx) const;
    void setInt(int idx, int value);                 // sets and notifies the host
    void beginGesture(int idx); void endGesture(int idx);
    void setAll(const int* values);                  // whole patch at once (preset load)
    void getAll(int* values) const;
    juce::AudioProcessorParameter* hostParam(int idx) const { return hostParams[(size_t) idx]; }

    // --- the two parts (mockup v3.5). The panel and the host parameters hold the PERF and GLOB values and the PART values of the
    // edited part (editPart()); the other part keeps its PART values and its preset info in a store. The engine plays both. ---
    int editPart() const { return editPartA.load(); }
    const CurrentPreset& partMeta(int part) const;                 // the sound of a part (the edited part: presets.cur)
    int  partValue(int part, int idx) const;                       // a PART value of either part (the edited part: the panel)
    bool syncEditPart();                                           // KEY MODE or PANEL SELECT changed: move the other part's sound onto the panel. True when it switched
    void storePanelPart();                                         // copy the edited part's panel values into its store (before a load)
    void putPart(int part, const int* values, const CurrentPreset& meta);   // a sound into a part's store (not onto the panel)
    void loadPartToPanel();                                        // the panel shows the store of the edited part
    void mirrorPartValue(int idx, int value);                      // PANEL SELECT BOTH: the same edit in the other part
    void markPartDirty(int part);
    juce::String presetName { "Init" };
    // --- the editor's keyboard strip (message thread only; lock-free queue, the audio thread plays them at the start of the next block) ---
    void stripNoteOn(int note, int velocity = 100);
    void stripNoteOff(int note);

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;                           // factory + user sounds and the current sound (message thread)
    jpurrr::Live live;                               // readouts for the editor displays (audio thread writes)
    int uiWidth = 0;                                 // editor width in pixels, saved with the state (0 = default)

private:
    void handleAsyncUpdate() override;               // message thread: MIDI controller changes and latency updates
    void parameterValueChanged(int, float) override { triggerAsyncUpdate(); }     // KEY MODE or PANEL SELECT from the host
    void parameterGestureChanged(int, bool) override {}
    std::array<std::atomic<int>, jpurrr::kNumParams> partVals[2];                  // PART values of each part (the edited part's entry is only current after storePanelPart)
    CurrentPreset partMetas[2];                                                    // preset info of each part (the edited part's is presets.cur)
    std::atomic<int> editPartA { 0 };
    std::atomic<bool> partBusy { false };                                          // the panel is being switched to the other part: the audio thread keeps the last sound
    int lastPv[2][jpurrr::kNumParams] = {};                                        // audio thread
    void initParts();
    // MIDI controller changes travel from the audio thread to the message thread through this FIFO.
    static constexpr int kMidiFifo = 512;
    juce::AbstractFifo midiFifo { kMidiFifo };
    std::array<jpurrr::MidiUpdate, kMidiFifo> midiFifoData {};
    std::array<jpurrr::MidiMapper, 16> mappers;
    // Notes of the keyboard strip travel from the message thread to the audio thread through this FIFO (single producer).
    struct StripEvent { std::int8_t kind; std::int8_t note; std::int8_t vel = 100; };         // 0 note on, 1 note off
    static constexpr int kStripFifo = 256;
    juce::AbstractFifo stripFifo { kStripFifo };
    std::array<StripEvent, kStripFifo> stripData {};
    std::atomic<bool> stripPanic { false };                            // a note off did not fit in the FIFO: release everything
    void pushStrip(StripEvent e);
    std::atomic<int> wantedLatency { 0 };
    int reportedLatency = 0;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    std::array<juce::RangedAudioParameter*, jpurrr::kNumParams> hostParams {};
    std::array<std::atomic<int>, jpurrr::kNumParams> uiVal;
    jpurrr::Synth synth; jpurrr::SoundFx soundFx; jpurrr::FxChain fx;
    double sr = 44100;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JPurrr8000AudioProcessor)
};
