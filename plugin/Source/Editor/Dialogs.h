// In-window dialogs of the mockup (scrim + panel): Guide, Settings, About, Save, Replace, Delete, Export, Import,
// Preset browser and the EDIT parameter pages.
#pragma once
#include "Widgets.h"

namespace jp {

class Root;

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
    void refresh();                                    // 30 Hz
    void lookChanged();
    class Panel; 

private:
    Root& root;
    std::unique_ptr<juce::Component> panel;
    bool pageMode = false;
    void show(std::unique_ptr<juce::Component> p, bool page = false);
    friend class Panel;
};

} // namespace jp
