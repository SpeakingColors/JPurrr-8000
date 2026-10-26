#include "PluginEditor.h"
#include "Editor/EditorFactory.h"
#include "Editor/Layout.h"
using namespace jp;

namespace {
constexpr float kDefaultScale = 0.72f;
constexpr float kMinScale = 0.5f, kMaxScale = 1.0f;
}

JPurrrEditor::JPurrrEditor(JPurrr8000AudioProcessor& p) : juce::AudioProcessorEditor(p), proc(p), root(p) {
    addAndMakeVisible(root);
    root.onPageChanged = [this] { setPageHeightAspect(); };
    setResizable(true, true);
    setConstrainer(&constrainer);
    constrainer.setMinimumSize((int) (layout::kDesignW * kMinScale), 100);
    int w = proc.uiWidth > 0 ? proc.uiWidth : (int) std::lround(layout::kDesignW * kDefaultScale);
    w = juce::jlimit((int) (layout::kDesignW * kMinScale), (int) (layout::kDesignW * kMaxScale), w);
    setPageHeightAspect();
    setScaleWidth(w);
    t0 = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz(30);
}
JPurrrEditor::~JPurrrEditor() { stopTimer(); }

void JPurrrEditor::setPageHeightAspect() {
    const double aspect = (double) layout::kDesignW / (double) root.designHeight();
    constrainer.setFixedAspectRatio(aspect);
    const int w = getWidth() > 0 ? getWidth() : (int) std::lround(layout::kDesignW * kDefaultScale);
    constrainer.setSizeLimits((int) (layout::kDesignW * kMinScale), (int) (layout::kDesignW * kMinScale / aspect), (int) (layout::kDesignW * kMaxScale), (int) (layout::kDesignW * kMaxScale / aspect));
    setScaleWidth(w);
}
void JPurrrEditor::setScaleWidth(int w) {
    const int h = (int) std::lround((double) w * root.designHeight() / layout::kDesignW);
    if (w != getWidth() || h != getHeight()) setSize(w, h); else resized();
}
void JPurrrEditor::resized() {
    if (inResize) return;
    const float s = (float) getWidth() / (float) layout::kDesignW;
    root.setBounds(0, 0, layout::kDesignW, root.designHeight());
    root.setTransform(juce::AffineTransform::scale(s));
    root.scale = s;
    proc.uiWidth = getWidth();
    root.repaint();
}
void JPurrrEditor::timerCallback() { root.tick(juce::Time::getMillisecondCounterHiRes() * 0.001 - t0); }

juce::AudioProcessorEditor* createJPurrrEditor(JPurrr8000AudioProcessor& p) { return new JPurrrEditor(p); }
