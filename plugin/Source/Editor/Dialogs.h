// In-window dialogs of the mockup (scrim + panel): Guide, Settings, About, Save, Replace, Delete, Export, Import,
// Preset browser and the EDIT parameter pages. All sizes are the mockup's CSS values times its dialog zoom (1.3).
#pragma once
#include "Widgets.h"

namespace jp {

class Root;
class DialogFrame;

class DialogLayer : public juce::Component {
public:
    explicit DialogLayer(Root& r);
    ~DialogLayer() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool hitTest(int x, int y) override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

    bool isOpen() const { return panel != nullptr; }
    void close();
    void openGuide(); void openSettings(); void openAbout(); void openSave(); void openBrowser();
    void openExport(); void openImport(); void openDelete(); void openPage(int bank);
    void refresh();                                    // 30 Hz: live values on the pages, frees closed dialogs
    void lookChanged();
    void relayout();                                   // after the content of the open dialog changed size

private:
    friend class DialogFrame;
    Root& root;
    std::unique_ptr<DialogFrame> panel;
    std::vector<std::unique_ptr<juce::Component>> graveyard;     // closed dialogs are freed on the next tick (a button may close its own dialog)
    bool pageMode = false;
    void openReplace(const juce::String& name, const juce::String& type, const juce::StringArray& genres, const juce::StringArray& tags, const juce::String& bank);
    DialogFrame& begin(const juce::String& title, float widthCss, bool page = false);
    void finish(DialogFrame& f);
};

} // namespace jp
