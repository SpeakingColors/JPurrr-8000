#!/usr/bin/env python3
"""Compare ref (mockup) and mine (JUCE) renders: compare.py refdir minedir outdir [state ...]
Writes <outdir>/<state>_cmp.png (rows: ref | mine | diff x4) and prints the mean abs difference per display."""
import sys, os
import numpy as np
from PIL import Image, ImageChops, ImageDraw
ref, mine, out = sys.argv[1:4]
states = sys.argv[4:] or sorted(d for d in os.listdir(mine) if os.path.isdir(os.path.join(mine, d)))
os.makedirs(out, exist_ok=True)
for st in states:
    rows = []
    for f in sorted(os.listdir(os.path.join(mine, st))):
        if not f.endswith('.png') or f.startswith('_') or f.endswith('_chrome.png'): continue
        rp, mp = os.path.join(ref, st, f), os.path.join(mine, st, f)
        if not os.path.exists(rp): continue
        a = Image.open(rp).convert('RGB'); b = Image.open(mp).convert('RGB')
        if a.size != b.size:
            print(st, f, 'SIZE MISMATCH', a.size, b.size); b = b.resize(a.size)
        d = ImageChops.difference(a, b)
        m = float(np.asarray(d).mean())
        dd = d.point(lambda v: min(255, v * 4))
        print('%-10s %-12s mean|d|=%.2f' % (st, f[:-4], m))
        if m < float(os.environ.get('CMP_MIN', '0')): continue
        gap = 8; w = a.width * 3 + gap * 2 if a.width < 700 else a.width
        if a.width < 700:
            row = Image.new('RGB', (w, a.height + 14), (35, 43, 88)); row.paste(a, (0, 14)); row.paste(b, (a.width + gap, 14)); row.paste(dd, (2 * (a.width + gap), 14))
        else:
            row = Image.new('RGB', (a.width, a.height * 3 + 2 * gap + 14), (35, 43, 88)); row.paste(a, (0, 14)); row.paste(b, (0, 14 + a.height + gap)); row.paste(dd, (0, 14 + 2 * (a.height + gap)))
        ImageDraw.Draw(row).text((2, 1), '%s  mean|d|=%.2f   ref | mine | diff x4' % (f[:-4], m), fill=(255, 255, 255))
        rows.append(row)
        os.makedirs(os.path.join(out, st), exist_ok=True); row.save(os.path.join(out, st, f))
    if not rows: continue
    # pack rows into columns of max height
    W = max(r.width for r in rows)
    sheet = Image.new('RGB', (W, sum(r.height + 6 for r in rows)), (35, 43, 88)); y = 0
    for r in rows: sheet.paste(r, (0, y)); y += r.height + 6
    sheet.save(os.path.join(out, st + '_cmp.png'))
