#!/usr/bin/env python3
# Drives the Standalone build under Xvfb with XTest (mouse and keys) and takes screenshots; for manual GUI checks.
# Usage inside xvfb-run: xdrive.py <exe> <outdir> <script.py>  where script.py defines run(d); d has click(x,y), rclick, drag, move,
# dclick, key(name), text(s), shot(name), wait(s) with x,y in DESIGN px of the editor (2920 wide design space).
import ctypes, ctypes.util, subprocess, sys, os, time
from PIL import Image

class Driver:
    def __init__(self, exe, out, env=None):
        self.x11 = ctypes.CDLL('libX11.so.6'); self.xt = ctypes.CDLL('libXtst.so.6')
        self.x11.XOpenDisplay.restype = ctypes.c_void_p
        self.d = self.x11.XOpenDisplay(None)
        self.out = out; os.makedirs(out, exist_ok=True)
        self.proc = subprocess.Popen([exe], env=env, stdout=open(os.path.join(out, 'app.log'), 'w'), stderr=subprocess.STDOUT)
        time.sleep(6)
        self.ox = self.oy = 0; self.scale = 0.72
        self.focus()
        self.locate()
    def focus(self):
        # no window manager under Xvfb: keys go to the window under the pointer
        D = ctypes.c_void_p(self.d)
        self.x11.XSetInputFocus(D, ctypes.c_ulong(1), 1, ctypes.c_ulong(0)); self.x11.XFlush(D)
    def snap(self, path):
        subprocess.run(['import', '-window', 'root', path], check=True)
    def locate(self):
        p = os.path.join(self.out, '_loc.png'); self.snap(p); im = Image.open(p).convert('RGB'); W, H = im.size
        # left wood cheek: brownish pixels
        def brown(c): return 70 < c[0] < 130 and 40 < c[1] < 80 and 15 < c[2] < 50 and c[0] > c[1] > c[2]
        for y in range(H // 2, H // 2 + 1):
            for x in range(W):
                if brown(im.getpixel((x, y))): self.ox = x; break
        x = self.ox; y = H // 2
        while y > 0 and brown(im.getpixel((x + 3, y - 1))): y -= 1
        self.oy = y
        print('editor origin', self.ox, self.oy)
    def _p(self, x, y): return int(self.ox + x * self.scale), int(self.oy + y * self.scale)
    def move(self, x, y):
        px, py = self._p(x, y); self.xt.XTestFakeMotionEvent(ctypes.c_void_p(self.d), 0, px, py, 0); self.x11.XFlush(ctypes.c_void_p(self.d)); time.sleep(0.35)
    def _btn(self, b, down):
        self.xt.XTestFakeButtonEvent(ctypes.c_void_p(self.d), b, down, 0); self.x11.XFlush(ctypes.c_void_p(self.d)); time.sleep(0.15)
    def click(self, x, y, b=1): self.move(x, y); self._btn(b, 1); self._btn(b, 0); time.sleep(0.15)
    def rclick(self, x, y): self.click(x, y, 3)
    def dclick(self, x, y): self.click(x, y); self.click(x, y)
    def drag(self, x1, y1, x2, y2, steps=12):
        self.move(x1, y1); self._btn(1, 1)
        for i in range(1, steps + 1): self.move(x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps)
        self._btn(1, 0); time.sleep(0.15)
    def wheel(self, x, y, up=True, n=1):
        self.move(x, y)
        for _ in range(n): self._btn(4 if up else 5, 1); self._btn(4 if up else 5, 0)
    def key(self, name, shift=False):
        self.x11.XStringToKeysym.restype = ctypes.c_ulong
        ks = self.x11.XStringToKeysym(name.encode()); kc = self.x11.XKeysymToKeycode(ctypes.c_void_p(self.d), ctypes.c_ulong(ks))
        if shift:
            sk = self.x11.XKeysymToKeycode(ctypes.c_void_p(self.d), ctypes.c_ulong(self.x11.XStringToKeysym(b'Shift_L')))
            self.xt.XTestFakeKeyEvent(ctypes.c_void_p(self.d), sk, 1, 0)
        self.xt.XTestFakeKeyEvent(ctypes.c_void_p(self.d), kc, 1, 0); self.xt.XTestFakeKeyEvent(ctypes.c_void_p(self.d), kc, 0, 0)
        if shift: self.xt.XTestFakeKeyEvent(ctypes.c_void_p(self.d), sk, 0, 0)
        self.x11.XFlush(ctypes.c_void_p(self.d)); time.sleep(0.12)
    def text(self, s):
        names = {' ': 'space', '-': 'minus', '.': 'period', ',': 'comma'}
        for ch in s: self.key(names.get(ch, ch), shift=ch.isupper())
    def wait(self, s): time.sleep(s)
    def shot(self, name, h=931):
        p = os.path.join(self.out, name + '.png'); self.snap(p)
        prev = open(p, 'rb').read()                      # wait until the picture is stable (the Debug build repaints slowly)
        for _ in range(30):
            time.sleep(0.5); self.snap(p); cur = open(p, 'rb').read()
            if cur == prev: break
            prev = cur
        im = Image.open(p).convert('RGB'); w = int(2920 * self.scale); hh = int(h * self.scale)
        im.crop((self.ox, self.oy, min(im.width, self.ox + w), min(im.height, self.oy + hh))).save(p)
    def close(self): self.proc.terminate()

if __name__ == '__main__':
    exe, out, script = sys.argv[1:4]
    env = dict(os.environ, HOME=os.environ.get('XD_HOME', '/tmp/jhome'))
    d = Driver(exe, out, env)
    try:
        ns = {}; exec(open(script).read(), ns); ns['run'](d)
    finally:
        d.close()
