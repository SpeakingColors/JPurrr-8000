#!/usr/bin/env python3
# Compare mockup and plugin snapshots. Usage: compare.py <mockdir> <plugdir> <outdir> [state...]
# Writes <state>_diff.png (red = differing pixels over the dimmed mockup) and prints the share of differing pixels.
import sys, os
from PIL import Image, ImageChops
mock, plug, out = sys.argv[1:4]
os.makedirs(out, exist_ok=True)
states = sys.argv[4:] or sorted(f[:-4] for f in os.listdir(plug) if f.endswith('.png'))
for s in states:
    a = Image.open(os.path.join(mock, s + '.png')).convert('RGB')
    b = Image.open(os.path.join(plug, s + '.png')).convert('RGB')
    if a.size != b.size:
        print(s, 'size differs', a.size, b.size); w = min(a.width, b.width); h = min(a.height, b.height)
        a = a.crop((0, 0, w, h)); b = b.crop((0, 0, w, h))
    d = ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v > 40 else 0)
    n = sum(1 for v in d.getdata() if v)
    base = a.point(lambda v: v // 3)
    base.paste(Image.new('RGB', a.size, (255, 40, 40)), mask=d)
    base.save(os.path.join(out, s + '_diff.png'))
    print('%-10s differing pixels %.2f%%' % (s, 100.0 * n / (a.width * a.height)))
