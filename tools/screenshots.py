#!/usr/bin/env python3
"""Screenshots of the game's screens -> build/screens/*.png (at 3x), and
build/screens/sheet.png with all of them. With --docs: in Game Boy green,
the ones the README shows -> docs/*.png.

  python3 tools/screenshots.py build/kakuro.gb [--docs]
"""
from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gbharness import GREEN, SHADES, Game  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
DOCS = "--docs" in sys.argv
OUT = ROOT / ("docs" if DOCS else "build/screens")
DOC_SHOTS = {"start", "title", "grid", "playing", "keypad", "combinations", "pause", "quit", "wave", "solved",
             "color_keypad", "color_grid"}
PALETTE = GREEN if DOCS else SHADES
SCALE = 3
shots: list[Path] = []


def save(game: Game, name: str) -> None:
    if DOCS and name not in DOC_SHOTS:
        return
    img = game.screen_rgb()
    path = OUT / f"{name}.png"
    path.parent.mkdir(parents=True, exist_ok=True)
    img.resize((img.width * SCALE, img.height * SCALE), Image.NEAREST).save(path)
    shots.append(path)
    print(path.relative_to(ROOT))


def solve(game: Game, leave: int = 0) -> None:
    """Enters the solution in every open cell except the last `leave`."""
    cells = [i for i in range(game.byte("ncells"))
             if game.byte("cell", i) & 0x80 and not game.byte("cell", i) & 0x10]
    for i in cells[:len(cells) - leave]:
        game.goto(i)
        game.enter(game.byte("solution", i))


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    rom = args[0] if args else str(ROOT / "build/kakuro.gb")
    game = Game(rom, palette=PALETTE)
    game.settle()
    game.wait(200)                          # the logo filled in
    while not game.reg(0x9800 + 32 * 16 + 5):   # PRESS START on (not the black tile)
        game.wait(1)
    save(game, "start")
    game.to_screen("start")
    save(game, "title")
    game.presses("down", 2)                 # 8x8 **: the cards' last row, first column
    game.press("left")
    game.to_screen("a")
    game.wait(20)
    save(game, "grid")
    game.to_screen("a")
    save(game, "board")
    solve(game, leave=game.byte("empty") * 2 // 3)
    game.wait(20)
    save(game, "playing")
    game.press("a", after=10)
    save(game, "keypad")
    game.press("b", after=10)
    game.press("select", after=30)
    save(game, "combinations")
    game.press("select", after=30)
    game.press("start", after=30)
    save(game, "pause")
    game.presses("down", 4)
    game.press("a", after=30)               # QUIT TO MENU asks first
    save(game, "quit")
    game.press("down")
    game.to_screen("a")                     # quit to the grid
    game.wait(20)
    save(game, "grid_after")
    game.stop()

    game = Game(rom, palette=PALETTE)       # a whole 6x6 puzzle, solved
    game.boot()
    game.to_screen("a")
    game.to_screen("a")
    solve(game)
    while not game.reg(0xFFFF) & 2:         # the wave's line interrupt
        game.wait(1)
    game.wait(12)
    save(game, "wave")
    game.wait(60)
    save(game, "solved")
    game.stop()

    game = Game(rom, cgb=True)              # on a Game Boy Color: an 8x8 puzzle
    game.boot()
    game.presses("down", 2)
    game.press("left")
    game.to_screen("a")
    game.wait(20)
    save(game, "color_grid")
    game.to_screen("a")
    solve(game, leave=game.byte("empty") * 2 // 3)
    game.press("a", after=10)
    save(game, "color_keypad")
    game.stop()

    if DOCS:
        return 0
    images = [Image.open(p) for p in shots]
    w, h = images[0].size
    cols = 4
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (w + 12) - 12, rows * (h + 12) - 12), (40, 40, 40))
    for k, im in enumerate(images):
        sheet.paste(im, ((k % cols) * (w + 12), (k // cols) * (h + 12)))
    sheet.save(OUT / "sheet.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
