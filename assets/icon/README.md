# JPurrr-8000 icon

The orange cat head from the mockup on a rounded dark indigo tile.

- `jpurrr-8000.svg` is the vector master. `jpurrr-8000-1024.png` is the same artwork at 1024 px, `jpurrr-8000-macos-1024.png` the same on Apple's icon grid (transparent margin, soft shadow), `jpurrr-8000-256.png` the window icon built into the Linux standalone.
- Do not edit the PNG files by hand. `python3 tools/make_icons.py` draws everything (Python 3 and Pillow only) and also writes the files the installers use: `installer/windows/jpurrr-8000.ico`, `installer/macos/jpurrr-8000.icns`, `installer/linux/icons/hicolor/*`, `docs/favicon.png` and `docs/apple-touch-icon.png`. `python3 tools/make_icons.py --check` only reports whether they are up to date.
- Small sizes (48 px and below) use a bolder cat: no border, and no whiskers at 32 px and below.
