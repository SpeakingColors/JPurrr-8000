// Host-facing MIDI control of the sound: CC table, NRPN and RPN handling. Pure C++17, no allocation,
// safe on the audio thread. Numbers come from the JP-8080 owner's manual (MIDI Implementation pp. 191 to 199
// and the Transmit/Receive Setting List p. 188, MODE2 column = the factory assignment of every knob).
// The manual lists no NRPN numbers for sound parameters. The NRPN scheme below is JPurrr's own and is
// NOT a Roland assignment (see docs/midi-and-sysex.md).
#pragma once
#include "jpurrr/params_gen.h"
#include <cstdint>
namespace jpurrr {

// Builds the lookup tables. Call once on the message thread before audio starts; later calls cost nothing.
// (The tables are built on first use otherwise, and a first use on the audio thread could allocate.)
void midiMapInit();

struct MidiCcEntry {
    int cc;                 // controller number
    const char* paramId;    // our parameter
    const char* roland;     // the manual's name for this controller on that knob
};
const MidiCcEntry* midiCcTable(int& count);     // knob assignments (MODE2 factory column)
int midiParamForCc(int cc);                     // parameter index for a plain CC, or -1 (also -1 for CC 7, 10, 65: see MidiMapper)
int midiCcForParam(int paramIndex);             // the CC that moves this parameter, or -1

// JPurrr's own NRPN numbers: NRPN = parameter index (MSB = index / 128, LSB = index % 128). Momentary buttons excluded.
int midiNrpnForParam(int paramIndex);           // -1 when the parameter has none
int midiParamForNrpn(int nrpn);                 // -1 when unassigned

// Scale a controller value into the range of a parameter. bits = 7 (CC) or 14 (NRPN data entry).
int midiScaleToParam(int paramIndex, int raw, int bits);
// 14 bit pitch bend (0..16383, centre 8192) to the live bend control (-64..63).
int midiBendToParam(int bend14);

struct MidiUpdate { int param; int value; };

// What one incoming controller change asks the processor to do. Nothing is allocated.
struct MidiMapResult {
    int count = 0;                  // number of entries in updates
    MidiUpdate updates[2];          // parameter changes (two for the pitch bend range)
    int sustain = -1;               // 0 or 1 when CC 64 was received
    bool allNotesOff = false;       // CC 123, 124, 125, 126, 127
    bool allSoundOff = false;       // CC 120
    bool resetControllers = false;  // CC 121: bend centre, mod 0, sustain off, RPN/NRPN unset
    bool handled() const { return count > 0 || sustain >= 0 || allNotesOff || allSoundOff || resetControllers; }
};

// One mapper per MIDI channel (it remembers RPN/NRPN selection and data entry bytes). Plain struct, no heap.
class MidiMapper {
public:
    MidiMapper() { reset(); }
    void reset();
    // current = the live parameter values (int[kNumParams]) or nullptr. Only CC 10 (pan) looks at it:
    // it moves the pan position (amp LFO 1 depth) when PAN is MANUAL, and is ignored otherwise.
    MidiMapResult controlChange(int cc, int value, const int* current = nullptr);
private:
    enum Mode : uint8_t { None, Rpn, Nrpn };
    Mode mode_ = None;
    int selMsb_ = 127, selLsb_ = 127;
    int dataMsb_ = 0, dataLsb_ = 0;
    bool haveMsb_ = false, haveLsb_ = false;
    MidiMapResult applyData(bool fromMsb);
};
}
