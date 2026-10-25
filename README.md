# Kakuro for the Game Boy

[![Version](https://img.shields.io/badge/version-1.0.0-blue)](CHANGELOG.md)
[![License: GPL v3](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)

Kakuro, the number crossword, for the original Game Boy. There are 150
puzzles, from 6x6 to 10x10, in a 32 KiB ROM; the progress of every puzzle
and its best time are kept in the cartridge's battery-backed RAM.

<p align="center">
  <img src="docs/title.png" alt="Title screen" width="32%">
  <img src="docs/grid.png" alt="Choosing a puzzle" width="32%">
  <img src="docs/playing.png" alt="A 9x9 puzzle in progress" width="32%">
</p>
<p align="center">
  <img src="docs/keypad.png" alt="Entering a digit with the keypad" width="32%">
  <img src="docs/combinations.png" alt="The combinations of the cursor's runs" width="32%">
  <img src="docs/solved.png" alt="A solved puzzle with a new best time" width="32%">
</p>

## Play

Load `build/kakuro.gb` in an emulator (mGBA, SameBoy, BGB, ...) or put it on
a flash cartridge. It is an MBC1 cartridge with 8 KiB of battery RAM.

Fill every white cell with a digit from 1 to 9. The digits of a run of
white cells, across or down, add up to the sum in the black cell before it:
the sum in the upper-right triangle belongs to the run to its right, the one
in the lower-left triangle to the run below it. No digit occurs twice in a
run. Digits on a grey cell are given and cannot be changed. The puzzle is
solved when every cell is filled and every run adds up; every puzzle has
exactly one solution.

| Button | On the board |
| --- | --- |
| D-pad | Move the cursor to the nearest white cell in that direction |
| A | Open the keypad: D-pad to choose 1-9 or the erase key (x), A to put it, B to cancel |
| B | Wipe the cell |
| SELECT | Combinations of the cursor's runs (SELECT or B closes) |
| START | Pause menu: resume, reveal the cell (+30 seconds), start over, quit to the menu |

The status line under the board shows the sum and length of the cursor's
run across (`→20/4`) and down (`↓16/2`) and the clock. The combinations
panel lists every set of different digits that makes each sum; a set that
lacks a digit already in the run is dim. Below them are the digits the cell
can still take, given both runs. When the last cell is filled but a run
does not add up, the status line says `NOT YET RIGHT`.

The title screen offers CONTINUE when the last puzzle played is in
progress. CHOOSE PUZZLE lists the groups of one size and difficulty with
the number solved, then the puzzles of a group with a preview, the best
time and whether it is solved (✓) or in progress (■). Every entry is saved
at once and the clock every second, so switching off loses nothing.

## Puzzles

The puzzles come from [kakuro-8bit](https://github.com/ifilot/kakuro-8bit)
(`assets/puzzles/e5`). That project also forbids two runs from using the
same set of digits; with the standard rules alone, 64 of them have more
than one solution. `tools/gen_puzzles.py` adds givens from the stored
solution (1 to 3 per puzzle) until each has exactly one.

| Puzzles | Grid (with the sums) | Difficulty |
| --- | --- | --- |
| 1-10 | 6 x 6 | ★★ |
| 11-30 | 7 x 7 | ★★ |
| 31-40 | 7 x 7 | ★★★ |
| 41-60 | 8 x 8 | ★★ |
| 61-80 | 8 x 8 | ★★★ |
| 81-100 | 9 x 9 | ★★ |
| 101-121 | 9 x 9 | ★★★ |
| 122-126 | 10 x 10 | ★★ |
| 127-145 | 10 x 10 | ★★★ |
| 146-148 | 10 x 10 | ★★★★ |
| 149-150 | 10 x 10 | ★★★★★ |

A cell is 16x16 dots. The sum row and column show only the halves that hold
sums, so an n x n grid is (2n-1) tiles square: up to 9x9 fits the 160x136
dots above the status line, and a 10x10 grid (152 dots) scrolls by one cell
with the cursor.

## Build

The game is C for [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020)
(4.5.0). Nothing needs root: unpack the Linux release in your home
directory, which is where the Makefile looks by default (`GBDK_HOME`).

```sh
curl -sL https://github.com/gbdk-2020/gbdk-2020/releases/download/4.5.0/gbdk-linux64.tar.gz | tar xz -C ~
make                  # -> build/kakuro.gb, and the ROM space used
```

The tests, screenshots and `make run` use [PyBoy](https://github.com/Baekalfen/PyBoy):

```sh
python3 -m venv ~/.venvs/pyboy && ~/.venvs/pyboy/bin/pip install pyboy pillow
make test PYTHON=~/.venvs/pyboy/bin/python
```

| Target | |
| --- | --- |
| `make` | Build `build/kakuro.gb` |
| `make run` | Play in PyBoy (arrows; A = `a`, B = `s`, START = Return, SELECT = Backspace) |
| `make test` | Every puzzle solved in the emulator, see below |
| `make screenshots` | Every screen -> `build/screens/` |
| `make docs` | The pictures above, in Game Boy green -> `docs/` |
| `make puzzles` | Regenerate `src/puzzles.[ch]` from `assets/puzzles` |
| `make gfx` | Regenerate `src/gfx.[ch]`; tile sheet in `build/gfx_preview.png` |

`make test` plays all 150 puzzles in order with the D-pad and the keypad,
as a player would. For each it checks the puzzle in memory (cells, givens,
sums, runs) against `src/puzzles.c` and against the `.puz` files, every
tile of the board on screen, every cursor step against a model of the
cursor rule, and that a wrong filling is refused (every tenth puzzle). It
also checks the combinations panel, wiping cells, given digits being
locked, the hint and starting over, progress and clock surviving a power
cycle through CONTINUE, the marks on the puzzle grid, the best times in
battery RAM, and battery RAM full of garbage at the first start.

With the 150 puzzles (5.3 KiB) the ROM uses about 24 KiB of its 32 KiB.

## Layout

| File | Contents |
| --- | --- |
| `src/main.c` | Start-up and the screens |
| `src/game.c` | Playing a puzzle: buttons, cursor movement, entering digits, solved, hint, start over |
| `src/puzzle.c` | Loading a puzzle, its sums and runs, the solved test, combinations of a sum |
| `src/board.c` | The board on the background layer, scrolling, the cursor and keypad sprites |
| `src/panel.c` | The window layer: status line, combinations, pause menu, solved |
| `src/menu.c` | Title, groups, puzzle grid with preview, rules |
| `src/save.c` | Battery RAM: best times, progress per puzzle, the last puzzle |
| `src/clock.c` | The game clock from the timer interrupt (16 Hz) |
| `src/input.c` | Buttons, with repeat for the D-pad |
| `src/sound.c` | Sound effects and the jingle for a solved puzzle |
| `src/video.c` | Tile loading, text, number formatting, fades |
| `src/puzzles.c`, `src/gfx.c` | Generated puzzle data and tiles |
| `assets/puzzles/` | The puzzle files from kakuro-8bit |
| `tools/` | Puzzle and tile generators, PyBoy harness, tests, screenshots |

## License

GNU General Public License v3.0; see [LICENSE](LICENSE).
