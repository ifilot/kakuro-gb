# Kakuro for the Game Boy: C with GBDK-2020.
#
# GBDK_HOME points at an unpacked GBDK-2020 release (4.5.0 or later), see
# README.md. The tests and screenshots use PyBoy (pip install pyboy pillow);
# PYTHON may name a virtual environment's interpreter that has it.

GBDK_HOME ?= $(HOME)/gbdk
LCC       = $(GBDK_HOME)/bin/lcc
PYTHON   ?= python3

# MBC1 with 8 KiB battery-backed RAM (best times and progress), 32 KiB ROM.
# Not --max-allocs-per-node: with 50000, SDCC 4.5 kept adding to a register
# it had already changed (q[3] = base + 3 became base + 5 in board.c).
LCCFLAGS = -Wl-m -Wl-j -Wm-yn"KAKURO" -Wm-yt0x03 -Wm-ya1

SOURCES = src/main.c src/game.c src/board.c src/panel.c src/menu.c src/puzzle.c src/save.c \
          src/clock.c src/input.c src/sound.c src/video.c src/puzzles.c src/gfx.c
HEADERS = $(wildcard src/*.h)
OBJECTS = $(patsubst src/%.c,build/obj/%.o,$(SOURCES))
ROM     = build/kakuro.gb

.PHONY: all run puzzles gfx test screenshots docs clean

all: $(ROM)

build/obj/%.o: src/%.c $(HEADERS) Makefile
	@mkdir -p build/obj
	$(LCC) $(LCCFLAGS) -c -o $@ $<

$(ROM): $(OBJECTS)
	$(LCC) $(LCCFLAGS) -o $@ $(OBJECTS)
	@$(PYTHON) tools/romusage.py $@ build/kakuro.map

# Play it in PyBoy (arrows, A = a, B = s, START = Return, SELECT = Backspace);
# the battery RAM is kept in build/kakuro.gb.ram.
run: $(ROM)
	$(PYTHON) -m pyboy --dmg -s 4 $(ROM)

# Regenerate src/puzzles.[ch] from assets/puzzles (checks every puzzle and
# adds givens until each has one solution).
puzzles:
	$(PYTHON) tools/gen_puzzles.py

# Regenerate src/gfx.[ch]; the tile sheet goes to build/gfx_preview.png.
gfx:
	$(PYTHON) tools/gen_gfx.py --preview

# Every puzzle loaded and solved in the emulator, with saving and the menus.
test: $(ROM)
	$(PYTHON) tools/test_game.py $(ROM)

# Screenshots of the screens -> build/screens/*.png; docs: the README's
# pictures in Game Boy green -> docs/*.png
screenshots: $(ROM)
	$(PYTHON) tools/screenshots.py $(ROM)

docs: $(ROM)
	$(PYTHON) tools/screenshots.py $(ROM) --docs

clean:
	rm -rf build
