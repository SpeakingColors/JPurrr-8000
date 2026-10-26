#!/usr/bin/env python3
# crop.py <state> x y w h [scale] : mockup on top, plugin below, written to /home/claude/snap-diff/crop.png
import sys
from PIL import Image
s, x, y, w, h = sys.argv[1], *map(int, sys.argv[2:6]); k = int(sys.argv[6]) if len(sys.argv) > 6 else 2
a = Image.open('/home/claude/snap-mock/%s.png' % s).convert('RGB').crop((x, y, x + w, y + h))
b = Image.open('/home/claude/snap-plugin/%s.png' % s).convert('RGB').crop((x, y, x + w, y + h))
o = Image.new('RGB', (w * k, h * k * 2 + 4), (255, 0, 255))
o.paste(a.resize((w * k, h * k), Image.NEAREST), (0, 0)); o.paste(b.resize((w * k, h * k), Image.NEAREST), (0, h * k + 4))
o.save('/home/claude/snap-diff/crop.png')
