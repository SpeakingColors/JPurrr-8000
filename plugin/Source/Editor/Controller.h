// Panel logic of the mockup, ported: parameter edits with their side effects (setP/deps), the LCD texts, the display
// buttons (ACT), preset navigation, ribbon assign, user settings. One per editor, no drawing.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <set>
#include "../PluginProcessor.h"
#include "../PresetManager.h"

namespace jp {

struct Settings {                       // saved with the plugin settings, not with each preset
    bool bubbles = true, power = true;
    juce::String look { "jp" };
    static Settings& get();
    void load(); void save() const;
};

class Controller {
public:
    explicit Controller(JPurrr8000AudioProcessor& p);
    JPurrr8000AudioProcessor& proc;
    PresetManager& pm;

    // ---- parameters (indices of jpurrr::kParams; ids are cached)
    static int idx(const char* id);
    int get(int i) const;
    void user(int i, int v);            // the mockup's setP(id, v, true): side effects, LCD readout, dirty flag
    void quiet(int i, int v);           // setP(id, v, false): lever, ribbon, internal
    void fromDisplay(int i, int v);     // a display changed a value (gate pattern, crossover, bend pad...)
    void begin(int i); void end(int i); // host gesture
    juce::String fmt(int i) const;      // fmtV
    juce::String label(int i) const;    // label with single spaces
    bool isSaved(int i) const;

    // ---- LCD, header texts
    juce::String lcd1() const, lcd2() const;
    juce::String title() const;         // preset name (+ " *" when edited)
    juce::String info() const;          // "A001 · factory"
    void say(const juce::String& t);
    juce::String message() const;

    // ---- front panel buttons
    bool editOn = false, shiftOn = false, perform = false;
    enum Assign { kNone, kRib, kVel } assigning = kNone;
    bool lit(const juce::String& id) const; bool blink(const juce::String& id) const;
    void action(const juce::String& id);                // momentary button (ACT)
    void controlTouched(int i);                         // for RIBBON / VELOCITY ASSIGN
    int ribbonTarget() const { return ribT; }
    void ribbonBegin(); void ribbonEnd(); void ribbonApply();

    // ---- presets
    void load(int listIndex);
    void step(int d);
    void navTo(int g, int b, int n);
    struct Nav { int g, b, n; };
    Nav nav() const;
    void markDirty();
    void afterLoad();                                   // deps of every parameter (mockup depsAll)
    PresetEntry snapshot(const juce::String& name) const;
    // Save dialog and friends return false and say() why on failure
    bool saveAs(const juce::String& name, const juce::String& type, const juce::StringArray& genres, const juce::StringArray& tags,
                const juce::String& bank, bool overwrite);
    void renameCurrent(const juce::String& newName);
    bool deleteCurrent();
    bool importText(const juce::String& text, juce::String& err);
    // Isolated hooks for the SysEx / file formats (to be wired to core/include/jpurrr/sysex.h and preset_io.h).
    bool importFile(const juce::File& f, juce::String& err);       // .json now; .syx later
    bool exportPatch(const juce::File& f, juce::String& err);      // .json now; .syx later
    static juce::String exportText(const PresetEntry& e);

    // ---- hooks set by the editor
    std::function<void()> openSave, openBrowser, startRename, closeDialog, openDelete;
    std::function<void(int)> openPage;
    std::function<void(int)> touched;                   // user changed a parameter: displays show their parameter view

    juce::String lcd2Override; juce::uint32 lcd2Until = 0;
    juce::String msgText; juce::uint32 msgUntil = 0;
    bool velHas(int i) const { return velSet.count(i) > 0; }

private:
    int ribT = -1, ribBase = 0;
    std::set<int> velSet;
    void deps(int i);
    void touchLcd(int i);
    juce::String showL2() const;
};

} // namespace jp
