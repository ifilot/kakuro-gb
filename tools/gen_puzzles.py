#!/usr/bin/env python3
"""Generate src/puzzles.h and src/puzzles.c from assets/puzzles/*.puz.

A .puz file (from kakuro-8bit, assets/puzzles/e5) is a header line
"# B5/P<n>/<stars>*" and a square grid of whitespace-separated cells: 0 is a
black cell, 1-9 the solution digit of a white cell, and a leading # marks a
digit that is given at the start. Row 0 and column 0 are always black; every
sum sits in a black cell to the left of or above its run. A run of a single
white cell has no sum in that direction.

Every puzzle is checked (rules, and exactly one solution with its givens);
should one have more solutions, givens are added from the stored solution
until only one remains, and reported on stdout.

The puzzles are ordered by size, then by difficulty (stars), then by their
original number, and numbered 1..N in that order. Consecutive puzzles of one
size and difficulty form a group, a line in the puzzle menu.

Encoding, per puzzle at puzzle_data + puzzle_offset[k]: the inner
(n-1)x(n-1) grid (rows and columns 1..n-1) as nibbles, high nibble first, 0
black and 1-9 the solution digit, padded to a whole byte; then the number of
givens and their indices in that inner grid. The grid size n and the stars
come from the group.

  python3 tools/gen_puzzles.py
"""
from __future__ import annotations

import re
import sys
from itertools import combinations
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "assets/puzzles"
OUTPUT = ROOT / "src/puzzles.h"
LIMIT = 400                       # solutions enumerated per round of adding givens

# combos[(k, s)]: bit masks (bit d = digit d) of k distinct digits 1-9 summing to s
COMBOS: dict[tuple[int, int], list[int]] = {}
for k in range(1, 10):
    for digits in combinations(range(1, 10), k):
        COMBOS.setdefault((k, sum(digits)), []).append(sum(1 << d for d in digits))


class Puzzle:
    def __init__(self, path: Path):
        lines = path.read_text().splitlines()
        header = lines[0] if lines and lines[0].startswith("#") else ""
        m = re.search(r"P(\d+)/(\d)\*", header)
        self.name = path.stem
        self.number = int(m.group(1)) if m else 0
        self.stars = int(m.group(2)) if m else 0
        rows = [line.split() for line in lines if line.strip() and not line.startswith("#")]
        self.n = len(rows)
        if not 6 <= self.n <= 10 or any(len(r) != self.n for r in rows):
            raise ValueError("not a square grid of 6 to 10")
        self.solution = [int(c.lstrip("#")) for r in rows for c in r]
        self.given = {i for i, c in enumerate(c for r in rows for c in r) if c.startswith("#")}
        self.runs = self._runs()

    def _runs(self) -> list[list[int]]:
        """Runs of two or more white cells, across then down, as cell indices."""
        n, sol, runs = self.n, self.solution, []
        for step, starts in ((1, [r * n for r in range(n)]), (n, list(range(n)))):
            for first in starts:
                run = []
                for k in range(n + 1):
                    i = first + k * step
                    if k < n and sol[i]:
                        run.append(i)
                        continue
                    if len(run) >= 2:
                        runs.append(run)
                    run = []
        return runs

    def check(self) -> str | None:
        n, sol = self.n, self.solution
        if any(sol[i] for i in range(n)) or any(sol[r * n] for r in range(n)):
            return "white cell in the sum row or column"
        if any(not 0 <= v <= 9 for v in sol):
            return "cell value out of range"
        covered = {i for run in self.runs for i in run}
        if any(v and i not in covered for i, v in enumerate(sol)):
            return "white cell without a sum"
        for run in self.runs:
            digits = [sol[i] for i in run]
            if len(set(digits)) != len(digits):
                return f"digit twice in a run: {digits}"
        return None

    def solve(self, given: set[int], limit: int) -> list[list[int]]:
        """Up to `limit` solutions with the digits in `given` fixed."""
        sol = self.solution
        cell_runs: dict[int, list[int]] = {}
        for r, run in enumerate(self.runs):
            for i in run:
                cell_runs.setdefault(i, []).append(r)
        target = [sum(sol[i] for i in run) for run in self.runs]
        value = [sol[i] if i in given else 0 for i in range(len(sol))]
        free = [i for i in cell_runs if not value[i]]
        found: list[list[int]] = []

        def allowed(i: int) -> int:
            mask = 0x3FE
            for r in cell_runs[i]:
                used = total = left = 0
                for j in self.runs[r]:
                    if value[j]:
                        used |= 1 << value[j]
                        total += value[j]
                    else:
                        left += 1
                ok = 0
                for m in COMBOS.get((left, target[r] - total), []):
                    if not m & used:
                        ok |= m
                mask &= ok
            return mask

        def search() -> None:
            if len(found) >= limit:
                return
            best, best_mask, best_count = None, 0, 10
            for i in free:
                if value[i]:
                    continue
                mask = allowed(i)
                count = bin(mask).count("1")
                if count < best_count:
                    best, best_mask, best_count = i, mask, count
                    if count <= 1:
                        break
            if best is None:
                found.append(value[:])
                return
            for d in range(1, 10):
                if best_mask & (1 << d):
                    value[best] = d
                    search()
                    value[best] = 0

        search()
        return found

    def make_unique(self) -> list[int]:
        """Adds givens until the solution is unique; returns the cells added."""
        given, added = set(self.given), []
        while True:
            solutions = self.solve(given, LIMIT)
            if self.solution not in solutions:
                raise ValueError("the stored solution does not satisfy the sums")
            if len(solutions) == 1:
                return added
            candidates = [i for i, v in enumerate(self.solution) if v and i not in given]
            cell = min(candidates, key=lambda i: sum(s[i] == self.solution[i] for s in solutions))
            given.add(cell)
            added.append(cell)
            self.given = given

    def encode(self) -> bytes:
        n = self.n
        inner = [(r, c) for r in range(1, n) for c in range(1, n)]
        nibbles = [self.solution[r * n + c] for r, c in inner]
        if len(nibbles) % 2:
            nibbles.append(0)
        data = bytearray((nibbles[i] << 4) | nibbles[i + 1] for i in range(0, len(nibbles), 2))
        givens = sorted(k for k, (r, c) in enumerate(inner) if r * n + c in self.given)
        data.append(len(givens))
        data += bytes(givens)
        return bytes(data)


def c_bytes(data, indent="    ", per_line=16):
    items = [f"0x{b:02X}" for b in data]
    return ",\n".join(indent + ", ".join(items[i:i + per_line]) for i in range(0, len(items), per_line))


def main() -> int:
    puzzles = []
    for path in sorted(SOURCE.glob("*.puz")):
        try:
            p = Puzzle(path)
        except ValueError as e:
            print(f"{path.name}: skipped ({e})")
            continue
        problem = p.check()
        if problem:
            print(f"{path.name}: skipped ({problem})")
            continue
        added = p.make_unique()
        if added:
            names = ", ".join(f"r{i // p.n}c{i % p.n}={p.solution[i]}" for i in added)
            print(f"{path.name}: {len(added)} given(s) added ({names})")
        puzzles.append(p)
    puzzles.sort(key=lambda p: (p.n, p.stars, p.number))

    groups: list[list[int]] = []                       # [n, stars, first, count]
    for k, p in enumerate(puzzles):
        if groups and groups[-1][0] == p.n and groups[-1][1] == p.stars:
            groups[-1][3] += 1
        else:
            groups.append([p.n, p.stars, k, 1])

    data, offsets = bytearray(), []
    for p in puzzles:
        offsets.append(len(data))
        data += p.encode()

    header = ["/* Generated by tools/gen_puzzles.py from assets/puzzles -- do not edit. */",
              "#ifndef PUZZLES_H", "#define PUZZLES_H", "",
              f"#define PUZZLE_COUNT {len(puzzles)}",
              f"#define GROUP_COUNT {len(groups)}", "",
              "/* Groups of one size and difficulty: grid size (with the sum row and",
              " * column), stars, first puzzle, count. */",
              "extern const unsigned char group_n[GROUP_COUNT], group_stars[GROUP_COUNT];",
              "extern const unsigned char group_first[GROUP_COUNT], group_count[GROUP_COUNT];", "",
              "/* Puzzle k at puzzle_data + puzzle_offset[k]: the inner (n-1)x(n-1) grid as",
              " * nibbles (0 black, 1-9 the solution digit) padded to a byte, the number of",
              " * givens, and their indices in the inner grid. */",
              "extern const unsigned int puzzle_offset[PUZZLE_COUNT];",
              "extern const unsigned char puzzle_data[];", "", "#endif", ""]
    source = ["/* Generated by tools/gen_puzzles.py from assets/puzzles -- do not edit.",
              " * Source files, in order: " + " ".join(p.name for p in puzzles) + " */",
              '#include "puzzles.h"', ""]
    for name, column in (("group_n", 0), ("group_stars", 1), ("group_first", 2), ("group_count", 3)):
        source.append(f"const unsigned char {name}[GROUP_COUNT] = {{ " + ", ".join(str(g[column]) for g in groups) + " };")
    source += ["", "const unsigned int puzzle_offset[PUZZLE_COUNT] = {",
               ",\n".join("    " + ", ".join(str(o) for o in offsets[i:i + 12]) for i in range(0, len(offsets), 12)),
               "};"]
    source += ["", f"const unsigned char puzzle_data[{len(data)}] = {{", c_bytes(data), "};", ""]
    OUTPUT.write_text("\n".join(header))
    OUTPUT.with_suffix(".c").write_text("\n".join(source))
    print(f"wrote {OUTPUT.relative_to(ROOT)} and .c: {len(puzzles)} puzzles in {len(groups)} groups, {len(data)} bytes")
    for n, stars, first, count in groups:
        print(f"  {n}x{n} {'*' * stars:5} puzzles {first + 1}-{first + count}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
