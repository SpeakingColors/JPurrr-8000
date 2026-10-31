#!/usr/bin/env python3
"""Builds data/player_library.json for the built-in MIDI player.

Sources:
  data/player_import_supurrsaw.json  demos from SupurrSaw (same author, AGPL-3.0-or-later)
  the tables below                   uplifting trance melody grids written as 16 steps per bar,
                                     plus chord pad and bass backings for the same progressions

Note names use C4 = MIDI 60 (same as the keyboard readout). Events are [beat, length in beats, note, velocity 1..127].
The grids are written in the style of 90s/00s uplifting trance. They are not copies of any track.
Run:  python3 tools/build_player_library.py   (check only: add --check)
"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PC = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def midi(name):
    p = PC[name[0]]
    i = 1
    while i < len(name) and name[i] in '#b':
        p += 1 if name[i] == '#' else -1
        i += 1
    return (int(name[i:]) + 1) * 12 + p


def pcs(name):
    return midi(name + '4') % 12


# ---- melody grids -------------------------------------------------------------------------------------
# One bar = 4 groups of 4 steps (16th notes). "X" = note, "X-" = note held one more step, "-" = rest.
# Repairs (each is one step, the pasted grid was one step off): marked with (fix).
GRIDS = [
    # id, name, key tonic, minor?, chords, bpm, bars
    ('ost_fsm', 'Ostinato wave (F# minor)', 'F#', True, ['F#m', 'D', 'A', 'E'], 138, [
        "C#5- F#5- G#5- A5-  G#5- F#5- C#5- E5-",
        "D5-  F#5- G#5- A5-  G#5- F#5- D5-  E5-",
        "E5-  A5-  B5-  C#6- B5-  A5-  E5-  G#5-",
        "E5-  G#5- B5-  D6-  C#6- B5-  G#5- E5-"]),
    ('anthem_em', 'Soaring anthem (E minor)', 'E', True, ['Em', 'C', 'G', 'D'], 138, [
        "- - B5-  - B5- B5-  - E5 F#5 G5- A5-",
        "B5- B5-  A5- G5-  F#5- E5- - F#5 G5-",
        "D5- - -  D6- D6-  - C6 B5-  A5- B5-",       # (fix) group 3 was one step short
        "C6- B5-  A5- F#5- D5- F#5- G5- A5-"]),
    ('gate_am', 'Gated triplets (A minor)', 'A', True, ['F', 'G', 'Am', 'Am'], 140, [
        "A5 A5 A5 -  A5 A5 A5 -  C6 C6 C6 -  F5 F5 F5 -",
        "B5 B5 B5 -  B5 B5 B5 -  D6 D6 D6 -  G5 G5 G5 -",
        "C6 C6 C6 -  C6 C6 C6 -  E6 E6 E6 -  A5 A5 A5 -",
        "E6- D6-  C6- B5-  A5- G4-  B4- C5-"]),
    ('octave_dm', 'Octave pedal arp (D minor)', 'D', True, ['Dm', 'Bb', 'F', 'C'], 138, [
        "D5 D6 A5 D6  F5 D6 E5 D6  F5 D6 A5 D6  F5 D6 E5 D6",
        "D5 D6 Bb5 D6 F5 D6 G5 D6  F5 D6 Bb5 D6 F5 D6 G5 D6",
        "C5 C6 A5 C6  F5 C6 G5 C6  A5 C6 C6 C6  A5 C6 G5 C6",
        "C5 C6 G5 C6  E5 C6 F5 C6  G5 C6 E5 C6  D5 C5 B4 A4"]),
    ('sparse_am', 'Sparse pluck with echoes (A minor)', 'A', True, ['Am', 'G', 'F', 'Em'], 138, [
        "- - E5-  - - A5-  - - B5-  - C6- -",
        "B5- - -  - - G5-  - - E5-  - D5- -",
        "A4- - -  - - C5-  - - E5-  - A5- -",
        "G5- F5-  E5- D5-  B4- C5-  D5- E5-"]),
    ('syncop_fsm', 'Syncopated turnaround (F# minor)', 'F#', True, ['F#m', 'D', 'Bm', 'C#m'], 138, [
        "C#5- - F#5  - C#5- F#5  - C#5 G#5 A5  - B5 C#6-",
        "D5- - F#5   - D5- F#5   - D5 A5 B5    - C#6 D6-",
        "D5- - F#5   - D5- F#5   - D5 G#5 A5   - B5 C#6-",
        "C#5- - E5   - C#5- E5   - G#5- A5     B5- C#6-"]),   # (fix) group 3 was one step long
    ('drift_em', 'Deep drifting notes (E minor)', 'E', True, ['Em', 'Bm', 'C', 'D'], 136, [
        "E5- - -   - - B5-  - - A5-  G5- F#5-",
        "F#5- - -  - - D5-  - - E5-  F#5- G5-",
        "G5- - -   - - E5-  - - C5-  D5- E5-",
        "A5- G5-   F#5- E5- D5- C5-  B4- D5-"]),
    ('run_csm', 'Anthemic run (C# minor)', 'C#', True, ['C#m', 'A', 'B', 'G#m'], 138, [
        "G#5- C#6- D#6- E6-  D#6- C#6- G#5- B5-",
        "A5-  C#6- D#6- E6-  D#6- C#6- A5-  B5-",
        "B5-  D#6- E6-  F#6- E6-  D#6- B5-  D#6-",
        "G#5- B5-  D#6- F#6- E6-  D#6- B5-  G#5-"]),
    ('roller_gm', 'Hypnotic 16th roller (G minor)', 'G', True, ['Gm', 'F', 'Eb', 'D'], 138, [
        "G5 G5 D5 G5  Bb5 G5 D5 G5  C5 G5 Bb5 G5  A5 F#5 D5 F#5",
        "F5 F5 C5 F5  A5 F5 C5 F5   Bb5 F5 A5 F5  G5 E5 C5 E5",
        "Eb5 Eb5 Bb4 Eb5  G5 Eb5 Bb4 Eb5  A5 Eb5 G5 Eb5  F5 D5 Bb4 D5",
        "D5 D5 A4 D5  F#5 D5 A4 D5  G5 D5 A5 D5   B5 D5 C6 D5"]),
    ('pedal_gm', 'Pedal tone arp (G minor)', 'G', True, ['Gm', 'Eb', 'Bb', 'F'], 138, [
        "G5 D5 Bb5 D5  C5 D5 Bb5 D5  A5 D5 G5 D5  F#5 D5 A5 D5",
        "G5 D5 Bb5 D5  C5 D5 Bb5 D5  A5 D5 G5 D5  Eb5 D5 G5 D5",
        "F5 D5 Bb5 D5  C5 D5 D5 D5   Eb5 D5 D5 D5  F5 D5 Bb5 D5",
        "A5 D5 C6 D5   Bb5 D5 A5 D5  G5 D5 F5 D5  Eb5 D5 D5 D5"]),
    ('hook_am', 'Classic hook (A minor)', 'A', True, ['Am', 'F', 'C', 'G'], 138, [
        "- - E5-   E5- E5-   - A4 B4-  C5- D5-",         # (fix) group 3 was one step short
        "E5- E5-   D5- C5-   B4- A4-   - B4 C5-",
        "G4- - -   G5- G5-   - F5 E5-  D5- E5-",         # (fix) group 3 was one step short
        "F5- E5-   D5- B4-   G4- B4-   C5- D5-"]),
]
# Left out because their grids do not add up to 16 steps in a way that can be repaired without guessing:
#   "overlapping pluck (F# minor)", "scalar run (A minor)", "gated hook (A minor)", "vocal airscape hook (F# minor)"

# ---- backings -----------------------------------------------------------------------------------------
PROGS = [  # id, name, tonic, chords (one per bar, 4 or 8), bpm
    ('am_f_c_g', 'Am F C G', ['Am', 'F', 'C', 'G'], 138),
    ('fm_epic', 'Fm Db Ab Eb Bbm Fm Db Eb', ['Fm', 'Db', 'Ab', 'Eb', 'Bbm', 'Fm', 'Db', 'Eb'], 138),
    ('gm_lift', 'Gm Eb Cm F Bb Eb Cm D', ['Gm', 'Eb', 'Cm', 'F', 'Bb', 'Eb', 'Cm', 'D'], 138),
    ('bm_drive', 'Bm G D A Em Bm G A', ['Bm', 'G', 'D', 'A', 'Em', 'Bm', 'G', 'A'], 138),
    ('csm_neo', 'C#m A E B F#m C#m A B', ['C#m', 'A', 'E', 'B', 'F#m', 'C#m', 'A', 'B'], 138),
    ('c_g_am_f', 'C G Am F (major key anthem)', ['C', 'G', 'Am', 'F'], 138),
    # progressions of the hand-written melodies in tools/trance_melodies.py (same bars, same key)
    ('lift_am', 'Am F C G Am F G Am', ['Am', 'F', 'C', 'G', 'Am', 'F', 'G', 'Am'], 138),
    ('descent_bm', 'Bm G D A Bm G D A', ['Bm', 'G', 'D', 'A', 'Bm', 'G', 'D', 'A'], 138),
    ('resolve_em', 'Em C Am B Em C Am B', ['Em', 'C', 'Am', 'B', 'Em', 'C', 'Am', 'B'], 138),
    ('stabs_csm', 'C#m A E B C#m A E B', ['C#m', 'A', 'E', 'B', 'C#m', 'A', 'E', 'B'], 138),
    ('anthem_c', 'C G Am F C G F G', ['C', 'G', 'Am', 'F', 'C', 'G', 'F', 'G'], 138),
    ('arp_lift_am', 'F G Am Am F G Am E', ['F', 'G', 'Am', 'Am', 'F', 'G', 'Am', 'E'], 138),
]


def chord_pcs(ch):
    root = ch[0]
    i = 1
    while i < len(ch) and ch[i] in '#b':
        root += ch[i]
        i += 1
    minor = ch[i:] == 'm'
    r = pcs(root)
    return r, [r, (r + (3 if minor else 4)) % 12, (r + 7) % 12]


def voice(prev, ch):
    """triad inversion nearest to the previous chord, inside C3..C5 (48..72)"""
    r, tri = chord_pcs(ch)
    cands = []
    for inv in range(3):
        n = tri[inv:] + tri[:inv]
        for base in (48, 55, 60):
            v, last = [], base - 1
            for p in n:
                x = last + 1
                while x % 12 != p:
                    x += 1
                v.append(x)
                last = x
            if v[0] >= 48 and v[-1] <= 72:
                cands.append(v)
    if not prev:
        return min(cands, key=lambda v: abs(sum(v) / 3 - 60))
    return min(cands, key=lambda v: sum(abs(a - b) for a, b in zip(sorted(v), sorted(prev))))


def bass_note(ch, lo, hi):  # root pitch inside lo..hi (MIDI numbers, C4 = 60)
    r, _ = chord_pcs(ch)
    n = r
    while n < lo:
        n += 12
    while n > hi:
        n -= 12
    return n


def backings():
    out = []
    for pid, name, chords, bpm in PROGS:
        bars = len(chords)
        # pad: voice-led triads, whole bars
        ev, prev = [], None
        for b, ch in enumerate(chords):
            v = voice(prev, ch)
            prev = v
            for n in v:
                ev.append([b * 4, 3.9, n, 80])
        out.append(dict(id='bk_pad_' + pid, group='Uplifting trance backings', name='Pad chords, voice led: ' + name, bpm=bpm, ev=ev, beats=bars * 4))
        # sub bass: offbeat eighths, low sine range
        ev = []
        for b, ch in enumerate(chords):
            n = bass_note(ch, 28, 39)
            for beat in range(4):
                ev.append([b * 4 + beat + 0.5, 0.4, n, 105])
        out.append(dict(id='bk_sub_' + pid, group='Uplifting trance backings', name='Sub bass, offbeats: ' + name, bpm=bpm, ev=ev, beats=bars * 4))
        # rolling mid bass: 16ths without the kick step
        ev = []
        for b, ch in enumerate(chords):
            n = bass_note(ch, 40, 51)
            for step in range(16):
                if step % 4:
                    ev.append([b * 4 + step * 0.25, 0.2, n, 100 if step % 4 == 2 else 85])
        out.append(dict(id='bk_roll_' + pid, group='Uplifting trance backings', name='Rolling mid bass: ' + name, bpm=bpm, ev=ev, beats=bars * 4))
    # galop bass for the first progression only
    pid, name, chords, bpm = PROGS[0]
    ev = []
    for b, ch in enumerate(chords):
        n = bass_note(ch, 40, 51)
        for step in range(1, 16, 2):
            ev.append([b * 4 + step * 0.25, 0.2, n, 100])
    out.append(dict(id='bk_galop_' + pid, group='Uplifting trance backings', name='Galop bass: ' + name, bpm=bpm, ev=ev, beats=16))
    return out


# ---- parse grids ---------------------------------------------------------------------------------------
def parse_bar(text):
    toks = text.split()
    slots = []
    for t in toks:
        if t == '-':
            slots.append(('r', 1))
        elif t.endswith('-'):
            slots.append((t[:-1], 2))
        else:
            slots.append((t, 1))
    return slots


def melody(gid, name, tonic, minor, chords, bpm, bars):
    ev, problems = [], []
    scale = {(pcs(tonic) + i) % 12 for i in ((0, 2, 3, 5, 7, 8, 10, 11, 9) if minor else (0, 2, 4, 5, 7, 9, 11))}
    for b, text in enumerate(bars):
        slots = parse_bar(text)
        total = sum(n for _, n in slots)
        if total != 16:
            problems.append('%s bar %d has %d steps' % (gid, b + 1, total))
        step = 0
        for nm, n in slots:
            if nm != 'r':
                m = midi(nm)
                r, tri = chord_pcs(chords[b])
                if m % 12 not in scale | set(tri) | {(r + 9) % 12}:
                    problems.append('%s bar %d: %s is outside the key and the chord' % (gid, b + 1, nm))
                accent = (step % 4 == 2)
                ln = 0.25 * n * (0.8 if n == 1 else 0.92)
                ev.append([b * 4 + step * 0.25, round(ln, 3), m, 110 if accent else 92])
            step += n
    return dict(id='tr_' + gid, group='Trance riffs (test patterns)', name=name + ', ' + '-'.join(chords), bpm=bpm, ev=ev, beats=len(bars) * 4), problems


def cstr(t):
    return '"' + t.replace('\\', '\\\\').replace('"', '\\"') + '"'


def write_cpp(lib):
    L = ['// Generated by tools/build_player_library.py from data/player_library.json. Do not edit.',
         '#include "jpurrr/player.h"', 'namespace jpurrr {', 'namespace {']
    items, groups, k = [], [], 0
    for g in lib['groups']:
        groups.append((g['name'], k, len(g['items'])))
        for d in g['items']:
            body = ','.join('{%.3ff,%.3ff,%d,%d}' % (e[0], e[1], e[2], e[3]) for e in sorted(d['ev'], key=lambda e: (e[0], e[2])))
            L.append('const PlayEvent kEv%d[] = {%s};' % (k, body))
            items.append((d['id'], d['name'], d['bpm'], d['beats'], k, len(d['ev'])))
            k += 1
    L.append('const LibItem kItems[] = {')
    for i, (id_, nm, bpm, beats, kk, n) in enumerate(items):
        L.append('  {%s,%s,%d,%.1ff,kEv%d,%d},' % (cstr(id_), cstr(nm), bpm, beats, kk, n))
    L.append('};')
    L.append('const LibGroup kGroups[] = {')
    for nm, first, cnt in groups:
        L.append('  {%s,%d,%d},' % (cstr(nm), first, cnt))
    L.append('};')
    L.append('}  // namespace')
    L.append('int libraryItemCount() { return %d; }' % len(items))
    L.append('const LibItem& libraryItem(int i) { return kItems[i < 0 ? 0 : i >= %d ? %d : i]; }' % (len(items), len(items) - 1))
    L.append('int libraryGroupCount() { return %d; }' % len(groups))
    L.append('const LibGroup& libraryGroup(int g) { return kGroups[g < 0 ? 0 : g >= %d ? %d : g]; }' % (len(groups), len(groups) - 1))
    L.append('int libraryFind(const char* id) { for (int i = 0; i < %d; ++i) { const char* a = kItems[i].id; const char* b = id; while (*a && *a == *b) { ++a; ++b; } if (!*a && !*b) return i; } return -1; }' % len(items))
    L.append('}  // namespace jpurrr')
    with open(os.path.join(ROOT, 'core', 'src', 'player_library_gen.cpp'), 'w') as f:
        f.write('\n'.join(L) + '\n')


def main():
    check = '--check' in sys.argv
    demos, problems = [], []
    for g in GRIDS:
        d, p = melody(*g)
        demos.append(d)
        problems += p
    from trance_melodies import NEW, build
    new = []
    for g in NEW:
        d, p = build(*g)
        d['group'] = 'Uplifting trance melodies'
        new.append(d)
        problems += p
    demos = new + demos
    demos += backings()
    imp = json.load(open(os.path.join(ROOT, 'data', 'player_import_supurrsaw.json')))['demos']
    for x in imp:
        ev = [[round(e[0], 3), round(e[1], 3), int(e[2]), int(round((e[3] if len(e) > 3 else 0.8) * 127))] for e in x['ev']]
        end = max(e[0] + e[1] for e in ev)
        demos.append(dict(id=x['id'], group=x['group'], name=x['name'], bpm=int(x['bpm']), ev=ev, beats=max(4, int(-(-end // 4)) * 4)))
    seen = set()
    for d in demos:
        if d['id'] in seen:
            problems.append('duplicate id ' + d['id'])
        seen.add(d['id'])
        for e in d['ev']:
            if not (0 <= e[2] <= 127 and 1 <= e[3] <= 127 and e[1] > 0 and e[0] >= 0):
                problems.append('bad event in %s: %s' % (d['id'], e))
    order = []
    for d in demos:
        if d['group'] not in order:
            order.append(d['group'])
    # trance groups first
    first = ['Uplifting trance melodies', 'Uplifting trance backings', 'Trance riffs (test patterns)']
    order = first + [g for g in order if g not in first]
    lib = dict(format=1, note='Built by tools/build_player_library.py. Events: [beat, length in beats, MIDI note (60 = C4), velocity 1..127].',
               groups=[dict(name=g, items=[d for d in demos if d['group'] == g]) for g in order])
    for g in lib['groups']:
        for d in g['items']:
            d.pop('group')
    print('%d items in %d groups' % (len(demos), len(order)))
    for p in problems:
        print('PROBLEM:', p)
    if check:
        return 1 if problems else 0
    with open(os.path.join(ROOT, 'data', 'player_library.json'), 'w') as f:
        json.dump(lib, f, separators=(',', ':'))
        f.write('\n')
    write_cpp(lib)
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
