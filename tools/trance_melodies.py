"""Original uplifting trance melodies for the built-in player (written by hand, not copied from any track).
Format: one string per bar, 16 steps (16th notes) per bar. Tokens: NOTE:len (len in steps, default 1), '.' = one step rest, '.N' = N steps rest.
Written by hand. The pulse style pieces follow measured statistics of 2000s trance (see docs/trance-patterns-learned.md); no notes were copied.
Rules used when writing (and checked by validate()):
  * every bar adds up to exactly 16 steps
  * a note that starts on beat 1 or beat 3 of the bar (step 0 or 8) is a tone of that bar's chord
  * every note is in the key (minor keys also allow the raised 7th and 6th; the major V chord brings its own notes)
  * the highest note of the piece sits in bars 5 to 7 (the lift), the last bar settles or hangs for the loop
"""
import re

# id, name, tonic, minor?, chords (one per bar), bpm, gate (note length as fraction of its steps), bars
NEW = [
    ('lift_am', 'Rising lift', 'A', True, ['Am', 'F', 'C', 'G', 'Am', 'F', 'G', 'Am'], 138, 0.92, [
        "E5:6 A5:2 C6:4 B5:2 A5:2",
        "C6:6 A5:2 F5:4 A5:2 C6:2",
        "E6:6 D6:2 C6:4 D6:2 E6:2",
        "D6:6 B5:2 G5:4 .2 B5:2",
        "C6:6 E6:2 A6:6 G6:2",
        "A6:6 G6:2 F6:4 E6:2 C6:2",
        "G6:4 D6:4 B5:4 D6:2 G6:2",
        "E6:6 C6:2 A5:8"]),
    ('descent_bm', 'Epic descent', 'B', True, ['Bm', 'G', 'D', 'A', 'Bm', 'G', 'D', 'A'], 138, 0.92, [
        "F#5:4 B5:4 D6:6 C#6:2",
        "D6:6 B5:2 G5:4 B5:2 D6:2",
        "F#6:6 E6:2 D6:4 A5:2 D6:2",
        "E6:6 C#6:2 A5:4 .2 C#6:2",
        "D6:2 F#6:6 B6:6 A6:2",
        "G6:6 F#6:2 D6:4 B5:2 D6:2",
        "A6:4 F#6:4 D6:4 F#6:2 A6:2",
        "E6:4 C#6:4 A5:4 E6:4"]),
    ('resolve_em', 'Classical resolution', 'E', True, ['Em', 'C', 'Am', 'B', 'Em', 'C', 'Am', 'B'], 138, 0.92, [
        "B5:4 G5:2 B5:2 E6:6 D6:2",
        "G6:6 E6:2 C6:4 E6:2 G6:2",
        "A6:6 G6:2 E6:4 C6:2 E6:2",
        "F#6:6 D#6:2 B5:4 D#6:2 F#6:2",
        "G6:2 B6:6 G6:2 E6:2 B6:4",
        "E6:4 G6:4 E6:4 C6:2 E6:2",
        "E6:4 A6:4 E6:4 C6:2 A5:2",
        "D#6:4 F#6:4 B6:6 .2"]),
    ('stabs_csm', 'Syncopated stabs', 'C#', True, ['C#m', 'A', 'E', 'B', 'C#m', 'A', 'E', 'B'], 138, 0.6, [
        "G#5:3 C#6:3 E6:2 G#6:3 E6:3 C#6:2",
        "A5:3 C#6:3 E6:2 A6:3 E6:3 C#6:2",
        "G#5:3 B5:3 E6:2 G#6:3 E6:3 B5:2",
        "F#5:3 B5:3 D#6:2 F#6:3 D#6:3 B5:2",
        "E6:3 G#6:3 B6:2 G#6:3 E6:3 C#6:2",
        "C#6:3 E6:3 A6:2 E6:3 C#6:3 A5:2",
        "G#6:3 B6:3 G#6:2 E6:3 G#6:3 B6:2",
        "D#6:3 F#6:3 B6:2 F#6:3 D#6:3 B5:2"]),
    ('anthem_c', 'Major anthem', 'C', False, ['C', 'G', 'Am', 'F', 'C', 'G', 'F', 'G'], 138, 0.92, [
        "E5:4 G5:4 C6:6 B5:2",
        "D6:6 B5:2 G5:4 B5:2 D6:2",
        "E6:4 C6:4 A5:4 C6:2 E6:2",
        "A5:4 C6:4 F6:6 E6:2",
        "G6:6 E6:2 G6:4 A6:2 G6:2",
        "B6:6 A6:2 G6:4 D6:2 G6:2",
        "A6:6 G6:2 F6:4 C6:2 F6:2",
        "G6:4 D6:4 B5:4 D6:4"]),
    ('arp_lift_am', 'Arp climb', 'A', True, ['F', 'G', 'Am', 'Am', 'F', 'G', 'Am', 'E'], 138, 0.8, [
        "A5:2 C6:2 F6:4 C6:4 A5:2 C6:2",
        "B5:2 D6:2 G6:4 D6:4 B5:2 D6:2",
        "C6:2 E6:2 A6:4 E6:4 C6:2 E6:2",
        "A6:6 G6:2 E6:4 C6:2 E6:2",
        "C6:2 F6:2 A6:8 F6:2 C6:2",
        "D6:2 G6:2 B6:8 G6:2 D6:2",
        "E6:2 A6:2 C7:8 A6:2 E6:2",
        "G#5:2 B5:2 E6:4 G#6:4 B6:4"]),
    ('pulse_fsm', 'Pulse motif', 'F#', True, ['F#m', 'D', 'A', 'E', 'F#m', 'D', 'A', 'E'], 138, 0.8, [
        "F#5:2 C#6:2 F#6:2 C#6:2 F#6:2 G#6:2 A6:2 G#6:2",
        "D6:2 A5:2 D6:2 A5:2 F#6:2 E6:2 D6:2 E6:2",
        "C#6:2 E6:2 A6:2 E6:2 A6:2 B6:2 A6:2 E6:2",
        "B5:2 E6:2 G#6:2 E6:2 B6:2 A6:2 G#6:2 F#6:2",
        "C#6:2 F#6:2 A6:2 F#6:2 A6:4 G#6:2 F#6:2",
        "D6:2 F#6:2 A6:2 F#6:2 A6:4 B6:2 A6:2",
        "C#6:2 E6:2 A6:2 E6:2 C#7:4 B6:2 A6:2",
        "B5:2 E6:2 G#6:2 B6:2 G#6:4 E6:2 B5:2"]),
    ('pump_gm', 'Pedal pump', 'G', True, ['Gm', 'Eb', 'Bb', 'F', 'Gm', 'Eb', 'Bb', 'F'], 138, 0.55, [
        "G5 G6 G5 D6 G5 Bb5 G5 D6 G5 G6 G5 F6 G5 D6 G5 Bb5",
        "Eb5 Eb6 Eb5 Bb5 Eb5 G5 Eb5 Bb5 Eb5 Eb6 Eb5 D6 Eb5 Bb5 Eb5 G5",
        "Bb5 Bb6 Bb5 F6 Bb5 D6 Bb5 F6 Bb5 Bb6 Bb5 A6 Bb5 F6 Bb5 D6",
        "F5 F6 F5 C6 F5 A5 F5 C6 F5 F6 F5 E6 F5 C6 F5 A5",
        "G5 G6 G5 Bb6 G5 D7 G5 Bb6 G5 G6 G5 Bb6 G5 A6 G5 G6",
        "Eb5 Bb6 Eb5 G6 Eb5 Bb6 Eb5 G6 Eb5 Eb6 Eb5 G6 Eb5 Bb5 Eb5 G6",
        "Bb5 D6 Bb5 F6 Bb5 Bb6 Bb5 F6 Bb5 D6 Bb5 F6 Bb5 D6 Bb5 F6",
        "F5 A5 F5 C6 F5 A5 F5 C6 F5 A5 F5 C6 F5:4"]),
    ('step_dm', 'Stepwise hook', 'D', True, ['Dm', 'Bb', 'F', 'C', 'Dm', 'Bb', 'F', 'C'], 138, 0.85, [
        "A5:2 A5:2 D6:2 A5:2 F6:2 E6:2 D6:2 A5:2",
        "Bb5:2 Bb5:2 D6:2 F6:2 D6:2 C6:2 Bb5:2 D6:2",
        "C6:2 C6:2 F6:2 C6:2 A6:2 G6:2 F6:2 C6:2",
        "C6:2 E6:2 G6:2 E6:2 G6:2 F6:2 E6:2 D6:2",
        "D6:2 F6:2 A6:2 F6:2 A6:4 G6:2 F6:2",
        "Bb5:2 D6:2 F6:2 D6:2 Bb6:6 A6:2",
        "A6:2 F6:2 C6:2 F6:2 A6:4 G6:2 F6:2",
        "G6:2 E6:2 C6:2 E6:2 G6:6 .2"]),
    ('sunrise_d', 'Sunrise hook', 'D', False, ['D', 'A', 'Bm', 'G', 'D', 'A', 'G', 'A'], 138, 0.85, [
        "F#5:2 A5:2 D6:2 A5:2 F#6:4 E6:2 D6:2",
        "E5:2 A5:2 C#6:2 A5:2 E6:4 D6:2 C#6:2",
        "B5:2 D6:2 F#6:2 D6:2 F#6:4 E6:2 D6:2",
        "G5:2 B5:2 D6:2 B5:2 G6:4 F#6:2 E6:2",
        "A5:2 D6:2 F#6:2 D6:2 A6:6 G6:2",
        "C#6:2 E6:2 A6:2 E6:2 A6:4 B6:2 C#7:2",
        "B5:2 D6:2 G6:2 D6:2 B6:6 A6:2",
        "E6:2 C#6:2 A5:2 C#6:2 E6:4 .4"]),
    ('descent_cm', 'Falling vamp', 'C', True, ['Cm', 'Bb', 'Gm', 'Ab', 'Cm', 'Bb', 'Gm', 'Ab'], 138, 0.85, [
        "G5:2 C6:2 Eb6:2 C6:2 G6:4 F6:2 Eb6:2",
        "F5:2 Bb5:2 D6:2 Bb5:2 F6:4 Eb6:2 D6:2",
        "G5:2 Bb5:2 D6:2 Bb5:2 D6:4 C6:2 Bb5:2",
        "Ab5:2 C6:2 Eb6:2 C6:2 Eb6:4 D6:2 C6:2",
        "C6:2 Eb6:2 G6:2 Eb6:2 G6:4 Ab6:2 G6:2",
        "D6:2 F6:2 Bb6:2 F6:2 Bb6:4 C7:2 Bb6:2",
        "D6:2 G6:2 Bb6:2 G6:2 Bb6:4 A6:2 G6:2",
        "C6:2 Eb6:2 Ab6:2 Eb6:2 C6:4 .4"]),
]

PC = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def _midi(name):
    p = PC[name[0]]
    i = 1
    while i < len(name) and name[i] in '#b':
        p += 1 if name[i] == '#' else -1
        i += 1
    return (int(name[i:]) + 1) * 12 + p


def _chord(ch):
    r = ch[0]
    i = 1
    while i < len(ch) and ch[i] in '#b':
        i += 1
    root = _midi(r + ch[1:i] + '4') % 12
    minor = ch[i:].startswith('m')
    return root, {root, (root + (3 if minor else 4)) % 12, (root + 7) % 12}


def parse_bar(text):
    out, step = [], 0
    for t in text.split():
        if t.startswith('.'):
            n = int(t[1:] or 1)
            step += n
            continue
        nm, _, ln = t.partition(':')
        n = int(ln or 1)
        out.append((step, n, nm))
        step += n
    return out, step


def build(gid, name, tonic, minor, chords, bpm, gate, bars):
    """Returns (item dict without group, problems)."""
    problems, ev = [], []
    tpc = _midi(tonic + '4') % 12
    scale = {(tpc + i) % 12 for i in ((0, 2, 3, 5, 7, 8, 10, 11, 9) if minor else (0, 2, 4, 5, 7, 9, 11))}
    top, top_bar = -1, -1
    for b, text in enumerate(bars):
        notes, total = parse_bar(text)
        if total != 16:
            problems.append('%s bar %d has %d steps' % (gid, b + 1, total))
        root, tri = _chord(chords[b])
        for step, n, nm in notes:
            m = _midi(nm)
            if m % 12 not in scale | tri:
                problems.append('%s bar %d: %s is outside the key' % (gid, b + 1, nm))
            if step in (0, 8) and m % 12 not in tri:
                problems.append('%s bar %d: %s on beat %d is not a %s tone' % (gid, b + 1, nm, 1 if step == 0 else 3, chords[b]))
            if m > top:
                top, top_bar = m, b + 1
            vel = 108 if step in (0, 8) else (100 if n >= 6 else 90)
            if 4 <= b <= 6:
                vel = min(120, vel + 8)
            ev.append([b * 4 + step * 0.25, round(0.25 * n * gate, 3), m, vel])
    if not 5 <= top_bar <= 7:
        problems.append('%s: highest note is in bar %d, not in the lift (bars 5 to 7)' % (gid, top_bar))
    return dict(id='tr_' + gid, name=name + ', ' + '-'.join(chords), bpm=bpm, ev=ev, beats=len(bars) * 4), problems


if __name__ == '__main__':
    bad = 0
    for g in NEW:
        d, p = build(*g)
        print(d['id'], len(d['ev']), 'notes', 'OK' if not p else '')
        for x in p:
            print('  PROBLEM', x)
            bad += 1
    raise SystemExit(1 if bad else 0)
