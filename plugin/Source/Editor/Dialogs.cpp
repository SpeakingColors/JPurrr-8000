#include "Dialogs.h"
#include "Root.h"
using namespace jp;

DialogLayer::DialogLayer(Root& r) : root(r) { setVisible(false); }
DialogLayer::~DialogLayer() = default;
void DialogLayer::paint(juce::Graphics&) {}
void DialogLayer::resized() {}
bool DialogLayer::hitTest(int, int) { return false; }
void DialogLayer::mouseDown(const juce::MouseEvent&) {}
bool DialogLayer::keyPressed(const juce::KeyPress&) { return false; }
void DialogLayer::close() {}
void DialogLayer::openGuide() {} void DialogLayer::openSettings() {} void DialogLayer::openAbout() {} void DialogLayer::openSave() {}
void DialogLayer::openBrowser() {} void DialogLayer::openExport() {} void DialogLayer::openImport() {} void DialogLayer::openDelete() {}
void DialogLayer::openPage(int) {}
void DialogLayer::refresh() {} void DialogLayer::lookChanged() {}
void DialogLayer::show(std::unique_ptr<juce::Component>, bool) {}
