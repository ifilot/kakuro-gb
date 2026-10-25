/* SPDX-License-Identifier: GPL-3.0-only */
/* menu.c -- the screens around the game, on the background layer with the
 * font and the menu tiles (logo, preview) in place of the board tiles. The
 * choice is marked with a '>' in front. */
#include <gb/gb.h>
#include <string.h>
#include "menu.h"
#include "main.h"
#include "gfx.h"
#include "input.h"
#include "puzzle.h"
#include "puzzles.h"
#include "save.h"
#include "sound.h"
#include "version.h"
#include "video.h"

#define SPACE T_FONT
#define PER_ROW 4                           /* puzzles per row on the grid screen */
#define GRID_Y  3
#define PREVIEW_REST 8                      /* frames before the preview is drawn */

static char text[21];

static void menu_begin(void)
{
    uint8_t k;
    screen_off();
    vram_tiles(0, T_MENU, game_tiles);      /* font and dim digits */
    vram_tiles(T_MENU, MENU_TILE_COUNT, menu_tiles);
    fill_bkg_rect(0, 0, 32, 32, SPACE);
    move_bkg(0, 0);
    move_win(7, 144);
    for (k = 0; k < 40; k++)
        hide_sprite(k);
}

static void menu_end(void)
{
    screen_on();
    input_flush();
}

static void pointer(uint8_t x, uint8_t y, uint8_t on)
{
    text_bkg(x, y, on ? ">" : " ");
}

/* Writes text[] up to p, centred, at row y. */
static void centre(uint8_t y, char *p)
{
    *p = 0;
    text_bkg((uint8_t)(20 - (p - text)) / 2, y, text);
}

static uint8_t solved_in(uint8_t g)
{
    uint8_t k, count = 0;
    for (k = group_first[g]; k < group_first[g] + group_count[g]; k++)
        if (save_best(k))
            count++;
    return count;
}

/* --- title ---------------------------------------------------------------- */

enum { ITEM_CONTINUE, ITEM_PLAY, ITEM_HELP };
static const char *const title_items[] = { "CONTINUE", "CHOOSE PUZZLE", "HOW TO PLAY" };

uint8_t title_screen(void)
{
    uint8_t items[3], count = 0, choice = 0, k, last = save_last();
    char *p;

    menu_begin();
    set_bkg_tiles(0, 1, LOGO_W, LOGO_H, logo_map);
    centre(7, put_str(text, "THE NUMBER CROSSWORD"));
    if (last != NO_PUZZLE && save_started(last))
        items[count++] = ITEM_CONTINUE;
    items[count++] = ITEM_PLAY;
    items[count++] = ITEM_HELP;
    for (k = 0; k < count; k++)
        text_bkg(5, 10 + 2 * k, title_items[items[k]]);
    p = put_str(text, "V" VERSION "   ");
    p = put_num(p, PUZZLE_COUNT, 0);
    centre(17, put_str(p, " PUZZLES"));
    pointer(3, 10, 1);
    menu_end();

    for (;;) {
        frame();
        input_update();
        if (repeated & (J_UP | J_DOWN)) {
            pointer(3, 10 + 2 * choice, 0);
            choice = (choice + (repeated & J_UP ? count - 1 : 1)) % count;
            pointer(3, 10 + 2 * choice, 1);
            sfx(SFX_MOVE);
        } else if (pressed & (J_A | J_START)) {
            sfx(SFX_SELECT);
            switch (items[choice]) {
            case ITEM_CONTINUE:
                current = last;
                return SCREEN_GAME;
            case ITEM_PLAY:
                return SCREEN_GROUPS;
            default:
                return SCREEN_HELP;
            }
        }
    }
}

/* --- groups --------------------------------------------------------------- */

#define GROUPS_Y 4

uint8_t groups_screen(void)
{
    uint8_t g, choice = puzzle_group(current);
    char *p;

    menu_begin();
    centre(1, put_str(text, "CHOOSE A PUZZLE"));
    text_bkg(2, GROUPS_Y - 1, "SIZE  LEVEL");
    text_bkg(14, GROUPS_Y - 1, "SOLVED");
    for (g = 0; g < GROUP_COUNT; g++) {
        p = put_size(text, group_n[g]);
        while (p < text + 6)
            *p++ = ' ';
        p = put_stars(p, group_stars[g]);
        while (p < text + 12)
            *p++ = ' ';
        p = put_num(p, solved_in(g), 2);
        *p++ = '/';
        p = put_num(p, group_count[g], 2);
        *p = 0;
        text_bkg(2, GROUPS_Y + g, text);
    }
    text_bkg(1, 16, "A: OPEN    B: BACK");
    pointer(0, GROUPS_Y + choice, 1);
    menu_end();

    for (;;) {
        frame();
        input_update();
        if (repeated & (J_UP | J_DOWN)) {
            pointer(0, GROUPS_Y + choice, 0);
            choice = (choice + (repeated & J_UP ? GROUP_COUNT - 1 : 1)) % GROUP_COUNT;
            pointer(0, GROUPS_Y + choice, 1);
            sfx(SFX_MOVE);
        } else if (pressed & (J_A | J_START)) {
            sfx(SFX_SELECT);
            if (puzzle_group(current) != choice)
                current = group_first[choice];
            return SCREEN_GRID;
        } else if (pressed & J_B) {
            sfx(SFX_ERASE);
            return SCREEN_TITLE;
        }
    }
}

/* --- puzzle grid ---------------------------------------------------------- */

/* The preview: the puzzle at 4 dots a cell, centred in 5x5 tiles. Black
 * cells black, empty white cells white, given digits light grey, entered
 * ones dark grey. */
static uint8_t preview[25 * 16];
static uint8_t dots[40];                    /* one dot row: a shade per dot */
static uint8_t packed[10];                  /* ... as 5 tiles' two bit planes */
static const uint16_t tile_row[5] = { 0, 80, 160, 240, 320 };

static void pack(void)
{
    const uint8_t *d = dots;
    uint8_t t, k, lo, hi;
    for (t = 0; t < 10; t += 2) {
        lo = hi = 0;
        for (k = 0; k < 8; k++, d++) {
            lo = (uint8_t)(lo << 1) | (*d & 1);
            hi = (uint8_t)(hi << 1) | (*d >> 1);
        }
        packed[t] = lo;
        packed[t + 1] = hi;
    }
}

static void put_row(uint8_t y)
{
    uint8_t *row = preview + tile_row[y >> 3] + ((y & 7) << 1);
    uint8_t t;
    for (t = 0; t < 10; t += 2, row += 16) {
        row[0] = packed[t];
        row[1] = packed[t + 1];
    }
}

static void draw_preview(uint8_t k)
{
    uint8_t i = 0, r, c, x, y, x0, v;
    puzzle_load(k);
    save_restore(k);
    memset(preview, 0, sizeof preview);
    x0 = (10 - n) * 2;
    for (r = 0, y = x0; r < n; r++, y += 4) {
        memset(dots, 0, sizeof dots);
        for (c = 0, x = x0; c < n; c++, x += 4, i++) {
            v = cell[i];
            dots[x] = dots[x + 1] = dots[x + 2] =
                !(v & CELL_WHITE) ? 3 : (v & CELL_FIXED) ? 1 : (v & CELL_DIGIT) ? 2 : 0;
            dots[x + 3] = 3;
        }
        pack();
        put_row(y);
        put_row(y + 1);
        put_row(y + 2);
        memset(dots + x0, 3, n * 4);
        pack();
        put_row(y + 3);
    }
    vram_tiles(T_PREVIEW, 25, preview);
}

/* The puzzle's number, best time and state; the preview follows once the
 * choice rests (drawing it takes a few frames). */
static void grid_info(uint8_t k)
{
    uint16_t best = save_best(k);
    char *p;
    p = put_str(text, "PUZZLE ");
    p = put_num(p, k + 1, 0);
    p = put_str(p, "  ");
    *p = 0;
    text_bkg(8, 10, text);
    p = put_str(text, "BEST ");
    p = put_best(p, best);
    *p = 0;
    text_bkg(8, 12, text);
    text_bkg(8, 14, save_started(k) ? "IN PROGRESS" : best ? "SOLVED     " : "NEW        ");
}

static void grid_pointer(uint8_t j, uint8_t on)
{
    pointer((j % PER_ROW) * 5, GRID_Y + j / PER_ROW, on);
}

uint8_t grid_screen(void)
{
    uint8_t g = puzzle_group(current), first = group_first[g], count = group_count[g];
    uint8_t j = current - first, k, x, y, rest = 0;
    uint8_t map[25];
    char *p;

    menu_begin();
    p = put_size(text, group_n[g]);
    p = put_str(p, "  ");
    p = put_stars(p, group_stars[g]);
    centre(1, p);
    for (k = 0; k < count; k++) {
        p = put_num(text, first + k + 1, 3);
        *p++ = save_started(first + k) ? '_' : save_best(first + k) ? ']' : ' ';
        *p = 0;
        text_bkg((k % PER_ROW) * 5 + 1, GRID_Y + k / PER_ROW, text);
    }
    for (y = 0; y < 5; y++)
        for (x = 0; x < 5; x++)
            map[y * 5 + x] = T_PREVIEW + y * 5 + x;
    set_bkg_tiles(1, 10, 5, 5, map);
    text_bkg(1, 16, "] SOLVED  _ STARTED");
    text_bkg(1, 17, "A: PLAY    B: BACK");
    grid_pointer(j, 1);
    grid_info(first + j);
    draw_preview(first + j);
    menu_end();

    for (;;) {
        frame();
        input_update();
        if (rest && !--rest)
            draw_preview(first + j);
        if (repeated & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) {
            k = j;
            if (repeated & J_LEFT)
                k = j ? j - 1 : count - 1;
            else if (repeated & J_RIGHT)
                k = j + 1 < count ? j + 1 : 0;
            else if (repeated & J_UP)
                k = j >= PER_ROW ? j - PER_ROW : j;
            else if (j + PER_ROW < count)
                k = j + PER_ROW;
            if (k == j) {
                sfx(SFX_BUMP);
                continue;
            }
            grid_pointer(j, 0);
            j = k;
            grid_pointer(j, 1);
            grid_info(first + j);
            rest = PREVIEW_REST;
            sfx(SFX_MOVE);
        } else if (pressed & (J_A | J_START)) {
            sfx(SFX_SELECT);
            current = first + j;
            return SCREEN_GAME;
        } else if (pressed & J_B) {
            sfx(SFX_ERASE);
            current = first + j;
            return SCREEN_GROUPS;
        }
    }
}

/* --- rules ---------------------------------------------------------------- */

static const char *const help_text[] = {
    "    HOW TO PLAY",
    "",
    "FILL EVERY WHITE",
    "CELL WITH A DIGIT",
    "1 TO 9. EACH RUN",
    "ADDS UP TO ITS SUM:",
    "TOP-RIGHT ACROSS,",
    "BOTTOM-LEFT DOWN.",
    "NO DIGIT TWICE IN A",
    "RUN.",
    "",
    "D-PAD  MOVE",
    "A      ENTER A DIGIT",
    "B      WIPE THE CELL",
    "SELECT COMBINATIONS",
    "START  PAUSE MENU",
};

uint8_t help_screen(void)
{
    uint8_t k;
    menu_begin();
    for (k = 0; k < sizeof help_text / sizeof help_text[0]; k++)
        text_bkg(0, k, help_text[k]);
    menu_end();
    for (;;) {
        frame();
        input_update();
        if (pressed & (J_A | J_B | J_START)) {
            sfx(SFX_ERASE);
            return SCREEN_TITLE;
        }
    }
}
