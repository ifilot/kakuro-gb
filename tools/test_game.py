#!/usr/bin/env python3
"""Plays the ROM in PyBoy the way a player would and checks the game.

1. Every puzzle, in order: the puzzle in memory (cells, givens, sums, runs)
   against src/puzzles.c and against the .puz files themselves, the board
   on screen tile by tile, and then the puzzle solved with the D-pad and the
   keypad. Every tenth puzzle first gets a wrong last digit, which must be
   refused ("NOT YET RIGHT"). After the last puzzle every best time must be
   in the battery RAM.
2. On the first puzzle: given digits cannot be changed, B and the keypad's
   erase key wipe a cell, the combinations panel against a Python model,
   the hint, and starting over.
3. Progress survives switching off: digits and clock come back through
   CONTINUE, and the grid screen marks solved and started puzzles.
4. Battery RAM holding garbage is cleared at start-up.

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
from gbharness import Game  # noqa: E402
import gen_puzzles  # noqa: E402

WIN_MAP, BG_MAP = 0x9C00, 0x9800
PANEL_CLOSED, PANEL_SOLVED = 136, 80
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
    """The (2n-1)^2 background tiles of the board, as board.c draws them."""
    n = p["n"]
    size = 2 * n - 1
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
            y, x = 2 * r - 1 + dy, 2 * c - 1 + dx
            if x >= 0 and y >= 0:
                grid[y][x] = q[qi]
    return grid


# --- reading the screen -----------------------------------------------------

def map_text(game: Game, base: int, row: int) -> str:
    """A map row as text; dim digits come out as lower-case a-i."""
    out = []
    for x in range(20):
        t = game.reg(base + 32 * row + x)
        if t < 64:
            out.append(chr(t + 32))
        elif t < 73:
            out.append(chr(ord("a") + t - 64))
        else:
            out.append("?")
    return "".join(out)


def wy(game: Game) -> int:
    return game.reg(0xFF4A)


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


def check_board(game: Game, k: int, p: dict, label: str) -> bool:
    n = p["n"]
    grid = expected_map(p, game.bytes("cell", n * n))
    got = [[game.reg(BG_MAP + 32 * y + x) for x in range(2 * n - 1)] for y in range(2 * n - 1)]
    return check(got == grid, f"puzzle {k + 1}: board tiles differ ({label})")


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
        check(map_text(game, WIN_MAP, 0).startswith("NOT YET RIGHT"), f"puzzle {k + 1}: wrong filling not refused")
        check(wy(game) == PANEL_CLOSED, f"puzzle {k + 1}: wrong filling counted as solved")
    game.enter(p["solution"][last])
    check(wait_panel(game, PANEL_SOLVED), f"puzzle {k + 1}: solved panel did not show")
    check(map_text(game, WIN_MAP, 2).strip() == "SOLVED!", f"puzzle {k + 1}: no SOLVED! text")


def combos(total: int, length: int) -> list[str]:
    return ["".join(map(str, c)) for c in combinations(range(1, 10), length) if sum(c) == total]


def check_panel(game: Game, p: dict) -> None:
    """The combinations panel for the cursor's runs."""
    cur, n = game.byte("cur"), p["n"]
    cells = game.bytes("cell", n * n)
    game.press("select", after=2)
    check(wait_panel(game, 0), "combinations panel did not open")
    rows = [map_text(game, WIN_MAP, y) for y in range(18)]
    text = "\n".join(rows)
    y = 4
    possible = set(range(1, 10))
    for across, arrow in ((True, "["), (False, "\\")):
        head = p["head_a" if across else "head_d"][cur]
        if head == 0xFF:
            check(rows[y].startswith(arrow + " NO SUM"), f"panel: no-sum line at row {y}: {rows[y]!r}")
            y += 2
            continue
        total = p["sum_a" if across else "sum_d"][head]
        length = p["len_a" if across else "len_d"][head]
        check(rows[y].strip() == f"{arrow} {total} IN {length}", f"panel: heading {rows[y]!r}")
        step = 1 if across else n
        run = [head + step * (j + 1) for j in range(length)]
        placed = {cells[j] & 15 for j in run} - {0}
        others = {cells[j] & 15 for j in run if j != cur} - {0}
        want, fits = [], set()
        for c in combos(total, length):
            digits = set(map(int, c))
            dim = not placed <= digits
            want.append("".join(chr(ord("a") + int(d) - 1) for d in c) if dim else c)
            if others <= digits:
                fits |= digits - others
        possible &= fits
        got = []
        y += 1
        while rows[y].strip():
            got += rows[y].split()
            y += 1
        check(got == want, f"panel: combinations {got} != {want}")
        y += 1
    if cells[cur] & 0x10:
        check(rows[16].startswith("GIVEN DIGIT"), f"panel: {rows[16]!r}")
    else:
        check(rows[16].strip() == "POSSIBLE: " + ("".join(map(str, sorted(possible))) or "NONE"),
              f"panel: {rows[16]!r}, want {sorted(possible)}")
    game.press("select", after=2)
    check(wait_panel(game, PANEL_CLOSED), "combinations panel did not close")
    del text


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


def full_run(rom: str, model: list[dict]) -> bytes:
    game = Game(rom)
    game.settle()
    check(map_text(game, BG_MAP, 10).strip() == "> CHOOSE PUZZLE", "title: CONTINUE offered on a new cartridge")
    game.to_screen("a")                          # groups
    game.to_screen("a")                          # grid of 6x6
    game.to_screen("a")                          # puzzle 1
    started = time.time()
    for k, p in enumerate(model):
        if check_loaded(game, k, p):
            check_board(game, k, p, "at the start")
            if k == 0:
                feature_tests(game, k, p)
            solve(game, k, p, wrong_first=k % 10 == 0)
            check_board(game, k, p, "solved")
        game.to_screen("a")                      # next puzzle
        if k % 25 == 24:
            print(f"  {k + 1} puzzles solved ({time.time() - started:.0f} s)")
    check(game.byte("current") == 0, "after the last puzzle: not back at puzzle 1")
    ram = game.ram()
    check(ram[:7] == b"KAKURO\x01", "battery RAM: no signature")
    for k in range(len(model)):
        best = ram[0x10 + 2 * k] | ram[0x11 + 2 * k] << 8
        check(best > 0, f"puzzle {k + 1}: no best time saved")
        check(ram[0x200 + 44 * k] == 0, f"puzzle {k + 1}: still marked as started")
    return ram


def resume_test(rom: str, ram: bytes, model: list[dict]) -> None:
    game = Game(rom, ram)
    game.settle()
    game.to_screen("a")                          # groups, at puzzle 1's group
    game.to_screen("a")                          # grid
    game.press("right", after=20)
    game.to_screen("a")                          # puzzle 2
    p = model[1]
    check(game.byte("current") == 1, "resume: puzzle 2 not started")
    cells = open_cells(game)[:3]
    for i in cells:
        game.goto(i)
        game.enter(p["solution"][i])
    game.wait(200)
    seconds = game.word("clock_seconds")
    state = game.bytes("cell", p["n"] ** 2)
    ram = game.ram()                             # switched off while playing

    game = Game(rom, ram)
    game.settle()
    check(map_text(game, BG_MAP, 10).strip() == "> CONTINUE", "resume: no CONTINUE on the title screen")
    game.to_screen("a")
    check(game.byte("current") == 1, "resume: CONTINUE opened another puzzle")
    check(game.bytes("cell", p["n"] ** 2) == state, "resume: digits not restored")
    check(abs(game.word("clock_seconds") - seconds) <= 1, f"resume: clock {game.word('clock_seconds')} != {seconds}")
    check_board(game, 1, {**p}, "resumed")
    game.press("start", after=20)
    game.presses("down", 3, after=2)
    game.to_screen("a")                          # quit to the grid
    game.wait(20)
    check(map_text(game, BG_MAP, 3)[1:5] == "  1]" and map_text(game, BG_MAP, 3)[6:10] == "  2_",
          f"grid: marks {map_text(game, BG_MAP, 3)!r}")
    game.stop()


def garbage_test(rom: str) -> None:
    game = Game(rom, bytes((k * 37 + 11) & 0xFF for k in range(8192)))
    game.settle()
    check(map_text(game, BG_MAP, 10).strip() == "> CHOOSE PUZZLE", "garbage RAM: CONTINUE offered")
    ram = game.ram()
    check(ram[:7] == b"KAKURO\x01" and not any(ram[0x10:0x10 + 300]), "garbage RAM: not cleared")


def main() -> int:
    rom = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build/kakuro.gb")
    print("model")
    model = load_model()
    print(f"full run: {len(model)} puzzles")
    ram = full_run(rom, model)
    print("resume")
    resume_test(rom, ram, model)
    print("garbage in battery RAM")
    garbage_test(rom)
    if failures:
        print(f"{len(failures)} failure(s)")
        return 1
    print("all tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
