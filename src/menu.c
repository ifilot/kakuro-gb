/* SPDX-License-Identifier: GPL-3.0-only */
/* menu.c -- the screens around the game: the start screen, the title with
 * the groups to choose from, and the puzzles of a group.
 *
 * They look like the board: black, with white cells and sums. Cells and
 * texts are drawn on canvases (canvas.c), each in tiles of its own, once
 * (that takes frames); the choice has the board's cursor, sprites 0-3, that
 * steps a dot inwards every half second. */
#include <gb/gb.h>
#include "menu.h"
#include "main.h"
#include "canvas.h"
#include "gfx.h"
#include "input.h"
#include "puzzle.h"
#include "puzzles.h"
#include "best.h"
#include "sound.h"
#include "version.h"
#include "video.h"

#define PREVIEW_REST 8                      /* frames before the puzzle's details are drawn */

static char text[24];
static uint8_t br_x, br_y, br_w, br_h, br_on, pulse;

static void brackets_place(void)
{
    uint8_t p = (pulse & 0x20) ? 1 : 0;
    if (!br_on)
        return;
    uint8_t x0 = br_x + 8 + p, x1 = br_x + br_w - p, y0 = br_y + 16 + p, y1 = br_y + br_h + 8 - p;
    move_sprite(0, x0, y0);
    move_sprite(1, x1, y0);
    move_sprite(2, x0, y1);
    move_sprite(3, x1, y1);
}

/* The cursor around w x h dots from screen dot (x, y). */
static void brackets(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    br_x = x;
    br_y = y;
    br_w = w;
    br_h = h;
    br_on = 1;
    brackets_place();
}

static void menu_begin(void)
{
    uint8_t k;
    screen_off();
    video_palettes(0, 0);
    vram_tiles(0, MENU_TILE_COUNT, menu_tiles);
    fill_bkg_rect(0, 0, 32, 32, M_BLACK);
    attr_bkg(0, 0, 32, 32, CPAL_PAPER);
    move_bkg(0, 0);
    move_win(7, 144);
    for (k = 0; k < 40; k++)
        hide_sprite(k);
    br_on = 0;
    for (k = 0; k < 4; k++) {
        set_sprite_tile(k, S_CORNER);
        set_sprite_prop(k, (k & 1 ? S_FLIPX : 0) | (k & 2 ? S_FLIPY : 0));
    }
    pulse = 0;
}

static void menu_end(void)
{
    screen_on();
    input_flush();
}

static void menu_frame(void)
{
    frame();
    input_update();
    if (!(++pulse & 0x1F))
        brackets_place();
}

/* "33 OF 80 SOLVED" for count of total */
static char *put_solved(char *p, uint8_t count, uint8_t total)
{
    p = put_num(p, count, 0);
    p = put_str(p, " OF ");
    p = put_num(p, total, 0);
    return put_str(p, " SOLVED");
}

/* --- the start screen ----------------------------------------------------- */

/* At power-on, in the title screens' frame (the logo and the border): a
 * scene (tools/scene.py), a zen garden seen from above. The logo is filled
 * in a part at a time (the letters, then the sum), then PRESS START blinks
 * under the scene. On the Color the screen
 * has palettes of its own. */
#define T_SCENE     M_FREE_TITLE
#define T_PRESS     (T_SCENE + SCENE_TILE_COUNT)    /* 12 tiles */
#define PRESS_X     4                       /* the line's map column, row 16 */
#define PART_FRAMES 10                      /* between the logo's parts */

static void press_line(uint8_t on)
{
    uint8_t row[12], k;
    for (k = 0; k < 12; k++)
        row[k] = on ? (uint8_t)(T_PRESS + k) : M_BLACK;
    set_bkg_tiles(PRESS_X, 16, 12, 1, row);
}

uint8_t splash_screen(void)
{
    uint8_t part = 0, wait = PART_FRAMES, r, p, t = 0;

    menu_begin();
    video_palettes(scene_colours, 8);
    vram_tiles(M_LOGO, FRAME_TILE_COUNT, frame_tiles);
    set_bkg_tiles(0, 0, 20, 18, frame_map);
    set_bkg_tiles(0, 0, LOGO_W, LOGO_H, logo_empty);
    attr_bkg(0, 0, 20, 18, SCENE_PAL_LOGO);
    vram_tiles(T_SCENE, SCENE_TILE_COUNT, scene_tiles);
    set_bkg_tiles(0, SCENE_Y, 20, SCENE_H, scene_map);
    attr_tiles(0, SCENE_Y, 20, SCENE_H, scene_attr);
    canvas_begin(12, 1, C_BLACK);
    canvas_text((96 - 11 * 8) / 2, 0, "PRESS START", C_WHITE);
    canvas_show((uint8_t)T_PRESS, PRESS_X, 16);
    press_line(0);
    menu_end();

    for (;;) {
        menu_frame();
        t++;
        if (pressed & (J_START | J_A)) {
            sfx(SFX_SELECT);
            return SCREEN_TITLE;
        }
        if (part < LOGO_PARTS) {            /* the letters K-A-K-U-R-O, then the 26 */
            if (--wait)
                continue;
            p = part + 1 < LOGO_PARTS ? part + 1 : 0;
            for (r = 0; r < LOGO_H; r++)
                set_bkg_tiles(logo_part_x[p], r, logo_part_w[p], 1, frame_map + r * 20 + logo_part_x[p]);
            sfx(p ? SFX_PLACE : SFX_SELECT);
            wait = PART_FRAMES;
            part++;
            continue;
        }
        if (!(t & 0x1F))
            press_line(t & 0x20);
    }
}

/* --- title ---------------------------------------------------------------- */

/* The logo in the border, and under it the groups as cards, two to a row
 * (a size a row): the size as a board cell with a given digit, the stars
 * out of three, and a bar of the puzzles solved (light grey once all are;
 * SOON for a group without puzzles yet). The version at the bottom. */
#define CARD_X     1                        /* map tiles; a card's canvas is 9 x 3 */
#define CARD_Y     6
#define LINE_Y     16                       /* the bottom line; the border's bottom is row 17 */
#define T_SUBTITLE M_FREE_TITLE             /* 10 tiles */
#define T_CARDS    (T_SUBTITLE + 10)        /* 27 tiles a card */
#define T_LINE     (T_CARDS + 27 * GROUP_COUNT)    /* 18 */

#if GROUP_COUNT != 6
#error "the title has a 3x2 grid of groups"
#endif

/* The card's background: 70 x 22 dots with rounded corners, from (1, 1). */
static void card_begin(uint8_t shade)
{
    canvas_begin(9, 3, C_BLACK);
    canvas_fill(1, 1, 70, 22, shade);
    canvas_fill(1, 1, 1, 1, C_BLACK);
    canvas_fill(70, 1, 1, 1, C_BLACK);
    canvas_fill(1, 22, 1, 1, C_BLACK);
    canvas_fill(70, 22, 1, 1, C_BLACK);
}

/* "14/20" at the card's right edge, and the bar up to it. */
static void card_bar(uint8_t solved, uint8_t count)
{
    uint8_t w;
    char *p = put_num(text, solved, 0);
    *p++ = '/';
    *put_num(p, count, 0) = 0;
    w = 67 - canvas_tiny_width(text);
    canvas_tiny(w, 16, text, C_BLACK);
    w -= 25 + 3;
    canvas_fill(25, 16, w, 5, C_BLACK);
    canvas_fill(26, 17, w - 2, 3, C_WHITE);
    canvas_fill(26, 17, (uint8_t)((uint16_t)(w - 2) * solved / count), 3, C_DARK);
}

/* The difficulty out of three stars, the missing ones in shade off: all
 * three in it, then the first ones over them. */
static const char three_stars[] = "^^^";

static void group_card(uint8_t g)
{
    uint8_t count = group_count[g], solved = best_solved(group_first[g], count);
    uint8_t done = count && solved == count;
    card_begin(done ? C_LIGHT : C_WHITE);
    canvas_fill(3, 3, 18, 18, C_BLACK);     /* the cell, with the board's lines */
    canvas_fill(4, 4, 16, 16, C_LIGHT);
    text[0] = (char)('0' + group_n[g]);
    text[1] = 0;
    canvas_text(9, 8, text, C_BLACK);
    text[0] = 'X';
    text[1] = (char)('0' + group_n[g]);
    text[2] = 0;
    canvas_text(25, 4, text, C_BLACK);
    canvas_text(45, 4, three_stars, done ? C_WHITE : C_LIGHT);
    canvas_text(45, 4, three_stars + 3 - group_stars[g], C_DARK);
    if (count)
        card_bar(solved, count);
    else
        canvas_tiny(25, 16, "SOON", C_DARK);
    canvas_show(T_CARDS + 27 * g, CARD_X + 9 * (g & 1), CARD_Y + 3 * (g >> 1));
}

static void title_cursor(uint8_t k)
{
    brackets((CARD_X + 9 * (k & 1)) * 8 + 1, (CARD_Y + 3 * (k >> 1)) * 8 + 1, 70, 22);
}

uint8_t title_screen(void)
{
    uint8_t g, k, choice = puzzle_group(current);
    char *p;

    menu_begin();
    vram_tiles(M_LOGO, FRAME_TILE_COUNT, frame_tiles);
    set_bkg_tiles(0, 0, 20, 18, frame_map);
    attr_bkg(0, 0, LOGO_W, LOGO_H, CPAL_CLUE);
    canvas_begin(10, 1, C_BLACK);           /* under the logo, in its last row */
    canvas_tiny(0, 2, "THE NUMBER CROSSWORD", C_LIGHT);
    canvas_show(T_SUBTITLE, 5, LOGO_H - 1);
    attr_bkg(5, LOGO_H - 1, 10, 1, CPAL_PAPER);
    for (g = 0; g < GROUP_COUNT; g++)
        group_card(g);
    canvas_begin(18, 1, C_BLACK);
    canvas_tiny(2, 2, "V" VERSION, C_DARK);
    p = put_num(text, PUZZLE_COUNT, 0);
    *put_str(p, " PUZZLES") = 0;
    canvas_tiny(142 - canvas_tiny_width(text), 2, text, C_DARK);
    canvas_show((uint8_t)T_LINE, 1, LINE_Y);
    title_cursor(choice);
    menu_end();

    for (;;) {
        menu_frame();
        if (repeated & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) {
            k = choice;
            if (repeated & (J_LEFT | J_RIGHT))
                k ^= 1;
            else if (repeated & J_UP) {
                if (k >= 2)
                    k -= 2;
            } else if (k + 2 < GROUP_COUNT)
                k += 2;
            if (k == choice) {
                sfx(SFX_BUMP);
                continue;
            }
            choice = k;
            title_cursor(choice);
            sfx(SFX_MOVE);
        } else if (pressed & (J_A | J_START)) {
            if (!group_count[choice]) {
                sfx(SFX_BUMP);
                continue;
            }
            sfx(SFX_SELECT);
            if (puzzle_group(current) != choice)
                current = group_first[choice];
            return SCREEN_GRID;
        }
    }
}

/* --- puzzle grid ---------------------------------------------------------- */

/* The puzzles of the group as a board of cells, PER_ROW to a row (solved
 * ones light grey like given digits, with a check mark); the
 * chosen one's preview beside it, a tile a cell, and its details below. */
#define PER_ROW   5
#define CELLS_X   1
#define CELLS_Y   3
#define PREVIEW_X 12
#define INFO_Y    12
#define T_GHEAD   M_FREE                    /* 40 tiles */
#define T_CELLS   (T_GHEAD + 40)            /* 80 */
#define T_INFO    (T_CELLS + 80)            /* 60 */
#define T_GKEYS   (T_INFO + 60)             /* 20 */

static void grid_cells(uint8_t first, uint8_t count)
{
    uint8_t j, x, y;
    canvas_begin(10, 8, C_BLACK);
    for (j = 0; j < count; j++) {
        x = (j % PER_ROW) * 16;
        y = (j / PER_ROW) * 16;
        canvas_fill(x + 1, y + 1, 15, 15, best_time(first + j) ? C_LIGHT : C_WHITE);
        *put_num(text, first + j + 1, 0) = 0;
        canvas_tiny(x + (16 - canvas_tiny_width(text)) / 2, y + 7, text, C_BLACK);
        if (best_time(first + j))           /* a check mark, top right */
            canvas_tiny(x + 9, y + 2, "]", C_DARK);
    }
    canvas_show(T_CELLS, CELLS_X, CELLS_Y);
}

static void grid_cursor(uint8_t j)
{
    brackets(CELLS_X * 8 + (j % PER_ROW) * 16 + 1, CELLS_Y * 8 + (j / PER_ROW) * 16 + 1, 15, 15);
}

/* Puzzle k's preview and details. */
static void grid_info(uint8_t k)
{
    uint8_t map[64], i, r, c, v, o, t;
    uint16_t best = best_time(k);
    char *p;

    puzzle_load(k);
    for (i = 0; i < 64; i++)
        map[i] = M_BLACK;
    o = (8 - n) / 2;
    for (i = 0, r = 0; r < n; r++)
        for (c = 0; c < n; c++, i++) {
            v = cell[i];
            if (!(v & CELL_WHITE))
                continue;
            /* A plain if: SDCC 4.5 stored a ?: chain here through the
             * wrong pointer for cells that are not given. */
            t = M_PREVIEW;
            if (v & CELL_FIXED)
                t += 1;
            map[(r + o) * 8 + c + o] = t;
        }
    set_bkg_tiles(PREVIEW_X, CELLS_Y, 8, 8, map);

    canvas_begin(20, 3, C_BLACK);
    p = put_str(text, "PUZZLE ");
    *put_num(p, k + 1, 0) = 0;
    canvas_text(4, 0, text, C_WHITE);
    canvas_tiny(4, 11, "BEST", C_LIGHT);
    *put_best(text, best) = 0;
    canvas_tiny(40, 11, text, C_WHITE);
    canvas_tiny(4, 18, "STATE", C_LIGHT);
    canvas_tiny(40, 18, best ? "SOLVED" : "NEW", C_WHITE);
    canvas_show(T_INFO, 0, INFO_Y);
}

uint8_t grid_screen(void)
{
    uint8_t g = puzzle_group(current), first = group_first[g], count = group_count[g];
    uint8_t j = current - first, k, rest = 0;

    menu_begin();
    canvas_begin(20, 2, C_BLACK);
    *put_size(text, group_n[g]) = 0;
    k = canvas_text(4, 4, text, C_WHITE);
    canvas_tiny(k + 2, 5, three_stars, C_DARK);
    canvas_tiny(k + 2, 5, three_stars + 3 - group_stars[g], C_LIGHT);
    *put_solved(text, best_solved(first, count), count) = 0;
    canvas_tiny(157 - canvas_tiny_width(text), 5, text, C_LIGHT);
    canvas_show(T_GHEAD, 0, 0);
    grid_cells(first, count);
    attr_bkg(PREVIEW_X, CELLS_Y, 8, 8, CPAL_CELL);
    canvas_begin(20, 1, C_BLACK);
    canvas_tiny(3, 1, "A: PLAY   B: BACK", C_LIGHT);
    canvas_tiny(77, 1, "]", C_LIGHT);
    canvas_tiny(84, 1, "SOLVED", C_LIGHT);
    canvas_show((uint8_t)T_GKEYS, 0, 17);
    grid_cursor(j);
    grid_info(first + j);
    menu_end();

    for (;;) {
        menu_frame();
        if (rest && !--rest)
            grid_info(first + j);
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
            j = k;
            grid_cursor(j);
            rest = PREVIEW_REST;
            sfx(SFX_MOVE);
        } else if (pressed & (J_A | J_START)) {
            sfx(SFX_SELECT);
            current = first + j;
            return SCREEN_GAME;
        } else if (pressed & J_B) {
            sfx(SFX_ERASE);
            current = first + j;
            return SCREEN_TITLE;
        }
    }
}

