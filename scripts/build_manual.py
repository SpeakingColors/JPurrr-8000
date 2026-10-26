#!/usr/bin/env python3
"""Render docs/manual/manual.md to docs/manual/JPurrr-8000-manual.pdf.

  python3 scripts/build_manual.py [--out FILE.pdf] [--keep-html]

Needs: pip install markdown playwright pypdf ; playwright install chromium
Images: ![caption](img/<name>.png) in manual.md is looked up in
  1. docs/manual/img/<name>.png          (real plugin screenshots, put them here)
  2. docs/manual/img/mockup/<name>.png   (shots of the design mockup)
  3. docs/screenshot.png                 (the README mockup picture)
The first one that exists is used. Mockup images get the note "design mockup" under them.
The cover shows the version from CMakeLists.txt. The table of contents gets real page numbers
(the PDF is rendered twice).
"""
import argparse
import asyncio
import html
import re
import sys
from pathlib import Path

import markdown
from pypdf import PdfReader
from playwright.async_api import async_playwright

ROOT = Path(__file__).resolve().parent.parent
MAN = ROOT / "docs" / "manual"
FONTS = ROOT / "plugin" / "Resources" / "fonts"

CAT = ('<svg viewBox="0 0 40 40" aria-hidden="true"><path fill="#f2a43a" d="M5 5l10 8c3-1 7-1 10 0l10-8 1 19c0 8-7 13-16 13S4 32 4 24z"/>'
       '<circle cx="14.5" cy="23" r="2.6" fill="#121843"/><circle cx="25.5" cy="23" r="2.6" fill="#121843"/>'
       '<path d="M18.3 27h3.4L20 29.2z" fill="#121843"/>'
       '<path d="M3 26l9-1M3 30l9-2M37 26l-9-1M37 30l-9-2" stroke="#121843" stroke-width="1" fill="none" stroke-linecap="round"/></svg>')


def project_version():
    text = (ROOT / "CMakeLists.txt").read_text()
    m = re.search(r"project\([^)]*VERSION\s+([0-9][0-9.]*)", text)
    if not m:
        sys.exit("Error: no project(... VERSION x.y.z) in CMakeLists.txt")
    return m.group(1)


def find_image(src):
    name = Path(src).name
    for cand, mock in ((MAN / src, False), (MAN / "img" / "mockup" / name, True), (ROOT / "docs" / "screenshot.png", True)):
        if cand.is_file():
            return cand, mock
    sys.exit(f"Error: no image found for {src}")


def render_body(md_text):
    md_text = re.sub(r"<!--.*?-->", "", md_text, flags=re.S)
    md = markdown.Markdown(extensions=["tables", "sane_lists", "toc"], extension_configs={"toc": {"permalink": False}})
    body = md.convert(md_text)

    # figures
    def fig(m):
        alt, src = html.unescape(m.group(1)), m.group(2)
        path, mock = find_image(src)
        note = '<span class="mock"> Image: design mockup.</span>' if mock else ""
        return f'<figure><img src="{path.as_uri()}" alt="{html.escape(alt)}"><figcaption>{html.escape(alt)}.{note}</figcaption></figure>'
    body = re.sub(r'<p><img alt="([^"]*)" src="([^"]*)" ?/?></p>', fig, body)

    # numbered headings (h1 = chapter, h2 = section) and the list for the TOC
    heads, c1, c2 = [], 0, 0

    def number(m):
        nonlocal c1, c2
        lvl, hid, title = int(m.group(1)), m.group(2), m.group(3)
        if lvl == 1:
            c1 += 1
            c2 = 0
            num = str(c1)
        else:
            c2 += 1
            num = f"{c1}.{c2}"
        plain = re.sub(r"<[^>]+>", "", title)
        heads.append((lvl, num, html.unescape(plain), hid))
        return f'<h{lvl} id="{hid}"><span class="num">{num}</span> {title}</h{lvl}>'
    body = re.sub(r'<h([12]) id="([^"]+)">(.*?)</h\1>', number, body)
    return body, heads


CSS = """
@font-face{font-family:'Barlow';font-weight:400;src:url('%(f)s/Barlow-400.ttf')}
@font-face{font-family:'Barlow';font-weight:500;src:url('%(f)s/Barlow-500.ttf')}
@font-face{font-family:'Barlow';font-weight:600;src:url('%(f)s/Barlow-600.ttf')}
@font-face{font-family:'Barlow Condensed';font-weight:500;src:url('%(f)s/BarlowCondensed-500.ttf')}
@font-face{font-family:'Barlow Condensed';font-weight:600;src:url('%(f)s/BarlowCondensed-600.ttf')}
@font-face{font-family:'Barlow Condensed';font-weight:700;src:url('%(f)s/BarlowCondensed-700.ttf')}
@font-face{font-family:'JetBrains Mono';font-weight:400;src:url('%(f)s/JetBrainsMono-400.ttf')}
@font-face{font-family:'JetBrains Mono';font-weight:500;src:url('%(f)s/JetBrainsMono-500.ttf')}
:root{--ink:#161a26;--mut:#59627a;--navy:#121843;--amber:#f2a43a;--amberd:#b4540a;--line:#d5d9e4;--soft:#f3f5fa}
@page{size:A4;margin:22mm 18mm 22mm 18mm;
  @bottom-center{content:counter(page);font:500 9pt 'Barlow',sans-serif;color:#59627a}
  @bottom-left{content:'JPurrr-8000 manual';font:500 9pt 'Barlow',sans-serif;color:#59627a}
  @bottom-right{content:'%(ver)s';font:500 9pt 'Barlow',sans-serif;color:#59627a}}
@page cover{margin:0;@bottom-center{content:none}@bottom-left{content:none}@bottom-right{content:none}}
*{box-sizing:border-box}
html{font:400 10.5pt/1.5 'Barlow',sans-serif;color:var(--ink)}
body{margin:0;background:#fff}
.cover{page:cover;height:297mm;width:210mm;position:relative;page-break-after:always;overflow:hidden;background:#fff}
.cover .band{background:var(--navy);height:150mm;padding:34mm 24mm 0;color:#fff;border-bottom:3mm solid var(--amber)}
.cover svg{width:46mm;height:46mm}
.cover .small{font:600 12pt 'Barlow Condensed',sans-serif;letter-spacing:.34em;color:#9fb0dc;margin-top:10mm}
.cover h1{font:italic 700 58pt/1 'Barlow Condensed',sans-serif;letter-spacing:.01em;margin:2mm 0 2mm;color:#f6f8fb;border:0;padding:0}
.cover .sub{font:600 16pt 'Barlow Condensed',sans-serif;letter-spacing:.3em;color:var(--amber)}
.cover .below{padding:16mm 24mm 0}
.cover .below .t{font:600 26pt 'Barlow Condensed',sans-serif;color:var(--navy)}
.cover .below p{margin:3mm 0;color:var(--mut);font-size:11.5pt;max-width:120mm}
.cover .foot{position:absolute;left:24mm;right:24mm;bottom:16mm;font-size:9pt;color:var(--mut);border-top:1px solid var(--line);padding-top:4mm}
.toc{page-break-after:always}
.toc h1{font:700 26pt 'Barlow Condensed',sans-serif;color:var(--navy);margin:0 0 8mm;border:0}
.toc ol{list-style:none;margin:0;padding:0}
.toc li{display:flex;align-items:baseline;gap:2mm;padding:.15mm 0;margin:0}
.toc li .lbl{white-space:nowrap}
.toc li .dots{flex:1;border-bottom:1px dotted #9aa3b8;transform:translateY(-1mm)}
.toc li.l1{margin-top:2.4mm;font-weight:600;font-size:11pt}
.toc li.l2{padding-left:7mm;font-size:9.2pt;color:#2b3350}
.toc .pg{font-variant-numeric:tabular-nums;color:var(--mut)}
h1,h2,h3{font-family:'Barlow Condensed',sans-serif;color:var(--navy);break-after:avoid}
h1{font-weight:700;font-size:25pt;margin:0 0 5mm;padding-bottom:2mm;border-bottom:.8mm solid var(--amber);margin-top:11mm}
main>h1:first-child{margin-top:0}
h2{font-weight:600;font-size:15pt;margin:7mm 0 2mm}
h1 .num,h2 .num{color:var(--amberd);margin-right:2mm}
p{margin:0 0 3mm;orphans:3;widows:3}
p:has(+ul),p:has(+ol),p:has(+table){break-after:avoid}
ul,ol{margin:0 0 3mm;padding-left:6mm}
li{margin-bottom:1mm}
strong{font-weight:600}
code{font:500 9pt 'JetBrains Mono',monospace;font-variant-ligatures:none;background:var(--soft);border:1px solid var(--line);border-radius:3px;padding:0 1.2mm;color:#1d2440;word-break:break-word}
table{border-collapse:collapse;width:100%%;margin:2mm 0 5mm;font-size:9.5pt;break-inside:auto}
th{background:var(--navy);color:#fff;text-align:left;font-weight:600;padding:1.6mm 2.4mm}
td{padding:1.6mm 2.4mm;border-bottom:1px solid var(--line);vertical-align:top}
tr{break-inside:avoid}
tr:nth-child(even) td{background:var(--soft)}
td:first-child{white-space:nowrap;font-weight:600;color:var(--navy);width:30%%}
figure{margin:4mm 0 6mm;break-inside:avoid;text-align:center}
figure img{max-width:100%%;border:1px solid var(--line);border-radius:2mm}
figcaption{font-size:8.5pt;color:var(--mut);margin-top:1.5mm}
figcaption .mock{font-style:italic}
"""


def build_html(body, heads, pages, ver):
    toc = []
    for lvl, num, title, hid in heads:
        pg = pages.get(num, "")
        toc.append(f'<li class="l{lvl}"><span class="lbl">{num}&nbsp;&nbsp;{html.escape(title)}</span><span class="dots"></span><span class="pg">{pg}</span></li>')
    css = CSS % {"f": FONTS.as_uri(), "ver": "Version " + ver}
    return f"""<!doctype html><html lang="en"><head><meta charset="utf-8"><title>JPurrr-8000 manual</title><style>{css}</style></head><body>
<section class="cover"><div class="band">{CAT}<div class="small">STUDIO 87</div><h1>JPurrr-8000</h1><div class="sub">SYNTHESIZER</div></div>
<div class="below"><div class="t">User manual</div><p>Version {ver}</p>
<p>A free and open source synth in the spirit of the late 90s JP series, with its own mix ready FX chain. VST3 and standalone for Linux, Windows and macOS.</p></div>
<div class="foot">Free software, AGPL-3.0-or-later. Not affiliated with or endorsed by Roland or Arturia. All names belong to their owners.</div></section>
<section class="toc"><h1>Contents</h1><ol>{''.join(toc)}</ol></section>
<main>{body}</main></body></html>"""


async def to_pdf(html_path, pdf_path):
    async with async_playwright() as p:
        b = await p.chromium.launch()
        pg = await b.new_page()
        await pg.goto(html_path.as_uri())
        await pg.evaluate("document.fonts.ready")
        await pg.wait_for_timeout(300)
        await pg.pdf(path=str(pdf_path), format="A4", prefer_css_page_size=True, print_background=True)
        await b.close()


def find_pages(pdf_path, heads, toc_pages):
    reader = PdfReader(str(pdf_path))
    texts = [re.sub(r"\s+", "", (pg.extract_text() or "")) for pg in reader.pages]
    pages, start = {}, toc_pages
    for lvl, num, title, hid in heads:
        key = re.sub(r"\s+", "", num + title)
        for i in range(start, len(texts)):
            if key in texts[i]:
                pages[num] = i + 1
                start = i
                break
        else:
            print(f"Warning: heading '{num} {title}' not found in the PDF", file=sys.stderr)
    return pages, len(reader.pages)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(MAN / "JPurrr-8000-manual.pdf"))
    ap.add_argument("--keep-html", action="store_true")
    a = ap.parse_args()
    ver = project_version()
    body, heads = render_body((MAN / "manual.md").read_text(encoding="utf-8"))
    work = MAN / "build"
    work.mkdir(exist_ok=True)
    html_path = work / "manual.html"
    out = Path(a.out)

    html_path.write_text(build_html(body, heads, {n: "00" for _, n, _, _ in heads}, ver), encoding="utf-8")
    asyncio.run(to_pdf(html_path, out))
    # the TOC is one or two pages: find where the first chapter starts
    reader = PdfReader(str(out))
    toc_pages = 2
    first = re.sub(r"\s+", "", heads[0][1] + heads[0][2])
    last = re.sub(r"\s+", "", heads[-1][1] + heads[-1][2])
    for i in range(1, len(reader.pages)):
        t = re.sub(r"\s+", "", reader.pages[i].extract_text() or "")
        if first in t and last not in t:   # the TOC page lists every heading, a chapter page does not
            toc_pages = i
            break
    pages, total = find_pages(out, heads, toc_pages)
    html_path.write_text(build_html(body, heads, pages, ver), encoding="utf-8")
    asyncio.run(to_pdf(html_path, out))
    pages2, total2 = find_pages(out, heads, toc_pages)
    if pages2 != pages:
        sys.exit("Error: page numbers moved between the two passes; check the TOC length")
    if not a.keep_html:
        html_path.unlink()
        try:
            work.rmdir()
        except OSError:
            pass
    print(f"{out} ({total2} pages, version {ver})")


if __name__ == "__main__":
    main()
