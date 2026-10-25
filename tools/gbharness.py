"""PyBoy driver for the screenshot and test scripts.

Runs the ROM headless (a ROM-only cartridge: every run starts from power-on
with nothing played), presses buttons the way a player would, and reads and
writes the game's variables through the symbols in build/kakuro.noi.
"""
from __future__ import annotations

import re
import warnings
from pathlib import Path

warnings.filterwarnings("ignore")
from pyboy import PyBoy  # noqa: E402

# A neutral grey ramp for screenshots (white .. black), and the green of
# the original Game Boy's screen.
SHADES = (0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000)
GREEN = (0xE0F8D0, 0x88C070, 0x346856, 0x081820)


DIRECTIONS = ("up", "down", "left", "right")


def move_model(n: int, white: list[bool], cur: int, direction: str) -> int | None:
    """The cursor rule of game.c: the nearest white cell straight ahead,
    else the closest in the quarter plane before the cursor; None if none."""
    r, c = divmod(cur, n)
    for j in range(2 * n + 1):
        o = (j + 1) // 2 if j & 1 else -(j // 2)
        for k in range(1, n):
            if abs(o) > k:
                continue
            rr, cc = {"up": (r - k, c + o), "down": (r + k, c + o),
                      "left": (r + o, c - k), "right": (r + o, c + k)}[direction]
            if 0 <= rr < n and 0 <= cc < n and white[rr * n + cc]:
                return rr * n + cc
    return None


class Game:
    def __init__(self, rom: str | Path, palette=SHADES, cgb: bool = False):
        """cgb: run it on a Game Boy Color (else on the original, in shades)."""
        rom = Path(rom)
        self.symbols = {}
        noi = rom.with_suffix(".noi")
        for m in re.finditer(r"^DEF (\w+) 0x([0-9A-F]+)", noi.read_text(), re.M):
            self.symbols[m.group(1)] = int(m.group(2), 16)
        self.pb = PyBoy(str(rom), window="null", sound_emulated=False, color_palette=palette, cgb=cgb)
        self.pb.set_emulation_speed(0)

    # --- time and buttons ---------------------------------------------------
    def wait(self, frames: int = 1) -> None:
        self.pb.tick(frames, True)

    def press(self, button: str, hold: int = 2, after: int = 4) -> None:
        self.pb.button(button, hold)
        self.wait(hold + after)

    def presses(self, button: str, count: int, after: int = 4) -> None:
        for _ in range(count):
            self.press(button, after=after)

    def settle(self) -> None:
        """Waits until a new screen is up: faded out, loaded, faded in."""
        mem = self.pb.memory
        for _ in range(120):
            if mem[0xFF47] != 0xE4 or not mem[0xFF40] & 0x80:
                break
            self.wait(1)
        steady = 0
        for _ in range(1200):
            steady = steady + 1 if mem[0xFF47] == 0xE4 and mem[0xFF40] & 0x80 else 0
            if steady == 10:
                break
            self.wait(1)

    def boot(self) -> None:
        """From power-on past the start screen to the title."""
        self.settle()
        self.to_screen("start")

    def to_screen(self, button: str) -> None:
        """Presses a button that leads to another screen and waits for it."""
        self.pb.button(button, 2)
        self.settle()

    # --- playing ------------------------------------------------------------
    def white(self) -> list[bool]:
        n = self.byte("n")
        return [bool(self.byte("cell", i) & 0x80) for i in range(n * n)]

    def goto(self, target: int) -> None:
        """Moves the cursor to cell `target` with the fewest D-pad presses,
        checking every step against move_model()."""
        n, white = self.byte("n"), self.white()
        start = self.byte("cur")
        paths = {start: []}
        queue = [start]
        while queue and target not in paths:
            here = queue.pop(0)
            for d in DIRECTIONS:
                there = move_model(n, white, here, d)
                if there is not None and there not in paths:
                    paths[there] = paths[here] + [(d, there)]
                    queue.append(there)
        assert target in paths, f"cell {target} cannot be reached from {start}"
        for d, there in paths[target]:
            self.press(d, after=2)
            assert self.byte("cur") == there, (d, self.byte("cur"), there)

    def enter(self, digit: int) -> None:
        """Opens the keypad, walks to `digit` (1-9, 0 to wipe) and presses A."""
        self.press("a", after=3)
        key = self.byte("key")
        want = digit or 10
        if key == 10 and want != 10:
            self.press("up", after=2)
            key = 8
        if want == 10:
            col, row = 1, 3
        else:
            col, row = (want - 1) % 3, (want - 1) // 3
        kc, kr = (key - 1) % 3, (key - 1) // 3
        if key != 10:
            for _ in range(abs(col - kc)):
                self.press("right" if col > kc else "left", after=2)
            for _ in range(abs(row - kr)):
                self.press("down" if row > kr else "up", after=2)
        assert self.byte("key") == want, (self.byte("key"), want)
        self.press("a", after=3)

    # --- memory -------------------------------------------------------------
    def addr(self, name: str) -> int:
        return self.symbols["_" + name]

    def byte(self, name: str, offset: int = 0) -> int:
        return self.pb.memory[self.addr(name) + offset]

    def word(self, name: str) -> int:
        a = self.addr(name)
        return self.pb.memory[a] | self.pb.memory[a + 1] << 8

    def bytes(self, name: str, count: int) -> list[int]:
        a = self.addr(name)
        return [self.pb.memory[a + k] for k in range(count)]

    def reg(self, address: int) -> int:
        return self.pb.memory[address]

    # --- output -------------------------------------------------------------
    def shot(self, path: str | Path) -> None:
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        self.pb.screen.image.convert("RGB").save(path)

    def screen_rgb(self):
        return self.pb.screen.image.convert("RGB")

    def poke(self, name: str, offset: int, value: int) -> None:
        self.pb.memory[self.addr(name) + offset] = value

    def stop(self) -> None:
        self.pb.stop(save=False)
