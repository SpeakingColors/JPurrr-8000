#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "jpurrr/params_gen.h"
#include "jpurrr/synth.h"
#include "jpurrr/fxchain.h"
#include "jpurrr/soundfx.h"
#include "jpurrr/live.h"
#include "jpurrr/midi_map.h"
#include "PresetManager.h"

// Parameter values are plain integers in the units of jpurrr::kParams (same as the mockup).
// Params flagged kNoSave in kParams (levers, display buttons, preview) are UI state, not host parameters:
// use getInt/setInt for every index, the processor routes it.
class JPurrr8000AudioProcessor : public juce::AudioProcessor, private juce::AsyncUpdater {
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

    juce::String presetName { "Init" };
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;                           // factory + user sounds and the current sound (message thread)
    jpurrr::Live live;                               // readouts for the editor displays (audio thread writes)
    int uiWidth = 0;                                 // editor width in pixels, saved with the state (0 = default)

private:
    void handleAsyncUpdate() override;               // message thread: MIDI controller changes and latency updates
    // MIDI controller changes travel from the audio thread to the message thread through this FIFO.
    static constexpr int kMidiFifo = 512;
    juce::AbstractFifo midiFifo { kMidiFifo };
    std::array<jpurrr::MidiUpdate, kMidiFifo> midiFifoData {};
    std::array<jpurrr::MidiMapper, 16> mappers;
    std::atomic<int> wantedLatency { 0 };
    int reportedLatency = 0;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    std::array<juce::RangedAudioParameter*, jpurrr::kNumParams> hostParams {};
    std::array<std::atomic<int>, jpurrr::kNumParams> uiVal;
    jpurrr::Synth synth; jpurrr::SoundFx soundFx; jpurrr::FxChain fx;
    double sr = 44100;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JPurrr8000AudioProcessor)
};
