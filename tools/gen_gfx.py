#!/usr/bin/env python3
"""Generate src/gfx.h and src/gfx.c: every tile of the game, drawn here.

Colours are the Game Boy's four shades: 0 white, 1 light grey, 2 dark grey,
3 black (BGP = 0xE4). The board is made of 16x16 cells, four tiles each:

- A white cell has a black grid line along its top and left edge; the
  right and bottom edges are the next cell's lines (or the black around the
  board). Entered digits are black on white, given digits black on light
  grey.
- A black cell is black. When it holds sums it gets a black diagonal from
  the top-left corner: the triangle above it holds the sum across (dark grey
  with a white 3x5 number in the top-right tile), the one below it the sum
  down (in the bottom-left tile). A triangle without a sum stays black.

The tiles are loaded from index 0 with BG tile data at 0x8800 (signed
addressing is handled in video.c); sprite tiles live at 0x8000-0x87FF, so
they never share space with the background.

  python3 tools/gen_gfx.py [--preview]   (preview: build/gfx_preview.png)
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT_H = ROOT / "src/gfx.h"
OUT_C = ROOT / "src/gfx.c"

W, L, D, B = 0, 1, 2, 3

# 5x7 font for ASCII 32..95. Unused punctuation is blank; a few codes carry
# the game's own symbols: [ right arrow, \ down arrow, ] check mark,
# ^ star, _ bullet.
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
SUM_MIN, SUM_MAX = 3, 45
SUM_COUNT = SUM_MAX - SUM_MIN + 1


def blank(color=W, w=8, h=8):
    return [[color] * w for _ in range(h)]


def stamp(img, glyph, x0, y0, color):
    for y, row in enumerate(glyph):
        for x, ch in enumerate(row):
            if ch == "#":
                img[y0 + y][x0 + x] = color


def number(img, value, x_right, y0, color):
    """A 3x5 number, right-aligned at column x_right (inclusive)."""
    digits = str(value)
    x = x_right - (4 * len(digits) - 1) + 1
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
        stamp(t, FONT.get(ch, []), 1, 0, color)
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
        number(img, down, 7, 9, W)
    return img


def white_cell(digit=0, given=False):
    img = blank(L if given else W, 16, 16)
    for k in range(16):
        img[0][k] = img[k][0] = B
    if digit:
        stamp(img, BIG[digit], 5, 3, B)
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
    # The numbers must not reach the diagonal tiles: check that the TL/BR
    # tiles of a cell with two-digit sums are the plain ones.
    q = tiles_of(clue_cell(45, 45))
    assert q[0] == tiles["tl"][3] and q[3] == tiles["br"][3], "sum digits overlap the diagonal"
    return tiles


# --- title logo -----------------------------------------------------------

LOGO_ROWS = 5                          # tile rows of the logo band


def logo_image():
    """160 x 40: a black band with a strip of cells spelling KAKURO, after a
    sum cell (21 = 1+2+...+6, one digit for every letter)."""
    img = blank(B, 160, LOGO_ROWS * 8)
    size, count = 22, 7
    x0, y0 = (160 - (size * count + 1)) // 2, (LOGO_ROWS * 8 - (size + 1)) // 2
    for k in range(count):
        cx = x0 + k * size
        if k == 0:
            for y in range(1, size):
                for x in range(1, size):
                    if x > y:
                        img[y0 + y][cx + x] = D
            number(img, 21, cx + size - 3, y0 + 3, W)
            continue
        for y in range(1, size):
            for x in range(1, size):
                img[y0 + y][cx + x] = W
        glyph = FONT["KAKURO"[k - 1]]
        big = ["".join(ch * 2 for ch in row) for row in glyph for _ in (0, 1)]
        stamp(img, big, cx + 1 + (size - 1 - 10) // 2, y0 + 1 + (size - 1 - 14) // 2, B)
    return img


# --- sprites --------------------------------------------------------------

def sprite_tiles():
    """Sprite colours: 0 transparent, 1 white, 2 dark grey, 3 black (with
    OBP0 = 0xE0; OBP1 swaps white and black for the chosen key). 0: cursor
    corner (top-left; flipped for the others); 1-9: keypad keys; 10: the
    erase key. The corner is a dark grey bracket, 2 dots thick and 4 long,
    so it stands out from the black grid lines and digits."""
    corner = blank(0)
    for k in range(4):
        for t in (0, 1):
            corner[t][k] = corner[k][t] = D
    out = [corner]
    for key in range(1, 11):
        t = blank(1)
        for k in range(8):
            t[0][k] = t[7][k] = t[k][0] = t[k][7] = B
        if key <= 9:
            stamp(t, SMALL[key], 3, 2, B)
        else:
            stamp(t, ["#.#", ".#.", "#.#"], 3, 3, B)
        out.append(t)
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
    game_count = len(order)
    assert game_count <= 256, game_count
    menu_base = index["BLACK"]                 # menu tiles replace the board tiles

    logo = logo_image()
    logo_tiles, logo_map = [], []
    for t in tiles_of(logo):
        if t == bt["black"]:
            logo_map.append(menu_base)         # the first menu tile is black too
            continue
        if t not in logo_tiles:
            logo_tiles.append(t)
        logo_map.append(menu_base + 1 + logo_tiles.index(t))
    menu_tiles = [bt["black"]] + logo_tiles
    preview_base = menu_base + len(menu_tiles)
    assert preview_base + 25 <= 256

    sprites = sprite_tiles()

    h = ["/* Generated by tools/gen_gfx.py -- do not edit. */",
         "#ifndef GFX_H", "#define GFX_H", "",
         "/* Background tiles while playing (loaded at 0, BG data at 0x8800). */"]
    for name, value in index.items():
        h.append(f"#define T_{name:<8} {value}")
    h += [f"#define GAME_TILE_COUNT {game_count}", "",
          f"#define SUM_MIN {SUM_MIN}",
          "/* The font covers ASCII 32..95: tile = T_FONT + ch - 32. */", "",
          "/* Menu screens: the board tiles are replaced by the logo and preview tiles. */",
          f"#define T_MENU {menu_base}",
          f"#define MENU_TILE_COUNT {len(menu_tiles)}",
          f"#define T_PREVIEW {preview_base}",
          f"#define LOGO_W 20", f"#define LOGO_H {LOGO_ROWS}", "",
          "/* Sprite tiles (0x8000). */",
          "#define S_CORNER 0", "#define S_KEY 1", f"#define SPRITE_TILE_COUNT {len(sprites)}", "",
          "extern const unsigned char game_tiles[], menu_tiles[], sprite_tiles[], logo_map[];",
          "", "#endif", ""]
    c = ["/* Generated by tools/gen_gfx.py -- do not edit. */", '#include "gfx.h"', "",
         c_array("game_tiles", b"".join(encode(t) for t in order)),
         c_array("menu_tiles", b"".join(encode(t) for t in menu_tiles)),
         c_array("sprite_tiles", b"".join(encode(t) for t in sprites)),
         c_array("logo_map", bytes(logo_map), 20)]
    OUT_H.write_text("\n".join(h))
    OUT_C.write_text("\n".join(c))
    print(f"wrote src/gfx.h and .c: {game_count} game tiles, {len(menu_tiles)} menu tiles, "
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
