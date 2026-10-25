# Kakuro for the Game Boy: C with GBDK-2020.
#
# GBDK_HOME points at an unpacked GBDK-2020 release (4.5.0 or later), see
# README.md. make run uses mGBA (MGBA names its binary). The tests and
# screenshots use PyBoy (pip install pyboy pillow); PYTHON may name a virtual
# environment's interpreter that has it.

GBDK_HOME ?= $(HOME)/gbdk
LCC       = $(GBDK_HOME)/bin/lcc
PYTHON   ?= python3
MGBA     ?= mgba-qt

# ROM only, 32 KiB, without battery RAM (the best times last until switching
# off); in colour on the Game Boy Color, in shades on the others (-yc).
# Not --max-allocs-per-node: with 50000, SDCC 4.5 kept adding to a register
# it had already changed (q[3] = base + 3 became base + 5 in board.c).
LCCFLAGS = -Wl-m -Wl-j -Wm-yn"KAKURO" -Wm-yt0x00 -Wm-yc

SOURCES = src/main.c src/game.c src/board.c src/panel.c src/menu.c src/puzzle.c src/best.c \
          src/clock.c src/input.c src/sound.c src/video.c src/wave.c src/canvas.c src/puzzles.c src/gfx.c \
          src/music.c
HEADERS = $(wildcard src/*.h)
OBJECTS = $(patsubst src/%.c,build/obj/%.o,$(SOURCES))
ROM     = build/kakuro.gb

.PHONY: all run puzzles gfx music listen test screenshots docs site serve clean

all: $(ROM)

build/obj/%.o: src/%.c $(HEADERS) Makefile
	@mkdir -p build/obj
	$(LCC) $(LCCFLAGS) -c -o $@ $<

$(ROM): $(OBJECTS)
	$(LCC) $(LCCFLAGS) -o $@ $(OBJECTS)
	@$(PYTHON) tools/romusage.py $@ build/kakuro.map

# Play it in mGBA (arrows, A = x, B = z, START = Return, SELECT = Backspace).
run: $(ROM)
	$(MGBA) $(ROM)

# Regenerate src/puzzles.[ch] from assets/puzzles (checks every puzzle and
# adds givens until each has one solution).
puzzles:
	$(PYTHON) tools/gen_puzzles.py

# Regenerate src/gfx.[ch]; the tile sheet goes to build/gfx_preview.png.
gfx:
	$(PYTHON) tools/gen_gfx.py --preview

# Regenerate src/music.[ch] from the notes in tools/gen_music.py.
music:
	$(PYTHON) tools/gen_music.py

# The music as the emulator plays it -> build/music.wav (one time round).
listen: $(ROM)
	$(PYTHON) tools/record_music.py $(ROM) build/music.wav

# Every puzzle loaded and solved in the emulator, with the menus.
test: $(ROM)
	$(PYTHON) tools/test_game.py $(ROM)

# Screenshots of the screens -> build/screens/*.png; docs: the README's
# pictures in Game Boy green -> docs/*.png
screenshots: $(ROM)
	$(PYTHON) tools/screenshots.py $(ROM)

docs: $(ROM)
	$(PYTHON) tools/screenshots.py $(ROM) --docs

# The website, the game in binjgb in the browser -> _site/ (checked);
# serve: at http://localhost:8000
site: $(ROM)
	$(PYTHON) tools/build_site.py --check

serve: site
	$(PYTHON) -m http.server 8000 --directory _site

clean:
	rm -rf build _site
