#include "jpurrr/parts.h"
#include <cstring>
namespace jpurrr {
namespace {
struct Table {
    PartClass c[kNumParams];
    Table() {
        static const char* const globX[] = { "vc", "vas", "oas", "vol", "aon", "ahold", "atp", "amd", "arg", "adst", "apt", "its", "itd", "itc", "itn", "mc1", "mc2",
                                             "os", "ksh", "loc", "msy", "did", "rxe", "pcc", "rkc", "thr", "txe", "txm", "tpc", "lcc", "mtu", "pum", "ptq", "mrs" };
        for (int i = 0; i < kNumParams; ++i) {
            const char* id = kParams[i].id;
            if (kParams[i].flags & (kNoSave | kMomentary)) { c[i] = PartClass::None; continue; }
            if (!std::strcmp(id, "km") || !std::strcmp(id, "ps") || !std::strcmp(id, "sp") || !std::strcmp(id, "pdet")) { c[i] = PartClass::Perf; continue; }
            bool g = id[0] == 'q' || id[0] == 'g';        // MIX READY chain (q...) and trance gate (g...)
            for (const char* x : globX) g = g || !std::strcmp(id, x);
            c[i] = g ? PartClass::Glob : PartClass::Part;
        }
    }
};
const Table& table() { static Table t; return t; }
}

PartClass partClassOf(int i) { return i < 0 || i >= kNumParams ? PartClass::None : table().c[i]; }

int effectiveEditPart(int km, int ps, int current) {
    if (km == 0) return kUpper;
    if (ps == 0) return kLower;
    if (ps == 3) return current == kLower ? kLower : kUpper;
    return kUpper;
}

int editedParts(int km, int ps, int current, int out[2]) {
    if (km == 0) { out[0] = kUpper; return 1; }
    if (ps == 2) { out[0] = kUpper; out[1] = kLower; return 2; }
    if (ps == 3) return 0;
    out[0] = effectiveEditPart(km, ps, current); return 1;
}

void copyPartValues(const int* from, int* to) {
    for (int i = 0; i < kNumParams; ++i) if (partClassOf(i) == PartClass::Part) to[i] = from[i];
}

void composePartVectors(const int* panel, int editPart, const int store[2][kNumParams], int out[2][kNumParams]) {
    for (int p = 0; p < 2; ++p) {
        std::memcpy(out[p], panel, sizeof(int) * kNumParams);
        if (p != editPart) copyPartValues(store[p], out[p]);
    }
}

}
