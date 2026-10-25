"""The start screen's scene, 160 x 88 dots under the logo: a dry garden
seen from above, sand raked in rings round four stones, a maple's crown in
the corner, leaves on the sand, the edge of the veranda.

It is drawn in materials, each with its shade on the DMG; the Color gives
every tile one of PALETTES (shade i of a palette is the material at i), so a
tile may only hold materials one palette has. The title screens' border
(gen_gfx.py) runs down its sides, in the EDGE materials: the line's, and
that outside it.
"""
from __future__ import annotations

import math
import random

W, H = 160, 88
TOP = 40                         # the scene's first screen row, under the logo
EDGE = ("sand", "shadow")


def canvas(fill):
    img = [[fill] * W for _ in range(H)]

    def put(x, y, m):
        if 0 <= x < W and 0 <= y < H:
            img[y][x] = m
    return img, put


MATERIALS = {
    "sand":   (0, 0xF2EAD6),
    "rake":   (1, 0xCDBE9C),     # the raked lines, the stones' light side
    "stone":  (2, 0x7E7A70),
    "maple":  (2, 0xD0482C),
    "wood":   (2, 0x9A6A44),
    "shadow": (3, 0x3A3430),
}

PALETTES = [
    ("sand", "rake", "stone", "shadow"),
    ("sand", "rake", "maple", "shadow"),
    ("sand", "rake", "wood", "shadow"),
]

STONES = ((44, 38, 9, 6), (104, 54, 12, 8), (124, 26, 6, 4), (70, 70, 5, 4))   # centre, radii


def ellipse(put, cx, cy, rx, ry, material):
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1:
                put(x, y, material)


def draw() -> list[list[str]]:
    img, put = canvas("sand")
    # the raking: rings round the stones, 4 dots apart, straight lines elsewhere
    for y in range(H):
        for x in range(W):
            ring = None
            for sx, sy, rx, ry in STONES:
                d = (math.hypot((x - sx) / (rx + 1), (y - sy) / (ry + 1)) - 1) * (rx + ry) / 2
                if d < 13 and (ring is None or d < ring):
                    ring = d
            if ring is not None:
                if ring >= 0 and int(ring) % 4 == 2:
                    img[y][x] = "rake"
            elif y % 4 == 1:
                img[y][x] = "rake"
    for sx, sy, rx, ry in STONES:           # each with its shadow and a light side
        ellipse(put, sx + 2, sy + 2, rx, ry, "shadow")
        ellipse(put, sx, sy, rx, ry, "stone")
        ellipse(put, sx - rx * 0.3, sy - ry * 0.35, rx * 0.45, ry * 0.4, "rake")
    # a maple's crown over the top right corner, the shade in it, fallen leaves
    rnd = random.Random(7)
    for _ in range(70):
        a, r = rnd.uniform(math.pi * 0.5, math.pi * 1.05), rnd.uniform(0, 30)
        ellipse(put, 160 + r * math.cos(a) * 1.4, -6 + r * math.sin(a) * 0.9, *[rnd.uniform(2, 4)] * 2, "maple")
    for _ in range(50):
        a, r = rnd.uniform(math.pi * 0.5, math.pi * 1.05), rnd.uniform(0, 28)
        put(round(160 + r * math.cos(a) * 1.4), round(-8 + r * math.sin(a) * 0.9), "shadow")
    for _ in range(16):
        x, y = rnd.randrange(70, 150), rnd.randrange(6, 50)
        if img[y][x] in ("sand", "rake") and all(math.hypot(x - sx, y - sy) > rx + 10 for sx, sy, rx, ry in STONES):
            put(x, y, "maple")
            put(x + 1, y, "maple")
    # the veranda's edge along the bottom: planks
    for y in range(80, H):
        for x in range(W):
            img[y][x] = "shadow" if y == 80 or (x - 6) % 22 == 0 else "wood"
    return img
