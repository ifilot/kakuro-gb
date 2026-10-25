# Changelog

All notable changes to Kakuro for the Game Boy are listed here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [semantic versioning](https://semver.org/).

## [1.0.0] - 2026-10-25

### Added

- Kakuro for the Game Boy in a 32 KiB ROM-only cartridge, in colour on the
  Game Boy Color (paper and ink: navy lines on cream, terracotta sums,
  entered digits in blue ink, given ones on sand) and in shades on the
  others.
- 80 puzzles from kakuro-8bit, 6x6 to 8x8, in groups of size and
  difficulty; where a puzzle had more than one solution, givens from its
  stored solution were added until it has exactly one. 6x6 with one star is
  a group without puzzles yet.
- A start screen: the logo filled in a letter at a time over a zen garden
  seen from above, PRESS START; a border round it and the title.
- The title: the groups as six cards under the logo, each with its size,
  its difficulty out of three stars and a bar of the puzzles solved. A
  group opens its puzzles as cells (solved ones with a check mark) beside
  the chosen puzzle's preview, best time and state.
- The board of 16x16-dot cells with 3x5-dot sums; digit entry with a sprite
  keypad; the combinations of the cursor's runs and the digits the cell can
  take; a pause menu with a hint (+30 seconds), starting over, the music
  on or off and quitting.
- A solved puzzle leads on a wave, line by line, to an overview: the time,
  the best time and the number solved, in all and per group.
- Background music, a tune and a bass on channels 2 and 3, and sound
  effects.
- Nothing is saved (no battery RAM): best times last until switching off,
  and a puzzle left unsolved is not kept, so quitting asks first.
- A website to play it in the browser, in binjgb, as a Game Boy or a Game
  Boy Color, with the keyboard, on-screen buttons or a gamepad.
- `make test`: every puzzle solved in PyBoy through the buttons, with the
  puzzle in memory and the board on screen checked against the puzzle
  files, and the menus, quitting and the Color's palettes checked.
