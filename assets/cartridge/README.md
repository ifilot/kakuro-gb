# Kakuro cartridge sticker

The [five revised zen-garden designs](zen-v2/README.md) follow the selected
illustrated style, omit the puzzle count and FRONT, and contain a verified
Kakuro board. Rebuild those with `make sticker-zen`.

`kakuro-sticker.png` is the finished label, built with code rather than generated
image text. Its cream, navy and terracotta palette follows `web/style.css`; the
garden echoes the opening screen. The board is a partially filled version of
`assets/puzzles/001.puz`, with sums calculated from its solution.

The side strip identifies the platform as **GAME BOY**. **FRONT** identifies the
placement; **NUMBER CROSSWORD** and **LOGIC PUZZLE** identify the game type.
**80 PUZZLES**, **GB + GBC** and **COLOR ENHANCED** describe the actual game.

## Files and printing

- `kakuro-sticker.svg`: editable vector label, 42 × 37 mm, 1.5 mm corner radius.
- `kakuro-sticker.png`: transparent rounded corners, 600 dpi raster export.
- `kakuro-sticker-bleed.svg` / `.pdf`: 46 × 41 mm artwork including 2 mm bleed
  on every edge; trim to 42 × 37 mm, then round the corners.
- `kakuro-sticker-print-a4.svg` / `.pdf`: an A4 sheet with bleed, crop marks,
  printing instructions and a 50 mm scale check. Print at **100% / Actual size**.
- `concepts/`: five imagegen concept images and the exact generation prompts.

The standard dimensions follow the
[ConsoleMods cartridge label table](https://consolemods.org/wiki/Cart_Labels).
Check the recess on your particular shell before cutting adhesive stock. Start
with a paper proof and verify the 50 mm scale check. These are RGB exports;
a print shop can convert to its preferred color profile.

## Rebuild

Run `make sticker`, or `python3 tools/build_sticker.py --out-dir PATH`.
Requires Python 3 with Pillow, DejaVu Sans and `rsvg-convert` (Debian/Ubuntu package
`librsvg2-bin`). The SVG remains editable text; the PDF embeds the rendered font.
The imagegen examples are independent design explorations; regeneration of the
production label does not call an image API or depend on those examples.
