// The plugin window: a native JUCE port of the approved mockup (docs/mockups/JPurrr-8000-mockup-v3.2.html).
#pragma once
#include "PluginProcessor.h"
#include "Editor/Root.h"

class JPurrrEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit JPurrrEditor(JPurrr8000AudioProcessor&);
    ~JPurrrEditor() override;
    void resized() override;
    void paint(juce::Graphics&) override {}
    jp::Root& content() { return root; }
    void setPageHeightAspect();                       // after a page change
    void setScaleWidth(int width);
private:
    JPurrr8000AudioProcessor& proc;
    jp::Root root;
    juce::ComponentBoundsConstrainer constrainer;
    juce::ComponentBoundsConstrainer* unused = nullptr;
    double t0 = 0;
    bool inResize = false;
    void timerCallback() override;
};
