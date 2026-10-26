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
struct Font { Fam fam; float size; short weight; float ls; };           // ls = letter spacing in em
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
struct Page {
    int h;                          // design height of the window for this page
    Span<Sec> secs; Span<Text> texts; Span<Knob> knobs; Span<Slider> sliders; Span<Switch> switches;
    Span<Opts> opts; Span<Select> selects; Span<Canvas> canvases; Span<Tip> tips; Span<HBtn> hbtns;
    Span<Rect> mbps; Span<VText> vtexts; Span<Cat> cats;
    bool hasLcd; Lcd lcd;
};
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
extern const char* const kParamPages[7][13]; // EDIT pages 1..6 (index 0 unused): param ids, nullptr terminated
}
