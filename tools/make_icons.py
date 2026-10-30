#!/usr/bin/env python3
"""Regenerate every app icon of JPurrr-8000 from one drawing. Needs only Python 3 and Pillow.

    python3 tools/make_icons.py            write all files
    python3 tools/make_icons.py --check    only report whether the committed files are up to date

The drawing is the orange cat head from the mockup on a rounded dark indigo tile (colours below).
Small sizes use a simpler, bolder cat (no border, no whiskers at 32 px and below) so they stay readable.

Files written (all committed, CI does not run this script):
  assets/icon/jpurrr-8000.svg                 vector master
  assets/icon/jpurrr-8000-1024.png            master for Linux, Windows and the web (tile nearly edge to edge)
  assets/icon/jpurrr-8000-macos-1024.png      same artwork on Apple's icon grid (transparent margin, soft shadow)
  assets/icon/jpurrr-8000-256.png             window icon (embedded into the Linux standalone)
  installer/windows/jpurrr-8000.ico           16 24 32 48 64 128 256 (BMP up to 128, PNG at 256)
  installer/macos/jpurrr-8000.icns            16 to 1024 with @2x (PNG entries, written by hand, no iconutil needed)
  installer/linux/icons/hicolor/...           16 22 24 32 48 64 128 256 512 PNG plus scalable svg
  docs/favicon.png, docs/apple-touch-icon.png web page icons
"""
import io
import math
import os
import struct
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---- brand colours (docs/mockups/JPurrr-8000-mockup-v3.6.html, JP Blue look) ----
BG_TOP = (0x1B, 0x24, 0x68)
BG_BOTTOM = (0x0D, 0x12, 0x36)
RIM = (0x3B, 0x4A, 0x8E)
CAT = (0xF2, 0xA4, 0x3A)
CUT = (0x12, 0x18, 0x43)   # eyes, nose, whiskers

# ---- the cat, in the mockup's 40 x 40 units (docs/mockups ... const CAT) ----
# Head outline: M5 5 l10 8 c3-1 7-1 10 0 l10-8 1 19 c0 8-7 13-16 13 S4 32 4 24 z
HEAD = [
    ("M", (5, 5)), ("L", (15, 13)),
    ("C", (18, 12), (22, 12), (25, 13)),
    ("L", (35, 5)), ("L", (36, 24)),
    ("C", (36, 32), (29, 37), (20, 37)),
    ("C", (11, 37), (4, 32), (4, 24)),
]
CAT_CX, CAT_CY = 20.0, 21.0      # centre of the head including ears
CAT_W = 32.0                     # head width (x 4 to 36)
WHISKERS = [((3, 26), (12, 25)), ((3, 30), (12, 28)), ((37, 26), (28, 25)), ((37, 30), (28, 28))]


def head_points():
    pts = []
    cur = (0, 0)
    for seg in HEAD:
        if seg[0] in "ML":
            cur = seg[1]
            pts.append(cur)
        else:
            p0, p1, p2, p3 = cur, seg[1], seg[2], seg[3]
            for i in range(1, 49):
                t = i / 48.0
                u = 1 - t
                pts.append((u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0],
                            u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]))
            cur = p3
    return pts


def style(size, apple):
    """Drawing parameters for one output size."""
    if size >= 64:
        s = dict(cat=0.60, rim=True, whisk=True, eye=2.6, nose=True, wstroke=1.0)
    elif size >= 48:
        s = dict(cat=0.64, rim=False, whisk=True, eye=3.0, nose=True, wstroke=1.5)
    elif size >= 24:
        s = dict(cat=0.70, rim=False, whisk=False, eye=3.4, nose=size >= 32, wstroke=1.0)
    else:
        s = dict(cat=0.76, rim=False, whisk=False, eye=3.9, nose=False, wstroke=1.0)
    if apple:
        s["margin"] = 100 / 1024 if size >= 128 else 0.04
        s["shadow"] = size >= 256
    else:
        s["margin"] = 0.02 if size >= 64 else 0.0
        s["shadow"] = False
    return s


def render(size, apple=False):
    st = style(size, apple)
    ss = 4 if size <= 512 else 2          # supersampling factor
    n = size * ss
    m = st["margin"] * n
    tile = n - 2 * m
    rad = 0.2237 * tile if apple else 0.1875 * tile

    def tile_mask(inset=0.0):
        im = Image.new("L", (n, n), 0)
        ImageDraw.Draw(im).rounded_rectangle([m + inset, m + inset, n - m - inset - 1, n - m - inset - 1],
                                             radius=max(rad - inset, 1), fill=255)
        return im

    # background gradient inside the tile
    grad = Image.linear_gradient("L").resize((n, n), Image.BILINEAR)
    top = Image.new("RGB", (n, n), BG_TOP)
    bot = Image.new("RGB", (n, n), BG_BOTTOM)
    bg = Image.composite(bot, top, grad)
    out = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    out.paste(bg, (0, 0), tile_mask())

    if st["rim"]:                                   # thin lighter rim, inside the tile
        w = max(tile * 0.0156, 1.0)
        ring = ImageChops.subtract(tile_mask(), tile_mask(w))
        out.paste(Image.new("RGB", (n, n), RIM), (0, 0), ring)

    # cat
    k = st["cat"] * tile / CAT_W                    # pixels per cat unit
    ox = n / 2 - CAT_CX * k
    oy = n / 2 + 0.004 * n - CAT_CY * k

    def P(p):
        return (ox + p[0] * k, oy + p[1] * k)

    layer = Image.new("L", (n, n), 0)
    ImageDraw.Draw(layer).polygon([P(p) for p in head_points()], fill=255)
    out.paste(Image.new("RGB", (n, n), CAT), (0, 0), layer)

    cut = Image.new("L", (n, n), 0)
    d = ImageDraw.Draw(cut)
    r = st["eye"] * k
    for cx in (14.5, 25.5):
        x, y = P((cx, 23))
        d.ellipse([x - r, y - r, x + r, y + r], fill=255)
    if st["nose"]:
        d.polygon([P((18.3, 27)), P((21.7, 27)), P((20, 29.2))], fill=255)
    if st["whisk"]:
        wd = st["wstroke"] * k
        for a, b in WHISKERS:
            pa, pb = P(a), P(b)
            d.line([pa, pb], fill=255, width=max(int(wd), 1))
            for q in (pa, pb):
                d.ellipse([q[0] - wd / 2, q[1] - wd / 2, q[0] + wd / 2, q[1] + wd / 2], fill=255)
    out.paste(Image.new("RGB", (n, n), CUT), (0, 0), ImageChops.multiply(cut, layer))   # whiskers stop at the head edge

    if st["shadow"]:                                # soft drop shadow like Apple's own icons
        a = tile_mask().filter(ImageFilter.GaussianBlur(0.012 * n))
        a = a.point(lambda v: int(v * 0.38))
        sh = Image.new("RGBA", (n, n), (0, 0, 0, 0))
        sh.paste((0, 0, 0, 255), (0, int(0.011 * n)), a)
        out = Image.alpha_composite(sh, out)

    return out.resize((size, size), Image.LANCZOS)


# ---- vector master -------------------------------------------------------------------------------
def svg_text():
    margin = 0.02
    s = 256.0
    m = margin * s
    tile = s - 2 * m
    k = 0.60 * tile / CAT_W
    ox = s / 2 - CAT_CX * k
    oy = s / 2 + 0.004 * s - CAT_CY * k
    rim = 0.0156 * tile

    def f(v):
        return ("%.3f" % v).rstrip("0").rstrip(".")

    path = "M5 5l10 8c3-1 7-1 10 0l10-8 1 19c0 8-7 13-16 13S4 32 4 24z"
    wh = "".join("M%s %sL%s %s" % (a[0], a[1], b[0], b[1]) for a, b in WHISKERS)
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" width="256" height="256">
  <title>JPurrr-8000</title>
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#{BG_TOP[0]:02x}{BG_TOP[1]:02x}{BG_TOP[2]:02x}"/>
      <stop offset="1" stop-color="#{BG_BOTTOM[0]:02x}{BG_BOTTOM[1]:02x}{BG_BOTTOM[2]:02x}"/>
    </linearGradient>
    <clipPath id="head"><path d="{path}"/></clipPath>
  </defs>
  <rect x="{f(m)}" y="{f(m)}" width="{f(tile)}" height="{f(tile)}" rx="{f(0.1875 * tile)}" fill="url(#bg)"/>
  <rect x="{f(m + rim / 2)}" y="{f(m + rim / 2)}" width="{f(tile - rim)}" height="{f(tile - rim)}" rx="{f(0.1875 * tile - rim / 2)}" fill="none" stroke="#{RIM[0]:02x}{RIM[1]:02x}{RIM[2]:02x}" stroke-width="{f(rim)}"/>
  <g transform="translate({f(ox)} {f(oy)}) scale({f(k)})">
    <path fill="#{CAT[0]:02x}{CAT[1]:02x}{CAT[2]:02x}" d="{path}"/>
    <circle cx="14.5" cy="23" r="2.6" fill="#{CUT[0]:02x}{CUT[1]:02x}{CUT[2]:02x}"/>
    <circle cx="25.5" cy="23" r="2.6" fill="#{CUT[0]:02x}{CUT[1]:02x}{CUT[2]:02x}"/>
    <path d="M18.3 27h3.4L20 29.2z" fill="#{CUT[0]:02x}{CUT[1]:02x}{CUT[2]:02x}"/>
    <path d="{wh}" stroke="#{CUT[0]:02x}{CUT[1]:02x}{CUT[2]:02x}" stroke-width="1" fill="none" stroke-linecap="round" clip-path="url(#head)"/>
  </g>
</svg>
"""


# ---- container formats -----------------------------------------------------------------------------
def png_bytes(im):
    b = io.BytesIO()
    im.save(b, "PNG", optimize=True)
    return b.getvalue()


def ico_bytes(images):
    """Classic ICO: 32 bit BMP entries up to 128 px (works with every Windows resource compiler), PNG at 256."""
    blobs = []
    for im in images:
        w, h = im.size
        if w >= 256:
            blobs.append(png_bytes(im))
            continue
        px = im.convert("RGBA").load()
        xor = bytearray()
        for y in range(h - 1, -1, -1):
            for x in range(w):
                r, g, b, a = px[x, y]
                xor += bytes((b, g, r, a))
        stride = ((w + 31) // 32) * 4
        andm = bytearray()
        for y in range(h - 1, -1, -1):
            row = bytearray(stride)
            for x in range(w):
                if px[x, y][3] <= 5:
                    row[x // 8] |= 0x80 >> (x % 8)
            andm += row
        hdr = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, len(xor) + len(andm), 0, 0, 0, 0)
        blobs.append(hdr + bytes(xor) + bytes(andm))
    out = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    for im, blob in zip(images, blobs):
        w, h = im.size
        out += struct.pack("<BBBBHHII", w % 256, h % 256, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    return out + b"".join(blobs)


ICNS_TYPES = [  # (type, pixel size)  names follow what iconutil writes
    ("icp4", 16), ("icp5", 32), ("icp6", 64), ("ic07", 128), ("ic08", 256), ("ic09", 512), ("ic10", 1024),
    ("ic11", 32), ("ic12", 64), ("ic13", 256), ("ic14", 512),
]


def icns_bytes(images_by_size):
    body = b""
    for t, px in ICNS_TYPES:
        data = png_bytes(images_by_size[px])
        body += t.encode("ascii") + struct.pack(">I", 8 + len(data)) + data
    return b"icns" + struct.pack(">I", 8 + len(body)) + body


# ---- comparing (for --check) -------------------------------------------------------------------------
# PNG compression differs between Pillow / zlib versions, so files are compared by their pixels, not their bytes.
def decoded(rel, data):
    """List of Pillow images inside a generated file (None for text files)."""
    if rel.endswith(".png"):
        return [Image.open(io.BytesIO(data)).convert("RGBA")]
    if rel.endswith(".icns"):
        out, i = [], 8
        while i < len(data):
            n = struct.unpack(">I", data[i + 4:i + 8])[0]
            out.append(Image.open(io.BytesIO(data[i + 8:i + n])).convert("RGBA"))
            i += n
        return out
    if rel.endswith(".ico"):
        ico = Image.open(io.BytesIO(data))
        return [ico.ico.getimage(sz).convert("RGBA") for sz in sorted(ico.info["sizes"])]
    return None


def same(rel, old, new):
    if old is None:
        return False
    if old == new:
        return True
    a, b = decoded(rel, old), decoded(rel, new)
    if a is None or b is None or len(a) != len(b):
        return False
    for x, y in zip(a, b):
        if x.size != y.size or max(hi for _, hi in ImageChops.difference(x, y).getextrema()) > 3:
            return False
    return True


# ---- writing -------------------------------------------------------------------------------------
def main():
    check = "--check" in sys.argv
    files = {}

    def put(rel, data):
        files[rel] = data

    full = {s: render(s) for s in (16, 22, 24, 32, 48, 64, 128, 256, 512, 1024)}
    mac = {s: render(s, apple=True) for s in (16, 32, 64, 128, 256, 512, 1024)}
    svg = svg_text().encode("utf-8")

    put("assets/icon/jpurrr-8000.svg", svg)
    put("assets/icon/jpurrr-8000-1024.png", png_bytes(full[1024]))
    put("assets/icon/jpurrr-8000-macos-1024.png", png_bytes(mac[1024]))
    put("assets/icon/jpurrr-8000-256.png", png_bytes(full[256]))
    put("installer/windows/jpurrr-8000.ico", ico_bytes([full[s] if s in full else render(s) for s in (16, 24, 32, 48, 64, 128, 256)]))
    put("installer/macos/jpurrr-8000.icns", icns_bytes(mac))
    for s in (16, 22, 24, 32, 48, 64, 128, 256, 512):
        put("installer/linux/icons/hicolor/%dx%d/apps/jpurrr-8000.png" % (s, s), png_bytes(full[s]))
    put("installer/linux/icons/hicolor/scalable/apps/jpurrr-8000.svg", svg)
    put("docs/favicon.png", png_bytes(render(64)))
    put("docs/apple-touch-icon.png", png_bytes(render(180).convert("RGB")))

    stale = 0
    for rel, data in sorted(files.items()):
        path = os.path.join(ROOT, rel)
        old = open(path, "rb").read() if os.path.exists(path) else None
        if check:
            if not same(rel, old, data):
                print("out of date:", rel)
                stale += 1
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        if old != data:
            with open(path, "wb") as fh:
                fh.write(data)
            print("wrote", rel)
    if check:
        print("icons are up to date" if not stale else "%d file(s) out of date, run tools/make_icons.py" % stale)
        return 1 if stale else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
