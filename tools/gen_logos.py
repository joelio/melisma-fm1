#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The logos, as SVG with the lettering drawn as outlines (so they look the same wherever they are shown):
   tools/gen_logos.py  ->  assets/logo/*.svg
The emblem is drawn by assets/logo-takes/gen.py (a Spitfire in plan under a pair of headphones); the lettering is
DIN Condensed Bold, read from this computer's fonts and written out as shapes. Needs fonttools (pip install fonttools)."""
import sys
from pathlib import Path
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "assets/logo-takes"))
import gen  # noqa: E402  (the emblem's shapes)

FONT = Path("/System/Library/Fonts/Supplemental/DIN Condensed Bold.ttf")
CREAM, SKY, SKY_DEEP, INK = "#ece8dc", "#a9cdb4", "#5d7f6a", "#000"
OUT = ROOT / "assets/logo"

font = TTFont(str(FONT))
glyphs, cmap, upm = font.getGlyphSet(), font.getBestCmap(), font["head"].unitsPerEm


def lettering(x, y, s, size, colour, spacing=0.0, anchor="middle", opacity=1.0):
    """s as outlines: baseline at y, size in px, extra space between letters in px."""
    k = size / upm
    widths = [glyphs[cmap[ord(c)]].width * k for c in s]
    total = sum(widths) + spacing * (len(s) - 1)
    at = x - total / 2 if anchor == "middle" else x - total if anchor == "end" else x
    parts = []
    for c, w in zip(s, widths):
        pen = SVGPathPen(glyphs, ntos=lambda v: f"{v:.2f}".rstrip("0").rstrip("."))
        glyphs[cmap[ord(c)]].draw(TransformPen(pen, (k, 0, 0, -k, at, y)))
        d = pen.getCommands()
        if d:
            parts.append(d)
        at += w + spacing
    op = f' opacity="{opacity}"' if opacity != 1.0 else ""
    return f'<path fill="{colour}"{op} d="{" ".join(parts)}"/>'


def emblem(x, y, scale):
    """The Spitfire under its headphones, in a 200 x 200 box at (x, y), scaled."""
    return (f'<g transform="translate({x},{y}) scale({scale})">'
            f'{gen.cans2([SKY], SKY, SKY_DEEP, SKY_DEEP, None)}{gen.spitfire(stroke=CREAM, sw=3.0)}</g>')


def melisma_mark(x, y, scale):
    """The Melisma mark: one note, and three more under a slur."""
    return (f'<g transform="translate({x},{y}) scale({scale})" stroke="{SKY}" fill="{SKY}">'
            '<path d="M13.6 45 V22" stroke-width="2.4" stroke-linecap="round" fill="none"/>'
            '<path d="M19 26 Q38 16 58 24" stroke-width="2.4" stroke-linecap="round" fill="none"/>'
            '<ellipse cx="9.5" cy="45" rx="4.6" ry="3.4" transform="rotate(-20 9.5 45)"/>'
            '<ellipse cx="31" cy="37" rx="4.2" ry="3.1" transform="rotate(-20 31 37)"/>'
            '<ellipse cx="45" cy="34" rx="4.2" ry="3.1" transform="rotate(-20 45 34)"/>'
            '<ellipse cx="58" cy="33" rx="4.2" ry="3.1" transform="rotate(-20 58 33)"/></g>')


def svg(w, h, body, title):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}" role="img" aria-label="{title}">\n'
            f'<title>{title}</title>\n<rect width="{w}" height="{h}" rx="{min(w, h) * 0.09:.0f}" fill="{INK}"/>\n{body}\n</svg>\n')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    files = {
        # the maker's badge: emblem and name
        "merlin-engines.svg": svg(240, 240, emblem(30, 6, 0.9)
                                  + lettering(120, 216, "MERLIN ENGINES", 30, CREAM, spacing=4.4), "Merlin Engines"),
        # the emblem alone
        "merlin-engines-mark.svg": svg(240, 240, emblem(14, 22, 1.06), "Merlin Engines"),
        # the instrument's badge
        "melisma.svg": svg(240, 240, melisma_mark(52, 8, 2.05)
                           + lettering(120, 168, "MELISMA", 46, CREAM, spacing=9)
                           + f'<path d="M48,184 H192" stroke="{CREAM}" stroke-width="0.8" opacity="0.4"/>'
                           + lettering(120, 210, "MERLIN ENGINES", 16, SKY, spacing=4.2), "Melisma, a Merlin Engines instrument"),
        # the banner at the head of the README
        "melisma-banner.svg": svg(960, 280, emblem(40, 30, 1.1)
                                  + lettering(300, 150, "MELISMA", 124, CREAM, spacing=16, anchor="start")
                                  + f'<path d="M302,172 H900" stroke="{CREAM}" stroke-width="1.2" opacity="0.4"/>'
                                  + lettering(302, 208, "A FOUR-PIECE BAND YOU SIT IN WITH", 28, SKY, spacing=4.6, anchor="start")
                                  + lettering(302, 244, "MERLIN ENGINES  ·  FOR THE M-VAVE FM-1", 20, CREAM, spacing=4.4, anchor="start", opacity=0.7),
                                  "Melisma: a four-piece band you sit in with. Merlin Engines, for the M-VAVE FM-1"),
    }
    for name, text in files.items():
        (OUT / name).write_text(text)
        print(f"assets/logo/{name}  {len(text)} B")


if __name__ == "__main__":
    main()
