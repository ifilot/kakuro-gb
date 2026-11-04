#!/usr/bin/env python3
"""Build the cartridge label as SVG, 600 dpi PNG and print PDFs.

Requires Pillow and rsvg-convert (librsvg). Geometry uses tenths of a millimetre;
the puzzle illustration uses the solution and clues of assets/puzzles/001.puz.
"""
from pathlib import Path
import argparse
import shutil
import subprocess
from xml.sax.saxutils import escape
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
NAVY, CREAM, TERRA, SAND = '#1e2440', '#f8f0dc', '#d8643c', '#e6d4a8'


def text(x, y, value, size, fill=NAVY, **attrs):
    attributes = ' '.join(f'{k.replace("_", "-")}="{v}"' for k, v in attrs.items())
    return (f'<text x="{x}" y="{y}" font-family="DejaVu Sans" '
            f'font-size="{size}" fill="{fill}" {attributes}>{escape(value)}</text>')


def artwork():
    parts = [f'<rect x="-20" y="-20" width="460" height="410" fill="{CREAM}"/>',
             f'<rect x="-20" y="-20" width="82" height="410" fill="{NAVY}"/>',
             f'<rect x="62" y="-20" width="4" height="410" fill="{TERRA}"/>',
             text(0, 0, 'GAME BOY', 30, CREAM, font_weight='bold',
                  letter_spacing='3', transform='translate(42 294) rotate(-90)'),
             text(89, 31, 'HOMEBREW', 11, TERRA, letter_spacing='2.1', font_weight='bold'),
             text(391, 31, 'FRONT', 10, NAVY, text_anchor='end', letter_spacing='1.2'),
             text(85, 97, 'KAKURO', 62, NAVY, font_weight='bold', letter_spacing='-2'),
             text(90, 123, 'NUMBER CROSSWORD', 17, NAVY, letter_spacing='1.45'),
             f'<path d="M90 139H393" stroke="{TERRA}" stroke-width="3"/>']
    # Raked sand curves and stones echo the game's opening screen.
    parts.append('<g clip-path="url(#garden)">')
    for y in range(159, 301, 9):
        parts.append(f'<path d="M78 {y} Q123 {y-13} 175 {y} T245 {y}" '
                     f'fill="none" stroke="{SAND}" stroke-width="2"/>')
    for cx, cy, rx, ry in [(131, 202, 18, 11), (179, 257, 14, 10)]:
        for ring in range(4, 0, -1):
            parts.append(f'<ellipse cx="{cx}" cy="{cy}" rx="{rx+ring*7}" '
                         f'ry="{ry+ring*5}" fill="{CREAM}" stroke="{SAND}" stroke-width="2"/>')
        parts.append(f'<ellipse cx="{cx}" cy="{cy}" rx="{rx}" ry="{ry}" fill="{NAVY}"/>')
        parts.append(f'<path d="M{cx-rx+6} {cy-3}q8 -7 17 -1" '
                     f'fill="none" stroke="#596078" stroke-width="3"/>')
    parts.append('</g>')
    rows = [line.split() for line in (ROOT / 'assets/puzzles/001.puz').read_text().splitlines()
            if line.strip() and not line.startswith('#')]
    grid = [[int(v.lstrip('#')) for v in row] for row in rows]
    n, cell, x0, y0 = len(grid), 25, 240, 153
    for r, row in enumerate(grid):
        for c, digit in enumerate(row):
            x, y = x0 + c * cell, y0 + r * cell
            parts.append(f'<rect x="{x}" y="{y}" width="{cell}" height="{cell}" '
                         f'fill="{CREAM if digit else NAVY}" stroke="{NAVY}" stroke-width="1.2"/>')
            if digit:
                # Partially filled board keeps the illustration recognizable.
                if (r + c) % 3 != 0:
                    parts.append(text(x+12.5, y+18, str(digit), 16, NAVY, text_anchor='middle'))
                continue
            sums = []
            for dr, dc in [(0, 1), (1, 0)]:
                rr, cc, digits = r+dr, c+dc, []
                while rr < n and cc < n and grid[rr][cc]:
                    digits.append(grid[rr][cc])
                    rr, cc = rr+dr, cc+dc
                sums.append(sum(digits) if len(digits) > 1 else 0)
            if any(sums):
                parts.append(f'<path d="M{x} {y}L{x+25} {y+25}" stroke="{SAND}" stroke-width=".8"/>')
                if sums[0]:
                    parts.append(text(x+22, y+9, str(sums[0]), 8, CREAM, text_anchor='end'))
                if sums[1]:
                    parts.append(text(x+3, y+22, str(sums[1]), 8, CREAM))
    parts += [f'<rect x="89" y="308" width="302" height="25" rx="3" fill="{TERRA}"/>',
              text(240, 325, '80 PUZZLES  /  LOGIC PUZZLE', 13, CREAM,
                   text_anchor='middle', font_weight='bold'),
              text(90, 351, 'GB + GBC', 11, NAVY, font_weight='bold'),
              text(391, 351, 'COLOR ENHANCED', 10, NAVY, text_anchor='end')]
    return ''.join(parts)


def svg(body, width, height, viewbox, clip=True):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}mm" height="{height}mm" '
            f'viewBox="{viewbox}"><title>Kakuro cartridge front sticker</title>'
            '<defs><clipPath id="trim"><rect width="420" height="370" rx="15"/></clipPath>'
            '<clipPath id="garden"><rect x="83" y="150" width="154" height="150"/></clipPath></defs>'
            f'<g{chr(32)+"clip-path=\"url(#trim)\"" if clip else ""}>{body}</g></svg>')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out-dir', type=Path, default=ROOT / 'assets/cartridge')
    args = parser.parse_args()
    renderer = shutil.which('rsvg-convert')
    if not renderer:
        parser.error('rsvg-convert is required (install librsvg2-bin)')
    out = args.out_dir
    out.mkdir(parents=True, exist_ok=True)
    art = artwork()
    label = out / 'kakuro-sticker.svg'
    label.write_text(svg(art, 42, 37, '0 0 420 370'))
    bleed = out / 'kakuro-sticker-bleed.svg'
    bleed.write_text(svg(art, 46, 41, '-20 -20 460 410', clip=False))
    # An A4 page with 2 mm bleed and crop marks at the 42 x 37 mm trim.
    sheet = '<g transform="translate(840 500)">' + art + '</g>'
    for x in (840, 1260):
        for y in (500, 870):
            dx, dy = (-1 if x == 840 else 1), (-1 if y == 500 else 1)
            sheet += (f'<path d="M{x+dx*25} {y}h{dx*25}M{x} {y+dy*25}v{dy*25}" '
                      'fill="none" stroke="#000" stroke-width="1"/>')
    sheet += text(1050, 1030, 'KAKURO / 42 × 37 mm / 2 mm bleed', 24, text_anchor='middle')
    sheet += text(1050, 1070, 'Print at 100% / Actual size. Cut to marks; round corners to 1.5 mm.',
                  20, text_anchor='middle')
    sheet += '<path d="M800 1160h500m-500 -8v16m500 -16v16" stroke="#000" stroke-width="2"/>'
    sheet += text(1050, 1200, '50 mm scale check', 20, text_anchor='middle')
    print_svg = out / 'kakuro-sticker-print-a4.svg'
    print_svg.write_text(svg(sheet, 210, 297, '0 0 2100 2970', clip=False))
    for source, name, fmt in [(label, 'kakuro-sticker.png', 'png'),
                              (bleed, 'kakuro-sticker-bleed.pdf', 'pdf'),
                              (print_svg, 'kakuro-sticker-print-a4.pdf', 'pdf')]:
        subprocess.run([renderer, '--format', fmt, '--dpi-x', '600', '--dpi-y', '600',
                        '--output', str(out / name), str(source)], check=True)
        if fmt == 'png':
            with Image.open(out / name) as image:
                image.save(out / name, dpi=(600, 600))
    print(f'Cartridge label written to {out}')


if __name__ == '__main__':
    main()
