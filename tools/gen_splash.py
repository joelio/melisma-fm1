#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Melisma's boot screens: assets/merlin-engines.svg and assets/splash.svg -> firmware/gen/mel_splash.h
(each 240 x 240, 16 colours, 4 bits a pixel).

  tools/gen_splash.py        needs rsvg-convert (librsvg) and Pillow

The header is checked in, so a build needs neither. Run this after changing the SVG.
"""
import subprocess
import sys
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SVGS = [ROOT / "assets/merlin-engines.svg", ROOT / "assets/splash.svg"]   # the maker's mark, then the instrument's plate
OUT, TMP = ROOT / "firmware/gen/mel_splash.h", ROOT / "build/splash.png"


def image(svg):
    subprocess.run(["rsvg-convert", "-w", "240", "-h", "240", str(svg), "-o", str(TMP)], check=True)
    im = Image.open(TMP).convert("RGB")
    if im.size != (240, 240):
        sys.exit(f"{svg.name} is {im.size}, not 240 x 240")
    q = im.quantize(16, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:48]
    px = q.tobytes()
    rows = []
    for y in range(240):
        r = px[y * 240:(y + 1) * 240]
        rows.append("{" + ", ".join(f"0x{r[x] << 4 | r[x + 1]:02X}" for x in range(0, 240, 2)) + "}")
    cols = ", ".join(f"RGB({pal[i * 3]}, {pal[i * 3 + 1]}, {pal[i * 3 + 2]})" for i in range(16))
    return cols, rows


def main():
    TMP.parent.mkdir(parents=True, exist_ok=True)
    imgs = [image(s) for s in SVGS]
    OUT.write_text(f"""/* SPDX-License-Identifier: GPL-3.0-only */
/* Melisma's boot screens: {", ".join(s.name for s in SVGS)}. Made by tools/gen_splash.py: change the SVGs, not this. */
#ifndef MEL_SPLASH_H
#define MEL_SPLASH_H
#define MEL_SPLASHES {len(imgs)}u
static const uint16_t MEL_SPLASH_PAL[MEL_SPLASHES][16] = {{
""" + "\n".join("    {" + c + "}," for c, _ in imgs) + """
};
static const uint8_t MEL_SPLASH[MEL_SPLASHES][240][120] = {
""" + "\n".join("  {\n" + "\n".join("    " + r + "," for r in rows) + "\n  }," for _, rows in imgs) + """
};
static void mel_splash_rows(uint32_t which, uint32_t y0, uint32_t h)   /* rows y0 .. y0 + h - 1 of a screen, into the canvas */
{
    uint32_t x, y;
    for (y = 0; y < h && y0 + y < 240u; y++)
        for (x = 0; x < 240u; x++) {
            uint32_t b = MEL_SPLASH[which][y0 + y][x >> 1];
            cv_pset((int32_t)x, (int32_t)y, MEL_SPLASH_PAL[which][x & 1u ? b & 15u : b >> 4]);
        }
}
#endif
""")
    print(f"{OUT.relative_to(ROOT)}: {len(imgs)} screens, {len(imgs) * 28800} B of image")


if __name__ == "__main__":
    main()
