#!/usr/bin/env python3
"""Builds data/melody_gen.json: what the melody generator learned from the 22 uplifting trance melodies of the player library.

The generator (tools/melody_gen.js in the mockup, core/src/melody_gen.cpp in the plugin) never copies a bar of pitches. It only uses
  * rhythm bank: the bar rhythms (onset step, length in 16ths) found in the 22 melodies, sorted into five families,
  * progressions: the chord loops of the 22 melodies as semitone offsets from the key note, minor or major.
The melodies themselves (tools/trance_melodies.py, tools/build_player_library.py) are not changed by this script.
Run:  python3 tools/build_melody_gen.py        (writes data/melody_gen.json, then run: node tools/embed_melody_gen.js)
"""
import collections, json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import trance_melodies as T, build_player_library as B

FAMILIES = ['anthem', 'run', 'roller', 'stabs', 'sparse']   # style numbers 1..5 in the generator (0 = any)


def pieces():
    out = []
    for (gid, name, tonic, minor, chords, bpm, gate, bars) in T.NEW:
        bs = []
        for t in bars:
            n, _ = T.parse_bar(t); bs.append([(s, l) for s, l, _nm in n])
        out.append((gid, T._midi(tonic + '4') % 12, minor, chords, bs))
    for (gid, name, tonic, minor, chords, bpm, bars) in B.GRIDS:
        bs = []
        for t in bars:
            step, ns = 0, []
            for nm, n in B.parse_bar(t):
                if nm != 'r': ns.append((step, n))
                step += n
            bs.append(ns)
        out.append((gid, B.pcs(tonic), minor, chords, bs))
    return out


def family(sig):
    n = len(sig)
    if n >= 12 and all(l == 1 for _, l in sig[:12]): return 'roller'
    if any(l == 3 for _, l in sig): return 'stabs'
    gaps = sum(1 for i in range(n - 1) if sig[i][0] + sig[i][1] < sig[i + 1][0])
    if n <= 6 and gaps >= 1: return 'sparse'
    if n >= 7 and all(l <= 2 or (s >= 8 and l == 4) for s, l in sig): return 'run'
    if n >= 7 and sig[0][1] == 1: return 'run'
    return 'anthem'


def is_ending(sig):
    if not sig: return False
    s, l = sig[-1]
    return l >= 4 or s + l <= 12


def chord_off(ch, tonic_pc):
    root = T._midi(ch[0] + (ch[1] if len(ch) > 1 and ch[1] in '#b' else '') + '4') % 12
    rest = ch[2:] if len(ch) > 1 and ch[1] in '#b' else ch[1:]
    return [(root - tonic_pc) % 12, 'm' if rest.startswith('m') else 'M']


def main():
    P = pieces()
    bank = {f: collections.Counter() for f in FAMILIES}; ends = collections.Counter()
    for gid, tonic, minor, chords, bars in P:
        for bi, bar in enumerate(bars):
            sig = tuple(bar)
            if not sig: continue
            bank[family(sig)][sig] += 1
            if bi == len(bars) - 1 and is_ending(sig): ends[sig] += 1
    progs = {'minor': [], 'major': []}
    for gid, tonic, minor, chords, bars in P:
        p = [chord_off(c, tonic) for c in chords]
        key = 'minor' if minor else 'major'
        if p not in progs[key]: progs[key].append(p)
    out = {
        'note': 'Learned from the 22 uplifting trance melodies (rhythms and chord loops only). Written by tools/build_melody_gen.py; do not edit by hand.',
        'families': FAMILIES,
        'rhythms': {f: [[[s, l] for s, l in sig] + [[0, c]] for sig, c in bank[f].most_common()] for f in FAMILIES},
        'endings': [[[s, l] for s, l in sig] for sig, c in ends.most_common()],
        'progressions': progs,
    }
    # the last pair [0, count] of each rhythm is its weight, stored apart so the lists stay readable
    for f in FAMILIES:
        out['rhythms'][f] = [{'w': r[-1][1], 'r': r[:-1]} for r in out['rhythms'][f]]
    fn = os.path.join(ROOT, 'data', 'melody_gen.json')
    json.dump(out, open(fn, 'w'), separators=(',', ':'))
    print('wrote', os.path.relpath(fn, ROOT))
    for f in FAMILIES: print(' ', f, len(out['rhythms'][f]), 'rhythms,', sum(x['w'] for x in out['rhythms'][f]), 'bars')
    print('  endings', len(out['endings']), ' progressions minor', len(progs['minor']), 'major', len(progs['major']))
    for k in progs: 
        for p in progs[k]: print('   ', k, ' '.join('%d%s' % (o, q) for o, q in p))

main()
