#!/usr/bin/env python3
"""Generate src/gfx.h and src/gfx.c: every tile of the game, drawn here (the
start screen's scene in scene.py), and the tables of the wave that leads
from a solved board to the overview.

Colours are the Game Boy's four shades: 0 white, 1 light grey, 2 dark grey,
3 black (BGP = 0xE4). The board is made of 16x16 cells, four tiles each:

- A white cell has a black grid line along its top and left edge; the
  right and bottom edges are the next cell's lines (or the black around the
  board). Entered digits are black on white, given digits black on light
  grey.
- A black cell is black. When it holds sums it gets a black diagonal from
  the top-left corner: the triangle above it holds the sum across (dark grey
  with a white 3x5 number right-aligned in the top-right tile), the one
  below it the sum down (left-aligned in the bottom-left tile, where that
  triangle is widest). A triangle without a sum stays black.

The tiles are loaded from index 0 with BG tile data at 0x8800 (signed
addressing is handled in video.c); sprite tiles live at 0x8000-0x87FF, so
they never share space with the background.

  python3 tools/gen_gfx.py [--preview]   (preview: build/gfx_preview.png)
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

import scene

ROOT = Path(__file__).resolve().parent.parent
OUT_H = ROOT / "src/gfx.h"
OUT_C = ROOT / "src/gfx.c"

W, L, D, B = 0, 1, 2, 3

# 5x7 font for ASCII 32..95. Unused punctuation is blank; a few codes carry
# the game's own symbols: [ right arrow, \ down arrow, ] check mark,
# ^ star, @ the same star in light grey (a difficulty's missing star), _ bullet.
FONT = {
    " ": [],
    "!": ["..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."],
    "'": ["..#..", "..#..", ".....", ".....", ".....", ".....", "....."],
    "(": ["...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."],
    ")": [".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."],
    "*": [".....", "..#..", "#.#.#", ".###.", "#.#.#", "..#..", "....."],
    "+": [".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."],
    ",": [".....", ".....", ".....", ".....", ".....", "..#..", ".#..."],
    "-": [".....", ".....", ".....", ".###.", ".....", ".....", "....."],
    ".": [".....", ".....", ".....", ".....", ".....", ".....", "..#.."],
    "/": ["....#", "...#.", "...#.", "..#..", ".#...", ".#...", "#...."],
    "0": [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
    "1": ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
    "2": [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
    "3": ["#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."],
    "4": ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
    "5": ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
    "6": ["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."],
    "7": ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
    "8": [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    "9": [".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."],
    ":": [".....", "..#..", "..#..", ".....", "..#..", "..#..", "....."],
    "<": ["...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."],
    "=": [".....", ".....", "#####", ".....", "#####", ".....", "....."],
    ">": [".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."],
    "?": [".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    "C": [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
    "D": ["###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###.."],
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    "G": [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"],
    "H": ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "I": [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
    "J": ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
    "K": ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
    "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    "M": ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
    "N": ["#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"],
    "O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    "Q": [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "V": ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
    "W": ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."],
    "X": ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    "Y": ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    "Z": ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
    "[": [".....", "..#..", "...#.", "#####", "...#.", "..#..", "....."],
    "\\": ["..#..", "..#..", "..#..", "#.#.#", ".###.", "..#..", "....."],
    "]": [".....", "....#", "...##", "#.##.", "###..", ".#...", "....."],
    "^": ["..#..", "..#..", "#####", ".###.", ".#.#.", "#...#", "....."],
    "@": ["..#..", "..#..", "#####", ".###.", ".#.#.", "#...#", "....."],
    "_": [".....", ".....", ".###.", ".###.", ".###.", ".....", "....."],
}

# 3x5 digits for the sums and the keypad.
SMALL = {
    0: ["###", "#.#", "#.#", "#.#", "###"],
    1: [".#.", "##.", ".#.", ".#.", "###"],
    2: ["###", "..#", "###", "#..", "###"],
    3: ["###", "..#", "###", "..#", "###"],
    4: ["#.#", "#.#", "###", "..#", "..#"],
    5: ["###", "#..", "###", "..#", "###"],
    6: ["###", "#..", "###", "#.#", "###"],
    7: ["###", "..#", "..#", ".#.", ".#."],
    8: ["###", "#.#", "###", "#.#", "###"],
    9: ["###", "#.#", "###", "..#", "###"],
}

# The status line's font: SMALL and these, 5 dots high, for ASCII 32..95
# like FONT ([ right arrow, \ down arrow). The line is made of 4-dot cells;
# a glyph wider than 3 dots takes two.
TINY = {
    " ": ["...", "...", "...", "...", "..."],
    "!": ["#", "#", "#", ".", "#"],
    "-": ["...", "...", "###", "...", "..."],
    ".": [".", ".", ".", ".", "#"],
    "/": ["..#", "..#", ".#.", "#..", "#.."],
    ":": [".", "#", ".", "#", "."],
    "A": [".#.", "#.#", "###", "#.#", "#.#"],
    "B": ["##.", "#.#", "##.", "#.#", "##."],
    "C": [".##", "#..", "#..", "#..", ".##"],
    "D": ["##.", "#.#", "#.#", "#.#", "##."],
    "E": ["###", "#..", "##.", "#..", "###"],
    "F": ["###", "#..", "##.", "#..", "#.."],
    "G": [".##", "#..", "#.#", "#.#", ".##"],
    "H": ["#.#", "#.#", "###", "#.#", "#.#"],
    "I": ["###", ".#.", ".#.", ".#.", "###"],
    "J": ["..#", "..#", "..#", "#.#", ".#."],
    "K": ["#.#", "#.#", "##.", "#.#", "#.#"],
    "L": ["#..", "#..", "#..", "#..", "###"],
    "M": ["#.#", "###", "###", "#.#", "#.#"],
    "N": ["##.", "#.#", "#.#", "#.#", "#.#"],
    "O": [".#.", "#.#", "#.#", "#.#", ".#."],
    "P": ["##.", "#.#", "##.", "#..", "#.."],
    "Q": [".#.", "#.#", "#.#", "##.", ".##"],
    "R": ["##.", "#.#", "##.", "#.#", "#.#"],
    "S": [".##", "#..", ".#.", "..#", "##."],
    "T": ["###", ".#.", ".#.", ".#.", ".#."],
    "U": ["#.#", "#.#", "#.#", "#.#", "###"],
    "V": ["#.#", "#.#", "#.#", "#.#", ".#."],
    "W": ["#.#", "#.#", "###", "###", "#.#"],
    "X": ["#.#", "#.#", ".#.", "#.#", "#.#"],
    "Y": ["#.#", "#.#", ".#.", ".#.", ".#."],
    "Z": ["###", "..#", ".#.", "#..", "###"],
    "[": ["..#..", "...#.", "#####", "...#.", "..#.."],
    "]": ["....#", "...#.", "#.#..", ".#...", "....."],
    "^": ["..#..", "..#..", "#####", ".###.", ".#.#."],
    "@": ["..#..", "..#..", "#####", ".###.", ".#.#."],
    "\\": ["..#..", "..#..", "#.#.#", ".###.", "..#.."],
}
TINY.update({str(d): rows for d, rows in SMALL.items()})

# 7x11 digits with 2-dot strokes for the cells.
BIG = {
    1: ["...##..", "..###..", ".####..", "...##..", "...##..", "...##..", "...##..", "...##..",
        "...##..", "...##..", ".######"],
    2: [".#####.", "##...##", ".....##", ".....##", "....##.", "...##..", "..##...", ".##....",
        "##.....", "##.....", "#######"],
    3: [".#####.", "##...##", ".....##", ".....##", "..####.", ".....##", ".....##", ".....##",
        ".....##", "##...##", ".#####."],
    4: ["....##.", "...###.", "..####.", ".##.##.", "##..##.", "##..##.", "#######", "....##.",
        "....##.", "....##.", "....##."],
    5: ["#######", "##.....", "##.....", "##.....", "######.", ".....##", ".....##", ".....##",
        ".....##", "##...##", ".#####."],
    6: ["..####.", ".##....", "##.....", "##.....", "######.", "##...##", "##...##", "##...##",
        "##...##", "##...##", ".#####."],
    7: ["#######", ".....##", ".....##", "....##.", "....##.", "...##..", "...##..", "..##...",
        "..##...", "..##...", "..##..."],
    8: [".#####.", "##...##", "##...##", "##...##", ".#####.", "##...##", "##...##", "##...##",
        "##...##", "##...##", ".#####."],
    9: [".#####.", "##...##", "##...##", "##...##", "##...##", ".######", ".....##", ".....##",
        ".....##", "....##.", ".####.."],
}

FONT_FIRST = 32
FONT_COUNT = 64
SUM_MIN, SUM_MAX = 3, 42               # 42 = 9+8+...+3, a run of 7 (8x8 grids)
SUM_COUNT = SUM_MAX - SUM_MIN + 1


def blank(color=W, w=8, h=8):
    return [[color] * w for _ in range(h)]


def stamp(img, glyph, x0, y0, color):
    for y, row in enumerate(glyph):
        for x, ch in enumerate(row):
            if ch == "#":
                img[y0 + y][x0 + x] = color


def number(img, value, x_right, y0, color, x_left=None):
    """A 3x5 number, right-aligned at column x_right (inclusive), or
    left-aligned at column x_left."""
    digits = str(value)
    x = x_left if x_left is not None else x_right - (4 * len(digits) - 1) + 1
    for ch in digits:
        stamp(img, SMALL[int(ch)], x, y0, color)
        x += 4


def tiles_of(img):
    """Splits an image (list of rows) into 8x8 tiles, row by row."""
    h, w = len(img), len(img[0])
    return [[row[tx * 8:tx * 8 + 8] for row in img[ty * 8:ty * 8 + 8]]
            for ty in range(h // 8) for tx in range(w // 8)]


def encode(tile):
    out = bytearray()
    for row in tile:
        lo = hi = 0
        for x, c in enumerate(row):
            lo |= (c & 1) << (7 - x)
            hi |= (c >> 1 & 1) << (7 - x)
        out += bytes((lo, hi))
    return bytes(out)


# --- background tiles -----------------------------------------------------

def font_tiles(color=B, only_digits=False):
    out = []
    for code in range(FONT_FIRST, FONT_FIRST + FONT_COUNT):
        ch = chr(code)
        if only_digits and not "1" <= ch <= "9":
            continue
        t = blank()
        stamp(t, FONT.get(ch, []), 1, 0, L if ch == "@" else color)
        out.append(t)
    return out


def clue_cell(across, down):
    """The 16x16 picture of a black cell; across/down: the sum or 0."""
    img = blank(B, 16, 16)
    for y in range(1, 16):
        for x in range(1, 16):
            if x > y and across:
                img[y][x] = D
            elif x < y and down:
                img[y][x] = D
    if across:
        number(img, across, 14, 2, W)
    if down:
        number(img, down, 7, 10, W, x_left=1)
    return img


def white_cell(digit=0, given=False, ink=B):
    """ink: the digit's shade; on the Color it is dark grey, so the digit
    can have a colour of its own, apart from the grid line."""
    img = blank(L if given else W, 16, 16)
    for k in range(16):
        img[0][k] = img[k][0] = B
    if digit:
        stamp(img, BIG[digit], 5, 3, ink)
    return img


def board_tiles():
    """BLACK, TL[a*2+d], BR[a*2+d], ACROSS[sum], DOWN[sum], WHITE[4],
    DIGIT[9][4], GIVEN[9][4] (quarters in the order TL TR BL BR)."""
    tiles = {"black": blank(B)}
    tiles["tl"], tiles["br"] = [], []
    for a in (0, 1):
        for d in (0, 1):
            q = tiles_of(clue_cell(10 if a else 0, 10 if d else 0))
            tiles["tl"].append(q[0])
            tiles["br"].append(q[3])
    tiles["across"] = [tiles_of(clue_cell(s, 0))[1] for s in range(SUM_MIN, SUM_MAX + 1)]
    tiles["down"] = [tiles_of(clue_cell(0, s))[2] for s in range(SUM_MIN, SUM_MAX + 1)]
    tiles["white"] = tiles_of(white_cell())
    tiles["digit"] = [t for d in range(1, 10) for t in tiles_of(white_cell(d))]
    tiles["given"] = [t for d in range(1, 10) for t in tiles_of(white_cell(d, True))]
    # The Color's: DIGIT and GIVEN with the digit in dark grey (a palette of its own).
    tiles["cgb_cells"] = [t for given in (False, True) for d in range(1, 10)
                          for t in tiles_of(white_cell(d, given, D))]
    # The numbers must not reach the diagonal tiles: check that the TL/BR
    # tiles of a cell with two-digit sums are the plain ones.
    q = tiles_of(clue_cell(SUM_MAX, SUM_MAX))
    assert q[0] == tiles["tl"][3] and q[3] == tiles["br"][3], "sum digits overlap the diagonal"
    return tiles


def tiny_font():
    """Per character of ASCII 32..95: its width, then its 5 rows with the
    leftmost dot in bit 6 (one dot from the cell's left edge; a narrower
    glyph is centred in the 3 dots); a character without a glyph is blank."""
    out = bytearray()
    for code in range(FONT_FIRST, FONT_FIRST + FONT_COUNT):
        rows = TINY.get(chr(code))
        if not rows:
            out += bytes(6)
            continue
        assert len(rows[0]) <= 7, chr(code)
        out.append(len(rows[0]))
        x0 = max(0, 3 - len(rows[0])) // 2
        for row in rows:
            out.append(sum(0x40 >> (x0 + x) for x, ch in enumerate(row) if ch == "#"))
    return bytes(out)


# --- the start screen's scene ----------------------------------------------------

def scene_tiles(base):
    """The scene as tiles (from base on), its map, and per tile the Color
    palette (scene.PALETTES) that has all its materials. The border runs
    down its sides (scene.EDGE: the border's material, the outside's), with
    a dot of the outside inside it."""
    img = scene.draw()
    line, outside = scene.EDGE
    border = border_image()
    for y in range(scene.H):
        for x in range(scene.W):
            if border[scene.TOP + y][x] == W:
                img[y][x] = line
            elif x < 2 or x > 157 or x in (3, 156):     # a dark gap inside the line, for a light scene
                img[y][x] = outside
    tiles, tmap, attrs = [], [], []
    for ty in range(scene.H // 8):
        for tx in range(scene.W // 8):
            mats = [[img[y][x] for x in range(tx * 8, tx * 8 + 8)] for y in range(ty * 8, ty * 8 + 8)]
            used = {m for row in mats for m in row}
            fits = [p for p, pal in enumerate(scene.PALETTES)
                    if all(pal[scene.MATERIALS[m][0]] == m for m in used)]
            assert fits, f"scene tile ({tx}, {ty}) has {sorted(used)}: no palette has them all"
            t = [[scene.MATERIALS[m][0] for m in row] for row in mats]
            if t not in tiles:
                tiles.append(t)
            tmap.append(base + tiles.index(t))
            attrs.append(fits[0])
    return tiles, tmap, attrs


def rgb555(c):
    r, g, b = c >> 16, (c >> 8) & 255, c & 255
    return (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10


def scene_colours():
    """The start screen's 8 Color palettes: the scene's (the rest its first
    again), then the logo's
    (the sums' colours, as video.c's CPAL_CLUE) for the logo and the line."""
    out = []
    for pal in scene.PALETTES + [scene.PALETTES[0]] * (7 - len(scene.PALETTES)):
        out += [rgb555(scene.MATERIALS[m][1]) for m in pal]
    out += [rgb555(c) for c in (0xF8F0DC, 0xF0B070, 0xD8643C, 0x1E2440)]
    return out


# --- menus ------------------------------------------------------------------

def preview_tiles():
    """A puzzle's preview at 8 dots a cell, a tile each, with the board's
    grid line along the top and left: white (empty), light grey (given).
    Black cells are the black tile."""
    out = []
    for shade in (W, L):
        t = blank(shade)
        for k in range(8):
            t[0][k] = t[k][0] = B
        out.append(t)
    return out


# --- the wave (wave.c) ------------------------------------------------------

WAVE_DEPTH, WAVE_PERIOD = 20, 72       # the sine of the delays: half its height, its length in lines
WAVE_LINES = WAVE_PERIOD + 1           # one period; line 144 (after the last) is line 0 again
WAVE_WIDTH = 159                       # how far a line travels, in dots


def wave_tables():
    """wave_delay[y]: how much later line y (modulo WAVE_PERIOD) starts, a
    sine of y, so the edge is a wave; wave_ease[p]: where a line is after p
    dots of progress, starting and ending slowly (smoothstep)."""
    delay = [round(WAVE_DEPTH * (1 + math.sin(2 * math.pi * y / WAVE_PERIOD))) for y in range(WAVE_LINES)]
    ease = []
    for p in range(WAVE_WIDTH + 1):
        x = p / WAVE_WIDTH
        ease.append(round(WAVE_WIDTH * x * x * (3 - 2 * x)))
    assert ease[0] == 0 and ease[-1] == WAVE_WIDTH
    return bytes(delay), bytes(ease)


# --- title logo -----------------------------------------------------------

LOGO_ROWS = 5                          # tile rows of the logo band


LOGO_PARTS = 7                         # the sum, then the six letters


def logo_image(parts=range(LOGO_PARTS)):
    """160 x 40: a black band with a strip of cells spelling KAKURO, after a
    sum cell: 26, for 2026 (and six different digits can make it, one for
    every letter: 1+2+3+4+7+9). parts: what is
    filled in, 0 the sum and 1-6 the letters; the cells are always there."""
    img = blank(B, 160, LOGO_ROWS * 8)
    size, count = 20, 7                    # in from the border
    x0, y0 = (160 - (size * count + 1)) // 2, (LOGO_ROWS * 8 - (size + 1)) // 2
    for k in range(count):
        cx = x0 + k * size
        if k == 0:
            for y in range(1, size):
                for x in range(1, size):
                    if x > y:
                        img[y0 + y][cx + x] = D
            if 0 in parts:
                number(img, 26, cx + size - 3, y0 + 3, W)
            continue
        for y in range(1, size):
            for x in range(1, size):
                img[y0 + y][cx + x] = W
        if k not in parts:
            continue
        glyph = FONT["KAKURO"[k - 1]]
        big = ["".join(ch * 2 for ch in row) for row in glyph for _ in (0, 1)]
        stamp(img, big, cx + 1 + (size - 1 - 10) // 2, y0 + 1 + (size - 1 - 14) // 2, B)
    return img


def border_image():
    """160 x 144: black, with a border all round the title screens: a line
    2 dots in, and at the corners brackets 2 dots thick, as the board's
    cursor has."""
    img = blank(B, 160, 144)
    for x in range(2, 158):
        img[2][x] = img[141][x] = W
    for y in range(2, 142):
        img[y][2] = img[y][157] = W
    for cx, cy, sx, sy in ((1, 1, 1, 1), (158, 1, -1, 1), (1, 142, 1, -1), (158, 142, -1, -1)):
        for k in range(10):
            for t in range(2):
                img[cy + sy * t][cx + sx * k] = W
                img[cy + sy * k][cx + sx * t] = W
    return img


def framed(image, top):
    """image (rows from screen row top on) with the border over it."""
    border = border_image()
    return [[W if border[top + y][x] == W else c for x, c in enumerate(row)] for y, row in enumerate(image)]


def logo_columns(part):
    """The tile columns where the logo with only `part` filled in differs
    from the empty one."""
    empty = tiles_of(logo_image(()))
    alone = tiles_of(logo_image((part,)))
    return sorted({k % 20 for k, (a, b) in enumerate(zip(empty, alone)) if a != b})


# --- sprites --------------------------------------------------------------

def sprite_tiles():
    """Sprite colours: 0 transparent, 1 white, 2 dark grey, 3 black (with
    OBP0 = 0xE0; OBP1 swaps white and black for the chosen key and keeps
    dark grey). 0: cursor corner (top-left; flipped for the others); 1-9:
    keypad keys; 10: the erase key; then the keypad's edges.

    The corner is a dark grey bracket, 2 dots thick and 4 long, so it stands
    out from the black grid lines and digits. A key is a 7x7 square inside
    a dark grey line; keys are 8 dots apart and share the line, so a key's
    tile has it along its top and left only, and the edge tiles close the
    keypad's right and bottom: a line down, a line across, both, a dot."""
    corner = blank(0)
    for k in range(4):
        for t in (0, 1):
            corner[t][k] = corner[k][t] = D
    out = [corner]
    for key in range(1, 11):
        t = blank(1)
        for k in range(8):
            t[0][k] = t[k][0] = D
        if key <= 9:
            stamp(t, SMALL[key], 3, 2, B)       # 2 dots each side, 1 above and below
        else:
            stamp(t, ["#.#", ".#.", "#.#"], 3, 3, B)
        out.append(t)
    down, across, both, dot = blank(0), blank(0), blank(0), blank(0)
    for k in range(8):
        down[k][0] = both[k][0] = D
        across[0][k] = both[0][k] = D
    dot[0][0] = D
    out += [down, across, both, dot]
    return out


# --- output ---------------------------------------------------------------

def c_array(name, data, per_line=16):
    items = [f"0x{b:02X}" for b in data]
    body = ",\n".join("    " + ", ".join(items[i:i + per_line]) for i in range(0, len(items), per_line))
    return f"const unsigned char {name}[{len(data)}] = {{\n{body}\n}};\n"


def main() -> int:
    font = font_tiles()
    dim = font_tiles(L, only_digits=True)
    bt = board_tiles()

    index = {}
    order: list = []

    def add(name, tiles):
        index[name] = len(order)
        order.extend(tiles)

    add("FONT", font)
    add("DIM", dim)
    add("BLACK", [bt["black"]])
    add("TL", bt["tl"])
    add("BR", bt["br"])
    add("ACROSS", bt["across"])
    add("DOWN", bt["down"])
    add("WHITE", bt["white"])
    add("DIGIT", bt["digit"])
    add("GIVEN", bt["given"])
    # the Color's cells replace DIGIT and GIVEN, which follow each other
    assert index["GIVEN"] == index["DIGIT"] + len(bt["digit"])
    assert len(bt["cgb_cells"]) == len(bt["digit"]) + len(bt["given"])
    game_count = len(order)
    assert game_count <= 256, game_count
    index["STATUS"] = game_count               # the status line's tiles, drawn by panel.c
    menu_tiles = [bt["black"]] + preview_tiles()    # from 0 on every menu screen
    logo_base = len(menu_tiles)                      # the logo after them, on the title

    # The title screens' frame: the logo and the border, the whole screen;
    # and the logo empty (for the start screen, which fills it in a part at
    # a time: per part its tile columns from the whole one).
    logo = logo_image()
    whole = framed(logo + [row[:] for row in border_image()[LOGO_ROWS * 8:]], 0)
    empty = framed(logo_image(()), 0)
    frame_tiles, maps = [], []
    for image in (whole, empty):
        maps.append([])
        for t in tiles_of(image):
            if t == bt["black"]:
                maps[-1].append(0)
                continue
            if t not in frame_tiles:
                frame_tiles.append(t)
            maps[-1].append(logo_base + frame_tiles.index(t))
    frame_map, logo_empty = maps
    part_columns = [logo_columns(p) for p in range(LOGO_PARTS)]
    scene_base = logo_base + len(frame_tiles)      # on the start screen
    s_tiles, s_map, s_attrs = scene_tiles(scene_base)
    assert scene_base + len(s_tiles) + 12 <= 256    # and the 12 of PRESS START
    for p, cols in enumerate(part_columns):
        assert cols == list(range(cols[0], cols[-1] + 1)), (p, cols)
        assert not any(set(cols) & set(c) for c in part_columns[p + 1:]), p

    sprites = sprite_tiles()
    delay, ease = wave_tables()

    h = ["/* Generated by tools/gen_gfx.py -- do not edit. */",
         "#ifndef GFX_H", "#define GFX_H", "",
         "/* Background tiles while playing (loaded at 0, BG data at 0x8800). */"]
    for name, value in index.items():
        h.append(f"#define T_{name:<8} {value}")
    h += [f"#define GAME_TILE_COUNT {game_count}",
          f"#define STATUS_TILE_MAX {256 - game_count}", "",
          f"#define SUM_MIN {SUM_MIN}",
          "/* The font covers ASCII 32..95: tile = T_FONT + ch - 32. */", "",
          "/* Menu screens: black and the preview's cells from 0 on, then the tiles",
          " * each screen draws (from M_FREE); the title has the logo first. */",
          "#define M_BLACK 0", "#define M_PREVIEW 1                /* white, given */",
          f"#define MENU_TILE_COUNT {len(menu_tiles)}",
          f"#define M_FREE {len(menu_tiles)}",
          "/* The title screens' frame (the logo and a border round the screen):",
          " * its tiles from M_LOGO, its map of the whole screen (inside black). */",
          f"#define M_LOGO {logo_base}",
          f"#define FRAME_TILE_COUNT {len(frame_tiles)}",
          f"#define M_FREE_TITLE {logo_base + len(frame_tiles)}",
          f"#define LOGO_W 20", f"#define LOGO_H {LOGO_ROWS}",
          "extern const unsigned char frame_tiles[], frame_map[];",
          "/* The start screen's scene (tools/scene.py): its tiles from M_FREE_TITLE,",
          " * its map from row SCENE_Y and Color palettes (the logo's is the last,",
          " * SCENE_PAL_LOGO). */",
          f"#define SCENE_TILE_COUNT {len(s_tiles)}",
          f"#define SCENE_Y {scene.TOP // 8}",
          f"#define SCENE_H {scene.H // 8}",
          "#define SCENE_PAL_LOGO 7",
          "extern const unsigned char scene_tiles[], scene_map[], scene_attr[];",
          "extern const unsigned int scene_colours[32];",
          "/* The logo with empty cells, and per part (the sum, then the letters)",
          " * the first tile column and how many, where it is filled in. */",
          f"#define LOGO_PARTS {LOGO_PARTS}",
          "extern const unsigned char logo_empty[], logo_part_x[], logo_part_w[];", "",
          "/* Sprite tiles (0x8000). */",
          "#define S_CORNER 0", "#define S_KEY 1",
          "#define S_EDGE 11                  /* down, across, both, dot */",
          f"#define SPRITE_TILE_COUNT {len(sprites)}", "",
          "/* The Color's DIGIT and GIVEN tiles (the digit dark grey), from T_DIGIT on. */",
          f"#define CGB_CELL_COUNT {len(bt['cgb_cells'])}",
          "extern const unsigned char cgb_cell_tiles[];", "",
          "/* The wave: line y starts wave_delay[y % WAVE_PERIOD] dots of progress late;",
          " * after p dots of progress a line has come wave_ease[p] dots (0..WAVE_WIDTH). */",
          f"#define WAVE_PERIOD {WAVE_PERIOD}", f"#define WAVE_LINES {WAVE_LINES}", f"#define WAVE_WIDTH {WAVE_WIDTH}",
          f"#define WAVE_END {WAVE_WIDTH + max(delay)}             /* progress when every line is in */",
          "extern const unsigned char wave_delay[WAVE_LINES], wave_ease[WAVE_WIDTH + 1];", "",
          "/* The status line's font: tiny_font[(ch - 32) * 6] is the width in dots, then 5 rows. */",
          "extern const unsigned char game_tiles[], menu_tiles[];",
          "extern const unsigned char sprite_tiles[], tiny_font[];",
          "", "#endif", ""]
    c = ["/* Generated by tools/gen_gfx.py -- do not edit. */", '#include "gfx.h"', "",
         c_array("game_tiles", b"".join(encode(t) for t in order)),
         c_array("menu_tiles", b"".join(encode(t) for t in menu_tiles)),
         c_array("frame_tiles", b"".join(encode(t) for t in frame_tiles)),
         c_array("sprite_tiles", b"".join(encode(t) for t in sprites)),
         c_array("cgb_cell_tiles", b"".join(encode(t) for t in bt["cgb_cells"])),
         c_array("frame_map", bytes(frame_map), 20),
         c_array("logo_empty", bytes(logo_empty), 20),
         c_array("scene_tiles", b"".join(encode(t) for t in s_tiles)),
         c_array("scene_map", bytes(s_map), 20),
         c_array("scene_attr", bytes(s_attrs), 20),
         "const unsigned int scene_colours[32] = {\n    "
         + ", ".join(f"0x{c:04X}" for c in scene_colours()) + "\n};\n",
         c_array("logo_part_x", bytes(c[0] for c in part_columns)),
         c_array("logo_part_w", bytes(len(c) for c in part_columns)),
         c_array("tiny_font", tiny_font(), 6),
         c_array("wave_delay", delay),
         c_array("wave_ease", ease)]
    OUT_H.write_text("\n".join(h))
    OUT_C.write_text("\n".join(c))
    print(f"wrote src/gfx.h and .c: {game_count} game tiles, {len(menu_tiles)} menu, "
          f"{len(frame_tiles)} frame and {len(s_tiles)} scene tiles, "
          f"{len(sprites)} sprite tiles")

    if "--preview" in sys.argv:
        preview(order, logo)
    return 0


def preview(order, logo):
    from PIL import Image
    shades = [(224, 248, 208), (136, 192, 112), (52, 104, 86), (8, 24, 32)]
    cols = 16
    rows = (len(order) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * 9, rows * 9 + 48), (255, 0, 255))
    for k, t in enumerate(order):
        for y in range(8):
            for x in range(8):
                sheet.putpixel(((k % cols) * 9 + x, (k // cols) * 9 + y), shades[t[y][x]])
    for y, row in enumerate(logo):
        for x, c in enumerate(row):
            if x < sheet.width:
                sheet.putpixel((x, rows * 9 + 4 + y), shades[c])
    out = ROOT / "build/gfx_preview.png"
    out.parent.mkdir(exist_ok=True)
    sheet.resize((sheet.width * 4, sheet.height * 4), Image.NEAREST).save(out)
    print(f"preview: {out.relative_to(ROOT)}")


if __name__ == "__main__":
    sys.exit(main())
