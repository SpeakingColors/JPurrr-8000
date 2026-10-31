// Mockup geometry (design pixels, window = 2920 wide) generated from data/layout.json by tools/gen_layout.py.
// Do not edit layout_gen.cpp by hand: run `node tools/dump_layout.js && python3 tools/gen_layout.py`.
#pragma once
#include <cstdint>
namespace jp::layout {

template <class T> struct Span {
    const T* p = nullptr; int n = 0;
    const T* begin() const { return p; }
    const T* end() const { return p + n; }
    const T& operator[](int i) const { return p[i]; }
};

struct Rect { float x, y, w, h; };
enum Fam : uint8_t { kBody = 0, kCond = 1, kMono = 2 };
struct Font { Fam fam; float size; short weight; float ls; bool italic; };           // ls = letter spacing in em
struct Line { const char* t; float x, w, base; };                        // x,w: measured by the browser, base = baseline y
struct Text { const char* id; Font f; char al; signed char tok; uint32_t col; Rect r; Span<Line> lines; };
struct Knob { const char* id; Rect r; };
struct Slider { const char* id; Rect r; int hh; float tk; };
struct Switch { const char* id; Rect r; bool mom; };
struct Opt { int v; Rect r; Rect led; };
struct Opts { const char* id; Rect r; Span<Opt> opts; };
struct Select { const char* id; const char* mp; Rect r; Font f; int n; };
struct Canvas { const char* id; Rect r; int cw, ch; };
struct Sec { Rect r; const char* title; Rect ttl; Rect span; };
struct Tip { Rect r; const char* tip; const char* forId; };
struct HBtn { const char* id; Rect r; const char* t; bool tab, sq; };
struct VText { const char* t; Rect r; Font f; };
struct Lcd { Rect r, l1, l2; Font f; };
struct Cat { Rect r; };
// v3.3: SAVE and "..." (rename or delete) buttons right of each FX block preset select. b.id is "mpsave_<mp>" or "mpmore_<mp>"
// (the SAVE caption is also a Text with that id in Page::texts, as with HBtn). b.t is "SAVE" or "..." (b.sq true for "...").
struct BlockBtn { const char* mp; bool more; HBtn b; Font f; const char* tip; };
struct Page {
    int h;                          // design height of the window with the keyboard strip shown (PREVIEW on, the default)
    int hBare;                      // design height with PREVIEW off (strip hidden): the panels end 4 px above the footer
    float stripY;                   // top of the keyboard strip in the window when it is shown (8 px under the lowest panel)
    Span<Sec> secs; Span<Text> texts; Span<Knob> knobs; Span<Slider> sliders; Span<Switch> switches;
    Span<Opts> opts; Span<Select> selects; Span<Canvas> canvases; Span<Tip> tips; Span<HBtn> hbtns;
    Span<Rect> mbps; Span<VText> vtexts; Span<Cat> cats; Span<BlockBtn> bbtns;
    bool hasLcd; Lcd lcd;
};

// v3.3 keyboard strip (#kbd): BEND and MOD wheels and the keys C2..C7 (MIDI 36..96, 61 keys). x is a window x. All y values are
// relative to the top of the strip: add Page::stripY. The strip is as wide as the centre panel and 166 tall (rect.h).
// Wheel: r is the wheel body (rounded 8, 1 px border). The handle (4 px bar, inset 2 px left and right) is at rest in `handle`;
// at wheel position wn (0..1, 1 = top) its top is r.y + 1 + 6 + (1 - wn) * travel. `line` is the 1 px centre mark of the BEND wheel
// (zero size on MOD). name = the BEND / MOD caption box, scale = box of the + / - or MAX / 0 captions (all captions are in KeyStrip::texts).
// wn at rest is 0.5 for BEND and 0 for MOD.
struct Wheel { const char* id; bool pitch; Rect r; Rect handle; Rect line; Rect name; Rect scale; float wn; float travel; const char* label; const char* tip; };
struct Key { int note; bool black; Rect r; };                               // white keys first, then black keys (draw in this order)
struct KeyLabel { int note; const char* t; float x, w, base; };            // "C2".."C7": centred on x + w / 2 at baseline base, font KeyStrip::labelFont, colour labelCol
struct StripKnob { const char* id; Rect r; Rect tipRect; const char* tip; };      // v3.8: BEND UP / BEND DOWN (r = the knob, tipRect = knob and caption); y relative to the strip
struct KeyStrip {
    Rect r;                         // the strip box (x, 0, w, 166): 6 px radius, pnl3 fill, 1 px sbd border
    Rect keysRect;                  // #keys: dark box (0b0e13) with 1 px edge border, 3 px radius; the keys are clipped to it
    Rect whls;                      // box around both wheels with their captions
    int firstNote, lastNote;        // 36 and 96
    Font labelFont; uint32_t labelCol;
    Span<Wheel> wheels;             // [0] = BEND (id wBend), [1] = MOD (id wMod)
    Span<Key> keys;
    Span<KeyLabel> labels;
    Span<Text> texts;               // wheel captions BEND, MOD, +, -, MAX, 0 and the knob captions BEND UP, BEND DOWN (Text::tok colours, y relative to the strip)
    Span<StripKnob> knobs;          // v3.8: [0] = bu, [1] = bd, left of the BEND wheel
};
// v3.5 PARTS band (LOWER card, KEYS map, UPPER card) on the SOUND page while EDIT PARTS is on. Rectangles: x is a window x, y is relative to the
// top of the band (kParts.top in the window). Inner boxes of a card or the map are relative to the card or map. base = text baseline in the same frame.
// The texts (sound names, chips, key mode ...) change with the state and are drawn by the plugin; only boxes and baselines come from the mockup.
struct PBox { Rect r; float base; };
struct PCard { Rect r; PBox led, title, role, ed, prev, name, next, browse, chipsRow, chip, chipId; };
struct PMap { Rect r; PBox title, pill, text, sp, mini, oct, octI; };
struct PartsGeo {
    float top;                      // y of the band in the window (also where the strip starts while the band is closed)
    float extra;                    // window height added while the band is shown (band + the 8 px gap)
    float stripY;                   // top of the keyboard strip while the band is shown
    Sec sec;                        // the PARTS frame: r, title, ttl, span
    PCard lower, upper;             // lower is the left card
    PMap map;
    Span<HBtn> btns; Span<Text> btnTexts;   // pt_prev_l, pt_next_l, pt_browse_l, then the same for u
};
// v3.8 PLAYER bar (#plr) above the keyboard strip while the PLAYER switch (CONTROLLER panel) is on. x is a window x, y is relative to the top of the bar.
// base = text baseline in the same frame. Texts that change (piece name, group, tempo, readouts) are drawn by the plugin: only boxes, fonts and baselines come from the mockup.
struct PlLine { float x, w, base; };
struct PlMenu {                     // #plMenu, opens upwards from the piece box: constants of its CSS
    float w, border, radius, groupH, groupPad, rowH, rowPadL, rowPadR, rowGap;
    Font groupFont, groupSmall, rowFont, rowSmall;
    float above, minTop, maxH, minH, left;
};
struct PlayerGeo {
    float h;                        // 40
    float extra;                    // window height added while the bar is shown (the bar and the 8 px gap)
    Rect bar; float radius;         // the bar box and its corner radius
    Font title, play, label, name, group, num, posSmall, posVal;   // PLAYER, PLAY / LOAD / EXPORT, BPM / LOCK / TRANSPOSE, piece name, group, number boxes, BAR, "1 / 8"
    PlLine tTitle;
    Rect playR, playIcon, stopIcon; PlLine playL, stopL;
    Rect prevR, prevIcon, nextR, nextIcon;
    Rect sel; Rect nameR; float nameBase; Rect grpR; float grpBase; Rect chev; float selPad;
    Rect bpmG, bpmR; float bpmBase; PlLine bpmL;
    Rect lockG, lockR, led; PlLine lockL;
    Rect trG, trR; float trBase; PlLine trL;
    Rect loadR, expR; PlLine loadL, expL;
    Rect posG, posR, posTR; PlLine barL; PlLine valL; int posW, posH;
    Rect seps[4];
    PlMenu menu;
    Span<Tip> tips;                 // data-tip of play, prev, sel, next, bpm group, lock group, transpose group, load, export, position group
};
extern const PlayerGeo kPlayer;
extern const PartsGeo kParts;
extern const KeyStrip kStrip;
extern const int kChromeH;          // window height in which the chrome (footer, cheeks) was dumped: footer shift = page height - kChromeH
enum PageId { kSound = 0, kChain = 1, kMaster = 2, kNumPages = 3 };
extern const Page kPages[kNumPages];
extern const Page kChrome;          // header, footer, cheeks (hbtn, texts, cat)
extern const Rect kHdr, kFtr, kCheekL, kCheekR, kPname, kPinfo, kMsg;
constexpr int kDesignW = 2920;

// Colours of the four looks (data-look): index with Tok.
enum Tok { pnl, pnl2, pnl3, edge, sbd, hi, ttl, lbl, lbl2, knob, knobhi, ind, cap, led, ledoff, acc, acc2, lcdbg, lcdfg, lcdoff,
           hdr, hdrfg, sel, selfg, dsp, dspline, dspfill, dspgrid, dspaccent, logo, btnbg, btnbd, wordmark, kNumToks };
constexpr int kNumLooks = 4;
extern const uint32_t kLooks[kNumLooks][kNumToks];   // ARGB
extern const char* const kLookIds[kNumLooks];        // jp navy soda purr

// Parameter value strings of params that have a custom format in the mockup (fmt), min..max.
struct FmtTable { const char* id; int min; int n; const char* const* s; };
extern const FmtTable kFmt[]; extern const int kNumFmt;

// FX block preset menus (mockup MPD) and sound types.
struct MpPreset { const char* name; int n; const int* vals; };      // vals follow ids order
struct MpDef { const char* key; const char* title; int nIds; const char* const* ids; int nList; const MpPreset* list; };
extern const MpDef kMp[]; extern const int kNumMp;
struct TypeVals { const char* type; int n; const char* const* ids; const int* vals; };  // qvals(type): sound type chain defaults
extern const TypeVals kQvals[]; extern const int kNumQvals;
extern const char* const kO1L[7][2]; extern const char* const kO2L[4][2];
extern const char* const kVmc[27];
extern const int kStartSlots[]; extern const int kNumStartSlots;      // factory 'START HERE' slots
extern const char* const kBanks[8]; extern const char* const kTypes[10]; extern const char* const kGenres[9];
extern const char* const kScope[]; extern const int kNumScope;          // SCOPE page: ids of the panel controls, in panel order
extern const char* const kParamPages[7][13]; // EDIT pages 1..6 (index 0 unused): param ids, nullptr terminated
}
