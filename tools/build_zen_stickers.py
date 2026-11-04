#!/usr/bin/env python3
"""Composite five illustrated stickers with a solver-checked Kakuro board.

Imagegen supplies the garden backgrounds; this script owns every puzzle cell,
digit and clue in the final SVG/PNG/PDF files. Requires Pillow and rsvg-convert.
"""
import base64
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from PIL import Image, ImageDraw, ImageFont
from gen_puzzles import Puzzle

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'assets/cartridge/zen-v2'
NAMES = ['06-autumn-garden', '07-pavilion-garden', '08-maple-and-stone',
         '09-evening-garden', '10-open-zen-garden']
NAVY, CREAM = '#18213c', '#f5e7c9'
X, Y, CELL = 160, 166, 26.5


def prepare_board():
    puzzle = Puzzle(ROOT / 'assets/puzzles/001.puz')
    error = puzzle.check()
    if error:
        raise ValueError(error)
    puzzle.make_unique()
    shown = puzzle.given | {i for i, d in enumerate(puzzle.solution)
                            if d and sum(divmod(i, puzzle.n)) % 3 != 0}
    solutions = puzzle.solve(shown, limit=2)
    if solutions != [puzzle.solution]:
        raise ValueError('The illustrated puzzle must have exactly one solution')
    n, clues = puzzle.n, []
    for i, digit in enumerate(puzzle.solution):
        if digit:
            continue
        r, c = divmod(i, n)
        clue = {'row': r, 'col': c}
        for label, dr, dc in [('across', 0, 1), ('down', 1, 0)]:
            rr, cc, run = r+dr, c+dc, []
            while rr < n and cc < n and puzzle.solution[rr*n+cc]:
                run.append(puzzle.solution[rr*n+cc])
                rr, cc = rr+dr, cc+dc
            if len(run) >= 2:
                if len(run) != len(set(run)):
                    raise ValueError('A digit repeats within a run')
                clue[label] = sum(run)
        if len(clue) > 2:
            clues.append(clue)
    proof = {
        'source': 'assets/puzzles/001.puz', 'size': n,
        'row_col_indexing': 'zero based',
        'clue_convention': 'upper-right = across; lower-left = down',
        'solution': [puzzle.solution[r*n:(r+1)*n] for r in range(n)],
        'displayed': [[puzzle.solution[r*n+c] if r*n+c in shown else None
                       for c in range(n)] for r in range(n)],
        'clues': clues, 'solutions_found_with_limit_2': len(solutions),
        'all_runs_have_distinct_digits': True,
        'all_clue_sums_match_solution': True,
    }
    return puzzle, shown, proof


def board_svg(puzzle, shown, proof):
    n = puzzle.n
    side = n * CELL
    scale = CELL / 28
    sums = {(c['row'], c['col']): c for c in proof['clues']}
    parts = [f'<g id="verified-kakuro" font-family="DejaVu Serif" filter="url(#paper-grain)">',
             f'<rect x="{X-4}" y="{Y-4}" width="{side+8}" height="{side+8}" '
             f'fill="{CREAM}" stroke="{NAVY}" stroke-width="1.5"/>']
    for r in range(n):
        for c in range(n):
            i, x, y = r*n+c, X+c*CELL, Y+r*CELL
            digit = puzzle.solution[i]
            parts.append(f'<rect x="{x}" y="{y}" width="{CELL}" height="{CELL}" '
                         f'fill="{CREAM if digit else NAVY}" stroke="{NAVY}" stroke-width="1"/>')
            if digit:
                if i in shown:
                    parts.append(f'<text x="{x+14*scale}" y="{y+21*scale}" fill="{NAVY}" '
                                 f'font-size="{21*scale}" font-weight="bold" text-anchor="middle">{digit}</text>')
                continue
            clue = sums.get((r, c), {})
            if not clue:
                continue
            parts.append(f'<path d="M{x} {y}l{CELL} {CELL}" stroke="{CREAM}" stroke-width=".8"/>')
            if 'across' in clue:
                parts.append(f'<text x="{x+25.5*scale}" y="{y+10*scale}" fill="{CREAM}" '
                             f'font-size="{9.2*scale}" text-anchor="end">{clue["across"]}</text>')
            if 'down' in clue:
                parts.append(f'<text x="{x+2.5*scale}" y="{y+25*scale}" fill="{CREAM}" '
                             f'font-size="{9.2*scale}">{clue["down"]}</text>')
    return ''.join(parts) + '</g>'


def document(body):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="42mm" height="37mm" '
            'viewBox="0 0 420 370"><title>Kakuro illustrated cartridge sticker</title>'
            '<defs><clipPath id="trim"><rect width="420" height="370" rx="15"/>'
            '</clipPath><filter id="paper-grain" x="0" y="0" width="1" height="1">'
            '<feTurbulence type="fractalNoise" baseFrequency=".65" numOctaves="3" '
            'seed="17" result="grain"/><feColorMatrix in="grain" type="saturate" values="0"/>'
            '<feComponentTransfer><feFuncA type="linear" slope=".24"/></feComponentTransfer>'
            '<feBlend in="SourceGraphic" mode="multiply"/></filter></defs>'
            '<g clip-path="url(#trim)">' + body + '</g></svg>')


def render(renderer, source, dest, fmt):
    subprocess.run([renderer, '-f', fmt, '-d', '600', '-p', '600',
                    '-o', str(dest), str(source)], check=True)
    if fmt == 'png':
        with Image.open(dest) as image:
            image.save(dest, dpi=(600, 600))


def comparison_sheet():
    sheet = Image.new('RGB', (1380, 840), '#eee3d0')
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype('DejaVuSans.ttf', 19)
    for i, name in enumerate(NAMES):
        x, y = 20 + (i % 3)*460, 12 + (i // 3)*420
        draw.text((x, y), name.replace('-', ' ').title(), font=font, fill=NAVY)
        with Image.open(OUT / f'{name}.png') as image:
            thumb = image.resize((420, 370), Image.Resampling.LANCZOS)
            sheet.paste(thumb, (x, y+30), thumb)
    sheet.save(OUT / 'comparison.png')


def main():
    renderer = shutil.which('rsvg-convert')
    if not renderer:
        raise SystemExit('rsvg-convert is required')
    OUT.mkdir(parents=True, exist_ok=True)
    puzzle, shown, proof = prepare_board()
    board = board_svg(puzzle, shown, proof)
    reference = OUT / 'verified-board.svg'
    reference.write_text(document(board))
    render(renderer, reference, OUT / 'verified-board.png', 'png')
    hashes = {}
    for name in NAMES:
        background = OUT / 'backgrounds' / f'{name}.png'
        data = base64.b64encode(background.read_bytes()).decode()
        image = f'<image width="420" height="370" preserveAspectRatio="none" href="data:image/png;base64,{data}"/>'
        source, output = OUT / f'{name}.svg', OUT / f'{name}.png'
        source.write_text(document(image + board))
        render(renderer, source, output, 'png')
        render(renderer, source, OUT / f'{name}.pdf', 'pdf')
        with Image.open(output) as final, Image.open(OUT / 'verified-board.png') as expected:
            # Compare the entire opaque board interior against the proof render.
            # This catches any background imagery changing clues or digits.
            w, h = final.size
            box = (int(X/420*w)+2, int(Y/370*h)+2,
                   int((X+CELL*puzzle.n)/420*w)-2, int((Y+CELL*puzzle.n)/370*h)-2)
            crop = final.crop(box).convert('RGB').tobytes()
            if crop != expected.crop(box).convert('RGB').tobytes():
                raise ValueError(f'{name}: final puzzle pixels differ from verified board')
            hashes[name] = hashlib.sha256(crop).hexdigest()
        print(f'Built and checked {name}')
    proof['final_board_pixel_sha256'] = hashes
    (OUT / 'puzzle-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    comparison_sheet()


if __name__ == '__main__':
    main()
