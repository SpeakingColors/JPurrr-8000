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
    root.onDefaultSize = [this] { setScaleWidth((int) std::lround(layout::kDesignW * kDefaultScale)); };
    setResizable(true, true);
    setConstrainer(&constrainer);
    constrainer.setMinimumSize((int) (layout::kDesignW * kMinScale), 100);
    int w = proc.uiWidth > 0 ? proc.uiWidth : (int) std::lround(layout::kDesignW * kDefaultScale);
    if (proc.uiWidth <= 0)                          // first open: never wider than the screen
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) w = juce::jmin(w, d->userArea.getWidth() - 40);
    w = juce::jlimit((int) (layout::kDesignW * kMinScale), (int) (layout::kDesignW * kMaxScale), w);
    setPageHeightAspect();
    setScaleWidth(w);
    t0 = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz(30);
    if (juce::JUCEApplicationBase::isStandaloneApp())     // the computer keys play the keyboard strip: take keyboard focus when the window opens
        juce::Timer::callAfterDelay(400, [sp = juce::Component::SafePointer<juce::Component>(&root)] { if (sp != nullptr && sp->isShowing()) sp->grabKeyboardFocus(); });
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
void JPurrrEditor::timerCallback() {
    if (jp::Settings::get().power && !isShowing()) return;      // power saving: nothing to draw while hidden
    root.tick(juce::Time::getMillisecondCounterHiRes() * 0.001 - t0);
}

juce::AudioProcessorEditor* createJPurrrEditor(JPurrr8000AudioProcessor& p) { return new JPurrrEditor(p); }
