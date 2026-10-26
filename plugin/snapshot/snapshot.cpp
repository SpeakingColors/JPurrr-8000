// Headless snapshot tool: builds the real editor, selects each page or dialog and writes PNGs at design scale 1
// (2920 px wide). Compare them with tools/snap_mockup.js output. Usage: jpurrr_snapshot <outdir> [state...]
#include "PluginEditor.h"
#include "Editor/Fonts.h"
#include <juce_events/juce_events.h>

static void writePng(const juce::Image& img, const juce::File& f) {
    f.deleteFile(); juce::FileOutputStream o(f); juce::PNGImageFormat png; png.writeImageToStream(img, o);
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    juce::File out(argc > 1 ? argv[1] : "snap-plugin"); out.createDirectory();
    juce::StringArray states;
    for (int i = 2; i < argc; ++i) states.add(argv[i]);
    if (states.isEmpty()) states = { "sound", "chain", "master", "guide", "settings", "about", "save", "browser", "export", "import", "page1", "page2", "page4", "page6", "page7", "page8", "menu", "look_navy", "look_soda", "look_purr" };

    JPurrr8000AudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* editor = dynamic_cast<JPurrrEditor*>(ed.get());
    editor->setScaleWidth(2920);
    editor->content().displayEnv().animPhase = 0.0;        // freeze the oscillator scroll
    for (auto& s : states) {
        editor->content().applyState(s);
        editor->setScaleWidth(2920);
        editor->content().tick(0.0);
        auto img = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        writePng(img, out.getChildFile(s + ".png"));
        std::printf("%s %dx%d\n", s.toRawUTF8(), img.getWidth(), img.getHeight());
        editor->content().applyState("reset");
    }
    ed.reset();
    jp::fonts::clearCache();
    return 0;
}
