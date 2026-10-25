/* SPDX-License-Identifier: GPL-3.0-only */
/* board.c -- drawing the board, the view and the sprites.
 *
 * Map coordinates: the left half of cell column c is map column 2c-1, so
 * column 0 has only its right half at map column 0; rows likewise. The rest
 * of the 32x32 map is black, which is what the screen shows around a board
 * smaller than the screen.
 *
 * Sprites 0-3 are the cursor's corners, 4-13 the keypad keys. */
#include <gb/gb.h>
#include "board.h"
#include "puzzle.h"
#include "gfx.h"
#include "video.h"

#define SPR_CURSOR 0
#define SPR_KEYS   4
#define KEY_PITCH  7                        /* keys overlap by their 1-dot border */

uint8_t cur;
uint8_t key;

static uint8_t board_px;                    /* the board's width and height in dots */
static int16_t view_x, view_y;              /* the map dot at the screen's top-left corner */
static int16_t cur_x, cur_y;                /* where the cursor is drawn (map dots) */
static uint8_t cursor_on, pulse;
static uint8_t pad_x, pad_y;                /* keypad position on the screen */

static void cell_tiles(uint8_t i, uint8_t *q)
{
    uint8_t v = cell[i], d, base, k;
    if (v & CELL_WHITE) {
        d = v & CELL_DIGIT;
        base = !d ? T_WHITE : ((v & CELL_FIXED) ? T_GIVEN : T_DIGIT) + (d - 1) * 4;
        q[0] = base;
        q[1] = base + 1;
        q[2] = base + 2;
        q[3] = base + 3;
        return;
    }
    k = (sum_a[i] ? 2 : 0) | (sum_d[i] ? 1 : 0);
    q[0] = T_TL + k;
    q[1] = sum_a[i] ? T_ACROSS + sum_a[i] - SUM_MIN : T_BLACK;
    q[2] = sum_d[i] ? T_DOWN + sum_d[i] - SUM_MIN : T_BLACK;
    q[3] = T_BR + k;
}

static void draw_at(uint8_t i, uint8_t r, uint8_t c)
{
    uint8_t q[4];
    cell_tiles(i, q);
    if (r && c)
        set_bkg_tiles(2 * c - 1, 2 * r - 1, 2, 2, q);
    else if (r) {                           /* column 0: the right half */
        set_bkg_tiles(0, 2 * r - 1, 1, 1, q + 1);
        set_bkg_tiles(0, 2 * r, 1, 1, q + 3);
    }
    else if (c)                             /* row 0: the lower half */
        set_bkg_tiles(2 * c - 1, 0, 2, 1, q + 2);
    else
        set_bkg_tiles(0, 0, 1, 1, q + 3);
}

void board_draw_cell(uint8_t i)
{
    draw_at(i, i / n, i % n);
}

static int16_t target_x(void) { return (int16_t)(cur % n) * 16 - 8; }
static int16_t target_y(void) { return (int16_t)(cur / n) * 16 - 8; }

/* The view row that keeps the cursor's cell in sight (tall boards only). */
static int16_t view_target(void)
{
    int16_t y;
    if (board_px <= VIEW_H)
        return -(int16_t)(VIEW_H - board_px) / 2;
    y = target_y() + 8 - VIEW_H / 2;
    if (y < 0)
        y = 0;
    if (y > board_px - VIEW_H)
        y = board_px - VIEW_H;
    return y;
}

void board_setup(void)
{
    uint8_t i = 0, r, c, k;
    vram_tiles(0, GAME_TILE_COUNT, game_tiles);
    fill_bkg_rect(0, 0, 32, 32, T_BLACK);
    for (r = 0; r < n; r++)
        for (c = 0; c < n; c++, i++)
            draw_at(i, r, c);
    board_px = (2 * n - 1) * 8;
    view_x = -(int16_t)(160 - board_px) / 2;
    for (k = 0; k < 4; k++) {
        set_sprite_tile(SPR_CURSOR + k, S_CORNER);
        set_sprite_prop(SPR_CURSOR + k, (k & 1 ? S_FLIPX : 0) | (k & 2 ? S_FLIPY : 0));
    }
    keypad_close();
    board_cursor(1);
    board_snap();
}

/* One step of a glide: half the way, at least one dot. */
static int16_t glide(int16_t from, int16_t to)
{
    int16_t d = to - from;
    if (d > 0)
        return from + (d + 1) / 2;
    if (d < 0)
        return from - (1 - d) / 2;
    return from;
}

static void place_sprites(void)
{
    uint8_t x, y, p;
    if (!cursor_on)
        return;
    /* The brackets sit inside the cell's grid lines (dots 1-15) and step
     * one dot inwards every half second. */
    x = (uint8_t)(cur_x - view_x + 8 + 1);
    y = (uint8_t)(cur_y - view_y + 16 + 1);
    p = (pulse & 0x20) ? 1 : 0;
    move_sprite(SPR_CURSOR + 0, x + p, y + p);
    move_sprite(SPR_CURSOR + 1, x + 7 - p, y + p);
    move_sprite(SPR_CURSOR + 2, x + p, y + 7 - p);
    move_sprite(SPR_CURSOR + 3, x + 7 - p, y + 7 - p);
}

void board_update(void)
{
    int16_t ty = view_target();
    pulse++;
    if (view_y != ty)
        view_y += view_y < ty ? 2 : -2;     /* targets are even, so this lands */
    cur_x = glide(cur_x, target_x());
    cur_y = glide(cur_y, target_y());
    move_bkg((uint8_t)view_x, (uint8_t)view_y);
    place_sprites();
}

void board_snap(void)
{
    view_y = view_target();
    cur_x = target_x();
    cur_y = target_y();
    move_bkg((uint8_t)view_x, (uint8_t)view_y);
    place_sprites();
}

void board_cursor(uint8_t visible)
{
    uint8_t k;
    cursor_on = visible;
    if (visible)
        place_sprites();
    else
        for (k = 0; k < 4; k++)
            hide_sprite(SPR_CURSOR + k);
}

/* Keypad: keys 1-9 in three rows, the erase key under 8. */
static void key_place(uint8_t k)            /* k: 1..10 */
{
    uint8_t col = k == 10 ? 1 : (k - 1) % 3, row = k == 10 ? 3 : (k - 1) / 3;
    uint8_t s = SPR_KEYS + k - 1;
    set_sprite_tile(s, S_KEY + k - 1);
    set_sprite_prop(s, k == key ? S_PALETTE : 0);
    move_sprite(s, pad_x + col * KEY_PITCH + 8, pad_y + row * KEY_PITCH + 16);
}

void keypad_open(uint8_t digit)
{
    int16_t x = target_x() - view_x, y = target_y() - view_y;
    uint8_t k;
    key = digit ? digit : 5;
    x = x + 16 + 3 + 3 * KEY_PITCH + 1 <= 160 ? x + 19 : x - 3 - (3 * KEY_PITCH + 1);
    y = y + 8 - (4 * KEY_PITCH + 1) / 2;
    if (y < 0)
        y = 0;
    if (y > VIEW_H - (4 * KEY_PITCH + 1))
        y = VIEW_H - (4 * KEY_PITCH + 1);
    pad_x = (uint8_t)x;
    pad_y = (uint8_t)y;
    for (k = 1; k <= 10; k++)
        key_place(k);
}

void keypad_close(void)
{
    uint8_t k;
    for (k = 0; k < 10; k++)
        hide_sprite(SPR_KEYS + k);
}

uint8_t keypad_move(uint8_t buttons)
{
    uint8_t old = key, col, row;
    if (key == 10) {
        col = 1;
        row = 3;
    } else {
        col = (key - 1) % 3;
        row = (key - 1) / 3;
    }
    if (buttons & J_LEFT)
        col = row == 3 ? col : (col + 2) % 3;
    else if (buttons & J_RIGHT)
        col = row == 3 ? col : (col + 1) % 3;
    else if (buttons & J_UP)
        row = row == 0 ? (col == 1 ? 3 : 2) : row - 1;
    else if (buttons & J_DOWN)
        row = row == 3 ? 0 : (row == 2 ? (col == 1 ? 3 : 0) : row + 1);
    key = row == 3 ? 10 : row * 3 + col + 1;
    if (key == old)
        return 0;
    key_place(old);
    key_place(key);
    return 1;
}
