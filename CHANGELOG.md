# Changelog

All notable changes to Kakuro for the Game Boy are listed here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [semantic versioning](https://semver.org/).

## [1.0.0] - 2026-10-24

### Added

- Kakuro for the original Game Boy (DMG), in a 32 KiB ROM with 8 KiB of
  battery-backed RAM (MBC1).
- 150 puzzles from kakuro-8bit, 6x6 to 10x10 in 11 groups of size and
  difficulty. Where a puzzle had more than one solution, givens from its
  stored solution were added until it has exactly one.
- Board of 16x16-dot cells with 3x5-dot sums; the sum row and column show
  only their halves with the sums, so grids up to 9x9 fit the screen and a
  10x10 grid scrolls up and down by one cell with the cursor.
- Digit entry with a sprite keypad, combinations of the cursor's runs (dim
  when they lack a digit already in the run) and the digits the cell can
  take, a pause menu with a hint (+30 seconds) and starting over.
- Progress and clock of every puzzle and best times kept in battery RAM;
  CONTINUE on the title screen returns to the last puzzle.
- `make test`: every puzzle solved in PyBoy through the buttons, with the
  puzzle in memory and the board on screen checked against the puzzle files.
