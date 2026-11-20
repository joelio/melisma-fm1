#!/usr/bin/env python3
"""Merlin Engines logo takes for the FM-1's 240 x 240 boot screen: a Spitfire in plan under a pair of headphones.
   python3 gen.py  ->  take-*.svg, take-*.png (as the device shows it: 240 px, 16 colours, doubled), sheet.png"""
import subprocess
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

CREAM, AMBER, BLUE, RED, DIM = "#ece8dc", "#f0aa32", "#3d7fb5", "#c4483a", "#8a877e"
HERE = Path(__file__).parent


def spitfire(stroke=None, fill=None, sw=1.6, detail=CREAM, roundels=True, ring=None, dot=None):
    """Plan view, nose up, on a 200 x 200 grid centred on x = 100. The elliptical wing is the whole likeness."""
    if stroke and not fill:                      # an outline of the whole aircraft: stroke every part, then black them in
        outer = spitfire(fill=stroke, sw=0, roundels=False).replace('fill="' + stroke + '" stroke="none" stroke-width="0"',
                                                                    f'fill="{stroke}" stroke="{stroke}" stroke-width="{sw * 2}"')
        inner = spitfire(fill="#000", sw=0, detail=detail, roundels=roundels).replace('stroke="#000"', f'stroke="{detail}"') \
            .replace('fill="#000"/>', f'fill="{detail}"/>') if False else None
        style = 'fill="#000" stroke="none"'
        cut = detail
        p = [outer.split('<path d="M82,43')[0]]   # the stroked bodies only (no airscrew, canopy or cannon yet)
    else:
        style = f'fill="{fill or "none"}" stroke="{stroke or "none"}" stroke-width="{sw}" stroke-linejoin="round"'
        cut = "#000" if fill else detail
        p = []
    # wing: a flatter ellipse ahead, a deeper one behind
    p.append(f'<path d="M31,84 A69,13 0 0 1 169,84 A69,23 0 0 1 31,84 Z" {style}/>')
    # tailplane
    p.append(f'<path d="M77,157 A23,5 0 0 1 123,157 A23,9 0 0 1 77,157 Z" {style}/>')
    # fuselage: spinner, cowling, a long taper to the rudder
    p.append(f'<path d="M100,34 C103.4,34 105.6,39 106.2,47 L107.4,70 C107.6,96 105,132 102.6,160 L101.6,171 '
             f'L100,174 L98.4,171 L97.4,160 C95,132 92.4,96 92.6,70 L93.8,47 C94.4,39 96.6,34 100,34 Z" {style}/>')
    # the airscrew, the canopy, the cannon
    p.append(f'<path d="M82,43 L118,43" stroke="{stroke or fill}" stroke-width="{sw * 1.5}" stroke-linecap="round"/>')
    p.append(f'<ellipse cx="100" cy="89" rx="3.3" ry="8" fill="none" stroke="{cut}" stroke-width="{sw * 0.8}"/>')
    for x in (70, 130):
        p.append(f'<path d="M{x},73 V66" stroke="{stroke or fill}" stroke-width="{sw}" stroke-linecap="round"/>')
    if roundels:
        for x in (58, 142):
            p.append(f'<circle cx="{x}" cy="86" r="6.2" fill="none" stroke="{ring or cut}" stroke-width="{sw * (1.1 if ring else 0.8)}"/>')
            p.append(f'<circle cx="{x}" cy="86" r="2.4" fill="{dot or cut}"/>')
    return "\n".join(p)


def headphones(bands, cup=AMBER, pad=CREAM, cx=100, cy=104, r=86, w=10):
    """A chunky pair over the top, like a rainbow: the band as one or more arcs, a cup at each wingtip."""
    p, n = [], len(bands)
    for i, c in enumerate(bands):
        rr = r + (n - 1) * w / 2 - i * w
        p.append(f'<path d="M{cx - rr},{cy} A{rr},{rr} 0 0 1 {cx + rr},{cy}" fill="none" stroke="{c}" stroke-width="{w + 0.3}"/>')
    tw = n * w
    for s in (-1, 1):
        x = cx + s * r
        p.append(f'<rect x="{x - tw / 2 - 3}" y="{cy - 6}" width="{tw + 6}" height="40" rx="7" fill="{cup}"/>')
        p.append(f'<rect x="{x - s * (tw / 2 + 7) - 3}" y="{cy - 1}" width="6" height="30" rx="3" fill="{pad}"/>')
    return "\n".join(p)


def cans(band=13, cup_w=26, cup_h=50, cx=100, cy=100, r=84, cup=AMBER, pad=CREAM):
    """The robust pair: a broad band, a yoke at each end, a deep cup with its cushion on the inside."""
    p = [f'<path d="M{cx - r},{cy + 4} V{cy} A{r},{r} 0 0 1 {cx + r},{cy} V{cy + 4}" fill="none" stroke="{cup}" stroke-width="{band}" stroke-linejoin="round"/>']
    for s in (-1, 1):
        x = cx + s * r
        p.append(f'<rect x="{x - band / 2 - 2}" y="{cy - 2}" width="{band + 4}" height="12" rx="3" fill="{pad}"/>')                 # the yoke
        p.append(f'<rect x="{x - cup_w / 2 + s * 2}" y="{cy + 6}" width="{cup_w}" height="{cup_h}" rx="{cup_w * 0.42}" fill="{cup}"/>')   # the cup
        gx = x + s * (cup_w / 2 - 5) + s * 2
        p.append(f'<path d="M{gx},{cy + 16} V{cy + cup_h - 4}" stroke="#000" stroke-width="1.6" stroke-linecap="round" opacity="0.55"/>')  # a groove
        px = x - s * (cup_w / 2 + 3) + s * 2
        p.append(f'<rect x="{px - 4.5}" y="{cy + 11}" width="9" height="{cup_h - 10}" rx="4.5" fill="{pad}"/>')                     # the cushion
    return "\n".join(p)


def svg(body):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 240 240" width="240" height="240">\n'
            f'<rect width="240" height="240" fill="#000"/>\n{body}\n</svg>\n')


def text(x, y, s, size, font, colour=CREAM, spacing=0, weight="bold", anchor="middle", opacity=1):
    return (f'<text x="{x}" y="{y}" text-anchor="{anchor}" fill="{colour}" opacity="{opacity}" font-family="{font}" '
            f'font-weight="{weight}" font-size="{size}" letter-spacing="{spacing}">{s}</text>')


GILL, FUTURA, DIN = "Gill Sans, Helvetica Neue", "Futura, Helvetica Neue", "DIN Condensed, Helvetica Neue"

def cans2(bands, cup, pad=CREAM, yoke=CREAM, badge=None, band=14, cup_w=28, cup_h=52, cx=100, cy=100, r=84):
    """The robust pair in service colours: the band in one or more stripes (outside first), and, if asked, a
    roundel on each cup (badge: its rings from the outside in)."""
    p, n = [], len(bands)
    w = band / n
    for i, c in enumerate(bands):
        rr = r + band / 2 - w / 2 - i * w
        p.append(f'<path d="M{cx - rr},{cy + 4} V{cy} A{rr},{rr} 0 0 1 {cx + rr},{cy} V{cy + 4}" fill="none" stroke="{c}" stroke-width="{w + 0.4}"/>')
    for s in (-1, 1):
        x = cx + s * r
        p.append(f'<rect x="{x - band / 2 - 2}" y="{cy - 2}" width="{band + 4}" height="12" rx="3" fill="{yoke}"/>')
        ccx = x + s * 2
        p.append(f'<rect x="{ccx - cup_w / 2}" y="{cy + 6}" width="{cup_w}" height="{cup_h}" rx="{cup_w * 0.42}" fill="{cup}"/>')
        if badge:
            for k, c in enumerate(badge):
                p.append(f'<circle cx="{ccx + s * 1.5}" cy="{cy + 6 + cup_h / 2}" r="{(cup_w / 2 - 3.5) * (len(badge) - k) / len(badge)}" fill="{c}"/>')
        else:
            gx = ccx + s * (cup_w / 2 - 5)
            p.append(f'<path d="M{gx},{cy + 16} V{cy + cup_h - 4}" stroke="#000" stroke-width="1.6" stroke-linecap="round" opacity="0.55"/>')
        px = ccx - s * (cup_w / 2 + 3)
        p.append(f'<rect x="{px - 4.5}" y="{cy + 11}" width="9" height="{cup_h - 10}" rx="4.5" fill="{pad}"/>')
    return "\n".join(p)


# RAF colours, lifted a little for a black screen (approximations of the BS 381C paints)
R_BLUE, R_RED, R_WHITE, R_YELLOW = "#2f5fb3", "#cf3a2c", "#f1eee4", "#f2c230"       # bright roundel
D_BLUE, D_RED = "#3b4f86", "#a8382e"                                                   # the dull wartime pair
DK_GREEN, DK_EARTH, OCEAN_GREY, SEA_GREY, SKY, PRU = "#5f6b45", "#8f7450", "#7c868f", "#a3acb3", "#b9d3b4", "#5b88ad"


def raf(bands, cup, pad, yoke, badge, sub, ring=None, dot=None, sw=3.0):
    return (f'<g transform="translate(30,3) scale(0.9)">{cans2(bands, cup, pad, yoke, badge)}{spitfire(stroke=CREAM, sw=sw, ring=ring, dot=dot)}</g>'
            + text(120, 203, "MERLIN ENGINES", 27, DIN, spacing=4.5)
            + text(120, 224, "MELISMA  ·  MARK I", 12, DIN, colour=sub, spacing=4, weight="normal"))


TAKES = {
    "1-roundel": ("1: Roundel. Blue, white and red band; a roundel on each cup",
                  raf([R_BLUE, R_WHITE, R_RED], R_BLUE, R_WHITE, R_WHITE, [R_BLUE, R_WHITE, R_RED], R_RED, ring=R_BLUE, dot=R_RED)),
    "2-roundel-yellow": ("2: Wartime roundel. Yellow outer ring, duller blue and red",
                         raf([R_YELLOW, D_BLUE, R_WHITE, D_RED], D_BLUE, R_WHITE, R_YELLOW, [R_YELLOW, D_BLUE, R_WHITE, D_RED], R_YELLOW, ring=D_BLUE, dot=D_RED)),
    "3-blue-red": ("3: Plain blue cans, red yokes and cushions, roundels on the wings",
                   raf([R_BLUE], R_BLUE, R_RED, R_RED, None, "#7fa3e0", ring=R_BLUE, dot=R_RED)),
    "4-battle-of-britain": ("4: 1940 camouflage. Dark earth and dark green, sky cushions",
                            raf([DK_EARTH, DK_GREEN], DK_GREEN, SKY, DK_EARTH, [D_BLUE, R_WHITE, D_RED], SKY, ring=D_BLUE, dot=D_RED)),
    "5-day-fighter": ("5: 1941 day fighter. Ocean grey and dark green, yellow yokes, sky",
                      raf([OCEAN_GREY, DK_GREEN], OCEAN_GREY, SKY, R_YELLOW, [D_BLUE, R_WHITE, D_RED], SKY, ring=D_BLUE, dot=D_RED)),
    "6-pru-blue": ("6: PRU blue, the photo-reconnaissance Spitfires, with roundels",
                   raf([PRU], PRU, R_WHITE, R_WHITE, [D_BLUE, D_RED], "#9dbfd9", ring=D_BLUE, dot=D_RED)),
}


def main():
    tiles = []
    for name, (note, body) in TAKES.items():
        f = HERE / f"take-{name}.svg"
        f.write_text(svg(body))
        png = HERE / f"take-{name}.png"
        subprocess.run(["rsvg-convert", "-w", "240", "-h", "240", str(f), "-o", str(png)], check=True)
        im = Image.open(png).convert("RGB").quantize(16, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert("RGB")
        im = im.resize((480, 480), Image.NEAREST)   # as the FM-1's screen shows it, doubled
        im.save(png)
        tiles.append((name, note, im))
    sheet = Image.new("RGB", (3 * 520 + 40, 2 * 560 + 20), (24, 24, 26))
    d = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 17)
    except OSError:
        font = ImageFont.load_default()
    for i, (name, note, im) in enumerate(tiles):
        x, y = 40 + (i % 3) * 520, 30 + (i // 3) * 560
        sheet.paste(im, (x, y))
        d.text((x, y + 490), note, fill=(225, 222, 212), font=font)
    sheet.save(HERE / "sheet4.png")
    print("sheet4.png")


if __name__ == "__main__":
    main()
