#!/usr/bin/env python3
"""Plays the ROM in PyBoy the way a player would and checks the game.

0. The start screen: the logo filled in, the scene under it, START to the
   title.
1. Every puzzle, in order: the puzzle in memory (cells, givens, sums, runs)
   against src/puzzles.c and against the .puz files themselves, the board
   on screen tile by tile, and then the puzzle solved with the D-pad and the
   keypad. Every tenth puzzle first gets a wrong last digit, which must be
   refused ("NOT YET RIGHT"). A solved puzzle must lead on the wave to the
   overview, with the number solved. After the last puzzle every best time
   must be in the store (work RAM: the cartridge is ROM only).
2. On the first puzzle: given digits cannot be changed, B and the keypad's
   erase key wipe a cell, the combinations panel against a Python model,
   the hint, and starting over.
3. Quitting a puzzle asks first; KEEP PLAYING (or B) keeps it, QUIT drops
   it: opened again it starts afresh. The grid screen marks solved puzzles.
4. The header: a ROM-only cartridge, without RAM, for the Color too.
5. On a Game Boy Color: every cell of the first puzzle has its palette,
   and the puzzle is solved through to the overview.

  python3 tools/test_game.py build/kakuro.gb
"""
from __future__ import annotations

import re
import sys
import time
from itertools import combinations
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from gbharness import SHADES, Game  # noqa: E402
import gen_puzzles  # noqa: E402

WIN_MAP, BG_MAP = 0x9C00, 0x9800
PUZZLE_COUNT = int(re.search(r"#define PUZZLE_COUNT (\d+)", (ROOT / "src/puzzles.h").read_text()).group(1))
PANEL_CLOSED = 136
failures: list[str] = []


def check(condition: bool, message: str) -> bool:
    if not condition:
        failures.append(message)
        print("  FAIL:", message)
    return condition


# --- models -----------------------------------------------------------------

def c_array(text: str, name: str) -> list[int]:
    m = re.search(name + r"\[[^\]]*\] = \{(.*?)\};", text, re.S)
    return [int(v, 0) for v in re.findall(r"0x[0-9A-Fa-f]+|\d+", m.group(1))]


def load_model() -> list[dict]:
    """The puzzles as the ROM stores them (src/puzzles.c), checked against
    the .puz files with the givens the generator adds."""
    text = (ROOT / "src/puzzles.c").read_text()
    gn, gs, gf, gc = (c_array(text, k) for k in ("group_n", "group_stars", "group_first", "group_count"))
    offsets, data = c_array(text, "puzzle_offset"), c_array(text, "puzzle_data")
    puzzles = []
    for g in range(len(gn)):
        for k in range(gf[g], gf[g] + gc[g]):
            n, m, p = gn[g], gn[g] - 1, offsets[k]
            nib = [(data[p + j // 2] >> (0 if j & 1 else 4)) & 15 for j in range(m * m)]
            sol = [0] * (n * n)
            for j, v in enumerate(nib):
                sol[(j // m + 1) * n + j % m + 1] = v
            p += (m * m + 1) // 2
            given = {(v // m + 1) * n + v % m + 1 for v in data[p + 1:p + 1 + data[p]]}
            puzzles.append({"n": n, "stars": gs[g], "group": g, "solution": sol, "given": given})

    sources = []
    for path in sorted((ROOT / "assets/puzzles").glob("*.puz")):
        p = gen_puzzles.Puzzle(path)
        if p.check():
            continue
        p.make_unique()
        sources.append(p)
    sources.sort(key=lambda p: (p.n, p.stars, p.number))
    check(len(sources) == len(puzzles), f"{len(puzzles)} puzzles in the ROM, {len(sources)} sources")
    for k, (p, q) in enumerate(zip(puzzles, sources)):
        check(p["n"] == q.n and p["stars"] == q.stars and p["solution"] == q.solution
              and p["given"] == q.given, f"puzzle {k + 1}: ROM data differs from {q.name}.puz")
    for p in puzzles:
        p.update(sums(p["n"], p["solution"]))
    return puzzles


def sums(n: int, sol: list[int]) -> dict:
    """sum/len per black cell and the head of every white cell, as puzzle.c."""
    out = {key: [0] * (n * n) for key in ("sum_a", "sum_d", "len_a", "len_d")}
    out["head_a"], out["head_d"] = [0xFF] * (n * n), [0xFF] * (n * n)
    for i in range(n * n):
        if sol[i]:
            continue
        for step, room, key in ((1, n - 1 - i % n, "a"), (n, n - 1 - i // n, "d")):
            run = []
            j = i
            while len(run) < room and sol[j + step]:
                j += step
                run.append(j)
            out["len_" + key][i] = len(run)
            if len(run) >= 2:
                out["sum_" + key][i] = sum(sol[j] for j in run)
                for j in run:
                    out["head_" + key][j] = i
    return out


def gfx_constants() -> dict:
    return {m.group(1): int(m.group(2)) for m in
            re.finditer(r"#define (\w+)\s+(\d+)", (ROOT / "src/gfx.h").read_text())}


GFX = gfx_constants()


def expected_map(p: dict, cells: list[int]) -> list[list[int]]:
    """The (2n)^2 background tiles of the board, as board.c draws them."""
    n = p["n"]
    size = 2 * n
    grid = [[None] * size for _ in range(size)]
    for i in range(n * n):
        v = cells[i]
        if v & 0x80:
            d = v & 15
            base = GFX["T_WHITE"] if not d else (GFX["T_GIVEN"] if v & 0x10 else GFX["T_DIGIT"]) + (d - 1) * 4
            q = [base, base + 1, base + 2, base + 3]
        else:
            a, dn = p["sum_a"][i], p["sum_d"][i]
            k = (2 if a else 0) | (1 if dn else 0)
            q = [GFX["T_TL"] + k, GFX["T_ACROSS"] + a - 3 if a else GFX["T_BLACK"],
                 GFX["T_DOWN"] + dn - 3 if dn else GFX["T_BLACK"], GFX["T_BR"] + k]
        r, c = divmod(i, n)
        for qi, (dy, dx) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1))):
            grid[2 * r + dy][2 * c + dx] = q[qi]
    return grid


# --- reading the screen -----------------------------------------------------

def map_text(game: Game, base: int, row: int) -> str:
    """A map row as text (the font's tiles; any other one is ?)."""
    out = []
    for x in range(20):
        t = game.reg(base + 32 * row + x)
        out.append(chr(t + 32) if t < 64 else "?")
    return "".join(out)


def shade(game: Game, x: int, y: int) -> int:
    """The shade (0 white .. 3 black) of a dot on the screen."""
    r, g, b = game.screen_rgb().getpixel((x, y))
    return SHADES.index(r << 16 | g << 8 | b)


def on_title(game: Game) -> bool:
    """Whether the cursor (sprite 0, its top-left bracket) is on one of the
    title's cards (menu.c: rows 6-14)."""
    return 40 <= game.reg(0xFE00) - 16 < 120


def grid_cell(game: Game, j: int) -> tuple[int, int]:
    """Puzzle j of the grid screen: the shade of its cell, and of a dot of
    the check mark in its top right (menu.c: solved ones have it)."""
    x, y = 8 + 16 * (j % 5), 24 + 16 * (j // 5)
    return shade(game, x + 8, y + 13), shade(game, x + 11, y + 5)


def status_text(game: Game) -> str:
    """The status line's text left of the clock (it is drawn dot by dot)."""
    return bytes(game.bytes("status_text", 16)).split(b"\0")[0].decode()


def wy(game: Game) -> int:
    return game.reg(0xFF4A)


def wait_overview(game: Game, frames: int = 300) -> tuple[bool, bool]:
    """Waits for the overview to cover the screen (window at 7, 0 and the
    line interrupt off again); returns (got there, the wave ran)."""
    waved = False
    for _ in range(frames):
        on = bool(game.reg(0xFFFF) & 2)
        waved |= on
        if waved and not on and game.reg(0xFF4B) == 7 and wy(game) == 0:
            return True, waved
        game.wait(1)
    return False, waved


def wait_panel(game: Game, target: int, frames: int = 60) -> bool:
    for _ in range(frames):
        if wy(game) == target:
            return True
        game.wait(1)
    return wy(game) == target


# --- checks -----------------------------------------------------------------

def check_loaded(game: Game, k: int, p: dict) -> bool:
    n = p["n"]
    ok = check(game.byte("current") == k and game.byte("n") == n, f"puzzle {k + 1}: not loaded")
    cells = game.bytes("cell", n * n)
    want = [(0x90 | p["solution"][i]) if i in p["given"] else (0x80 if p["solution"][i] else 0)
            for i in range(n * n)]
    ok &= check(cells == want, f"puzzle {k + 1}: cells {cells} != {want}")
    for key in ("sum_a", "sum_d", "len_a", "len_d", "head_a", "head_d"):
        ok &= check(game.bytes(key, n * n) == p[key], f"puzzle {k + 1}: {key} differs")
    ok &= check(game.bytes("solution", n * n) == p["solution"], f"puzzle {k + 1}: solution differs")
    ok &= check(game.byte("empty") == sum(1 for i in range(n * n) if p["solution"][i] and i not in p["given"]),
                f"puzzle {k + 1}: empty count")
    return ok


def check_board(game: Game, k: int, p: dict, label: str, centred: bool = True) -> bool:
    n = p["n"]
    grid = expected_map(p, game.bytes("cell", n * n))
    got = [[game.reg(BG_MAP + 32 * y + x) for x in range(2 * n)] for y in range(2 * n)]
    ok = check(got == grid, f"puzzle {k + 1}: board tiles differ ({label})")
    if not centred:                          # the wave has moved it
        return ok
    # Centred in the 160x136 dots above the status line.
    px = n * 16
    ok &= check(game.reg(0xFF43) == -((160 - px) // 2) & 0xFF, f"puzzle {k + 1}: board not centred across ({label})")
    ok &= check(game.reg(0xFF42) == -((136 - px) // 2) & 0xFF, f"puzzle {k + 1}: board not centred down ({label})")
    return ok


def open_cells(game: Game) -> list[int]:
    n = game.byte("n")
    return [i for i in range(n * n) if game.byte("cell", i) & 0x80 and not game.byte("cell", i) & 0x10]


def solve(game: Game, k: int, p: dict, wrong_first: bool) -> None:
    cells = open_cells(game)
    for i in cells[:-1]:
        game.goto(i)
        game.enter(p["solution"][i])
    last = cells[-1]
    game.goto(last)
    if wrong_first:
        game.enter(p["solution"][last] % 9 + 1)
        game.wait(2)
        check(status_text(game) == "NOT YET RIGHT", f"puzzle {k + 1}: wrong filling not refused")
        check(wy(game) == PANEL_CLOSED, f"puzzle {k + 1}: wrong filling counted as solved")
    game.enter(p["solution"][last])
    shown, waved = wait_overview(game)
    check(waved, f"puzzle {k + 1}: no wave after solving")
    check(shown, f"puzzle {k + 1}: overview did not show")
    # src/panel.c: the title on its canvas (from tile 1) at row 2, the counts
    # (from T_BLACK + 1) from row 8, after the wave
    game.wait(30)
    check(game.reg(WIN_MAP + 32 * 2 + 1) == 1, f"puzzle {k + 1}: no title on the overview")
    check(game.reg(WIN_MAP + 32 * 8 + 1) == GFX["T_BLACK"] + 1, f"puzzle {k + 1}: no counts on the overview")
    check(game.reg(WIN_MAP) == GFX["T_BLACK"] and vram_matches(game, GFX["T_BLACK"], GFX["T_BLACK"]),
          f"puzzle {k + 1}: the overview's frame not black")


def combos(total: int, length: int) -> list[str]:
    return ["".join(map(str, c)) for c in combinations(range(1, 10), length) if sum(c) == total]


def check_panel(game: Game, p: dict) -> None:
    """The combinations panel for the cursor's runs."""
    cur, n = game.byte("cur"), p["n"]
    cells = game.bytes("cell", n * n)
    labels = []
    for arrow, key in (("[", "a"), ("\\", "d")):
        head = p["head_" + key][cur]
        labels.append(arrow + (" -  " if head == 0xFF else f"{p['sum_' + key][head]:2}/{p['len_' + key][head]}"))
    check(status_text(game) == " ".join(labels), f"status line {status_text(game)!r}, want {' '.join(labels)!r}")
    game.press("select", after=2)
    check(wait_panel(game, 0), "combinations panel did not open")
    game.wait(25)                                # drawn once it covers the board
    # src/panel.c: what the panel shows, per run (0 across, 1 down)
    possible = 0x3FE
    for r, key in ((0, "a"), (1, "d")):
        head = p["head_" + key][cur]
        count = game.byte("info_count", r)
        if head == 0xFF:
            check(count == 0xFF, f"panel: run {key} has no sum, but {count} combinations")
            continue
        total, length = p["sum_" + key][head], p["len_" + key][head]
        check((game.byte("info_sum", r), game.byte("info_len", r)) == (total, length),
              f"panel: run {key} {game.byte('info_sum', r)} in {game.byte('info_len', r)}, want {total} in {length}")
        step = 1 if r == 0 else n
        run = [head + step * (j + 1) for j in range(length)]
        placed = {cells[j] & 15 for j in run} - {0}
        others = {cells[j] & 15 for j in run if j != cur} - {0}
        want = combos(total, length)
        masks = [game.byte("info_masks", 2 * (r * 12 + k)) | game.byte("info_masks", 2 * (r * 12 + k) + 1) << 8
                 for k in range(count)]
        got = ["".join(str(d) for d in range(1, 10) if m >> d & 1) for m in masks]
        check(got == want, f"panel: run {key} combinations {got} != {want}")
        dim = game.byte("info_dim", 2 * r) | game.byte("info_dim", 2 * r + 1) << 8
        want_dim = [not placed <= set(map(int, c)) for c in want]
        check([bool(dim >> k & 1) for k in range(len(want))] == want_dim, f"panel: run {key} dim {dim:#x}")
        fits = 0
        for c in want:
            digits = set(map(int, c))
            if others <= digits:
                for d in digits - others:
                    fits |= 1 << d
        possible &= fits
        # drawn: the run's tiles in the window, from the board's (T_BLACK on)
        check(game.reg(WIN_MAP + 32 * 4) >= GFX["T_BLACK"], "panel: the runs not drawn")
    got = game.byte("info_fits") | game.byte("info_fits", 1) << 8
    want = 0 if cells[cur] & 0x10 else possible
    check(got == want, f"panel: possible digits {got:#x}, want {want:#x}")
    game.press("select", after=2)
    check(wait_panel(game, PANEL_CLOSED), "combinations panel did not close")
    check(vram_matches(game), "combinations panel: the font and board tiles not loaded back")


def feature_tests(game: Game, k: int, p: dict) -> None:
    n = p["n"]
    # A given digit cannot be changed: A does not open the keypad, B leaves it.
    given = sorted(p["given"])[0]
    game.goto(given)
    game.press("a", after=4)
    check(not game.pb.memory[0xFE00 + 4 * 4], "keypad opened on a given digit")
    game.press("b", after=4)
    check(game.byte("cell", given) == 0x90 | p["solution"][given], "given digit changed")
    check_panel(game, p)

    cells = open_cells(game)
    a, b = cells[0], cells[1]
    game.goto(a)
    check_panel(game, p)
    game.enter(p["solution"][a])
    check(game.byte("cell", a) == 0x80 | p["solution"][a], "digit not entered")
    check_panel(game, p)
    game.press("b", after=4)                     # B wipes
    check(game.byte("cell", a) == 0x80, "B did not wipe the cell")
    game.enter(3)
    game.enter(0)                                # the keypad's erase key
    check(game.byte("cell", a) == 0x80, "erase key did not wipe the cell")

    # Hint: START, down, A reveals the cell and costs 30 seconds.
    game.goto(b)
    before = game.word("clock_seconds")
    game.press("start", after=20)
    game.press("down", after=2)
    game.press("a", after=20)
    check(game.byte("cell", b) == 0x80 | p["solution"][b], "hint did not reveal the cell")
    check(game.word("clock_seconds") >= before + 30, "hint cost no time")
    check_board(game, k, p, "after the hint")

    # Start over: every entered digit gone, clock at 0.
    game.press("start", after=20)
    game.presses("down", 2, after=2)
    game.press("a", after=20)
    check(all(game.byte("cell", i) == 0x80 for i in cells), "start over left digits")
    check(game.word("clock_seconds") < 2, "start over kept the clock")
    check_board(game, k, p, "after starting over")
    del n


# src/best.c: a best time per puzzle, 2 bytes (little endian), 0 = not solved.
def best_time(game: Game, k: int) -> int:
    return game.byte("best_times", 2 * k) | game.byte("best_times", 2 * k + 1) << 8


def set_best(game: Game, k: int, seconds: int) -> None:
    game.poke("best_times", 2 * k, seconds & 0xFF)
    game.poke("best_times", 2 * k + 1, seconds >> 8)


def vram_matches(game: Game, first: int = 1, last: int = GFX["GAME_TILE_COUNT"] - 1) -> bool:
    """Whether background tiles first..last in VRAM are game_tiles' (the
    DMG's: on the Color the digit cells are its own)."""
    want = gfx_array("game_tiles")
    for t in range(first, last + 1):
        base = 0x9000 + 16 * t if t < 128 else 0x8800 + 16 * (t - 128)
        if [game.reg(base + i) for i in range(16)] != want[16 * t:16 * t + 16]:
            return False
    return True


def gfx_array(name: str) -> list[int]:
    """An array of src/gfx.c."""
    body = re.search(rf"{name}\[\d+\] = {{(.*?)}};", (ROOT / "src/gfx.c").read_text(), re.S).group(1)
    return [int(x, 16) for x in re.findall(r"0x([0-9A-F]{2})", body)]


def splash_test(rom: str) -> None:
    game = Game(rom)
    game.settle()
    game.wait(300)
    logo = gfx_array("frame_map")[:100]
    got = [game.reg(BG_MAP + 32 * y + x) for y in range(5) for x in range(20)]
    check(got == logo, "start screen: the logo not filled in")
    scene = gfx_array("scene_map")
    got = [game.reg(BG_MAP + 32 * y + x) for y in range(5, 5 + len(scene) // 20) for x in range(20)]
    check(got == scene, "start screen: the scene not there")
    game.to_screen("start")
    check(on_title(game), "start screen: START did not lead to the title's cards")
    current = game.byte("current")
    game.press("left", after=4)                  # 6x6 *, no puzzles yet
    game.press("a", after=30)
    check(on_title(game) and game.byte("current") == current, "title: a group without puzzles opened")
    game.stop()


def full_run(rom: str, model: list[dict]) -> None:
    game = Game(rom)
    game.boot()
    check(on_title(game), "title: not shown after the start screen")
    game.to_screen("a")                          # grid of 6x6 **
    game.to_screen("a")                          # puzzle 1
    started = time.time()
    for k, p in enumerate(model):
        if check_loaded(game, k, p):
            check_board(game, k, p, "at the start")
            if k == 0:
                feature_tests(game, k, p)
            solve(game, k, p, wrong_first=k % 10 == 0)
            check_board(game, k, p, "solved", centred=False)
        game.to_screen("a")                      # next puzzle
        if k % 25 == 24:
            print(f"  {k + 1} puzzles solved ({time.time() - started:.0f} s)")
    check(game.byte("current") == 0, "after the last puzzle: not back at puzzle 1")
    for k in range(len(model)):
        check(best_time(game, k) > 0, f"puzzle {k + 1}: no best time")
    game.stop()


def quit_test(rom: str, model: list[dict]) -> None:
    """Leaving a puzzle asks first, and it is not kept."""
    game = Game(rom)
    game.boot()
    for k in (0, 2):                             # puzzles 1 and 3 solved before
        set_best(game, k, 100)
    game.to_screen("a")                          # grid, at puzzle 1's group
    game.press("right", after=20)
    game.to_screen("a")                          # puzzle 2
    p = model[1]
    n = p["n"]
    fresh = game.bytes("cell", n * n)
    check(game.byte("current") == 1, "quit: puzzle 2 not started")
    for i in open_cells(game)[:3]:
        game.goto(i)
        game.enter(p["solution"][i])
    game.wait(200)
    state = game.bytes("cell", n * n)
    game.press("start", after=20)
    game.presses("down", 4, after=2)             # QUIT TO MENU
    game.press("a", after=20)
    check(map_text(game, WIN_MAP, 2)[2:].strip() == "QUIT THIS PUZZLE?", f"quit: no question, {map_text(game, WIN_MAP, 2)!r}")
    check(map_text(game, WIN_MAP, 6)[2:].strip() == "TIME ARE LOST.", "quit: the loss not told")
    game.press("a", after=20)                    # KEEP PLAYING: back to the pause menu
    check(map_text(game, WIN_MAP, 2)[2:].strip() == "RESUME", "quit: KEEP PLAYING did not return to the pause menu")
    game.press("b", after=20)
    check(game.bytes("cell", n * n) == state, "quit: KEEP PLAYING lost digits")
    game.press("start", after=20)
    game.presses("down", 3, after=2)             # MUSIC: off and on again
    for on in (False, True):
        game.press("a", after=4)
        label = "MUSIC: ON" if on else "MUSIC: OFF"
        check(map_text(game, WIN_MAP, 5)[2:].strip() == label, f"pause: {map_text(game, WIN_MAP, 5)!r}, want {label}")
    game.press("down", after=2)
    game.press("a", after=20)
    game.press("b", after=20)                    # B: keeps playing too
    check(map_text(game, WIN_MAP, 2)[2:].strip() == "RESUME", "quit: B did not return to the pause menu")
    game.press("a", after=20)
    game.press("down", after=4)                  # QUIT PUZZLE
    game.to_screen("a")
    game.wait(20)
    # Puzzles 1 and 3: light, with a check mark; puzzle 2 left: neither.
    check([grid_cell(game, j) for j in range(3)] == [(1, 2), (0, 0), (1, 2)],
          f"grid: marks {[grid_cell(game, j) for j in range(3)]}")
    game.to_screen("b")
    check(on_title(game), "quit: B from the grid did not lead to the title")
    game.to_screen("a")
    game.to_screen("a")                          # puzzle 2 again
    check(game.byte("current") == 1, "quit: not back at puzzle 2")
    check(game.bytes("cell", n * n) == fresh, "quit: digits kept after quitting")
    check(game.word("clock_seconds") <= 1, f"quit: clock kept, {game.word('clock_seconds')}")
    game.stop()


def header_test(rom: str) -> None:
    data = Path(rom).read_bytes()
    check(data[0x147] == 0x00, f"header: cartridge type {data[0x147]:#04x}, want 0x00 (ROM only)")
    check(data[0x149] == 0x00, f"header: RAM size {data[0x149]:#04x}, want 0x00")
    check(data[0x148] == 0x00 and len(data) == 32768, "header: not a 32 KiB ROM")
    check(data[0x143] == 0x80, "header: not marked for the Game Boy Color too")


def cgb_test(rom: str, model: list[dict]) -> None:
    game = Game(rom, cgb=True)
    game.boot()
    check(game.byte("cgb") == 1, "Color: not detected")
    game.to_screen("a")                          # grid
    game.to_screen("a")                          # puzzle 1
    p = model[0]
    n = p["n"]
    cells = game.bytes("cell", n * n)
    for i in range(n * n):
        # video.h: CPAL_CLUE 1, CPAL_CELL 2, CPAL_GIVEN 3
        want = 1 if not cells[i] & 0x80 else 3 if cells[i] & 0x10 else 2
        r, c = divmod(i, n)
        got = {game.pb.memory[1, 0x9800 + 32 * (2 * r + dy) + 2 * c + dx] for dy in (0, 1) for dx in (0, 1)}
        check(got == {want}, f"Color: cell {i} palettes {got}, want {want}")
    solve(game, 0, p, wrong_first=False)
    game.stop()


def main() -> int:
    rom = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build/kakuro.gb")
    print("model")
    model = load_model()
    print("start screen")
    splash_test(rom)
    print(f"full run: {len(model)} puzzles")
    full_run(rom, model)
    print("quit")
    quit_test(rom, model)
    print("cartridge header")
    header_test(rom)
    print("Game Boy Color")
    cgb_test(rom, model)
    if failures:
        print(f"{len(failures)} failure(s)")
        return 1
    print("all tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
