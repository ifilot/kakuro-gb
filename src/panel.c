/* SPDX-License-Identifier: GPL-3.0-only */
/* panel.c -- the window layer. */
#include <gb/gb.h>
#include "panel.h"
#include "board.h"
#include "clock.h"
#include "gfx.h"
#include "puzzle.h"
#include "save.h"
#include "video.h"

#define SPACE (T_FONT)                      /* ' ' */

static uint8_t wy = PANEL_CLOSED, wy_target = PANEL_CLOSED;
static char text[21];

void panel_setup(void)
{
    fill_win_rect(0, 0, 32, 18, SPACE);
    wy = wy_target = PANEL_CLOSED;
    move_win(7, wy);
}

void panel_slide(uint8_t y)
{
    wy_target = y;
}

uint8_t panel_moving(void)
{
    return wy != wy_target;
}

void panel_update(void)
{
    if (wy == wy_target)
        return;
    if (wy < wy_target)
        wy = wy_target - wy > 12 ? wy + 12 : wy_target;
    else
        wy = wy - wy_target > 12 ? wy - 12 : wy_target;
    move_win(7, wy);
}

/* Writes text[] up to p, padded with spaces to the full width, at row y. */
static void line(uint8_t y, char *p)
{
    while (p < text + 20)
        *p++ = ' ';
    text[20] = 0;
    text_win(0, y, text);
}

/* "[20/4" for a run, "[ -  " without one. */
static char *run_label(char *p, char arrow, uint8_t head, uint8_t across)
{
    *p++ = arrow;
    if (head == NO_RUN)
        return put_str(p, " -  ");
    p = put_num(p, across ? sum_a[head] : sum_d[head], 2);
    *p++ = '/';
    return put_num(p, across ? len_a[head] : len_d[head], 1);
}

void status_draw(void)
{
    char *p = run_label(text, '[', head_a[cur], 1);
    *p++ = ' ';
    p = run_label(p, '\\', head_d[cur], 0);
    while (p < text + 13)
        *p++ = ' ';
    p = put_time(p, clock_now());
    line(0, p);
}

void status_time(void)
{
    char *p = put_time(text, clock_now());
    *p = 0;
    text_win(13, 0, text);
}

void status_note(const char *s)
{
    char *p = put_str(text, s);
    while (p < text + 13)
        *p++ = ' ';
    *p = 0;
    text_win(0, 0, text);
}

/* --- combinations -------------------------------------------------------- */

static uint8_t tiles[20];
static uint16_t masks[MAX_COMBOS];

/* The digits of the run other than those of cell `skip`. */
static uint16_t others(uint8_t head, uint8_t step, uint8_t len, uint8_t skip)
{
    uint16_t seen = 0;
    uint8_t i = head;
    while (len--) {
        i += step;
        if (i != skip)
            seen |= 1u << (cell[i] & CELL_DIGIT);
    }
    return seen & 0x3FE;
}

/* The heading and combinations of a run from window row y on; combinations
 * without every digit already in the run are dim. Returns the next free
 * row and puts the digits the cursor's cell can take into *fits. */
static uint8_t run_combos(uint8_t y, uint8_t across, uint16_t *fits)
{
    uint8_t head = across ? head_a[cur] : head_d[cur];
    uint8_t step = across ? 1 : n, len, sum, count, k, d, x, dim;
    uint16_t placed, other, m;
    char *p;

    if (head == NO_RUN) {
        p = put_str(text, across ? "[ NO SUM ACROSS" : "\\ NO SUM DOWN");
        line(y, p);
        return y + 2;
    }
    len = across ? len_a[head] : len_d[head];
    sum = across ? sum_a[head] : sum_d[head];
    p = text;
    *p++ = across ? '[' : '\\';
    *p++ = ' ';
    p = put_num(p, sum, 0);
    p = put_str(p, " IN ");
    p = put_num(p, len, 0);
    line(y++, p);

    placed = run_digits(head, step, len);
    other = others(head, step, len, cur);
    count = combinations(sum, len, masks);
    *fits = 0;
    x = 0;
    for (k = 0; k < 20; k++)
        tiles[k] = SPACE;
    for (k = 0; k < count; k++) {
        m = masks[k];
        if ((m & other) == other)
            *fits |= m & ~other;
        dim = (m & placed) != placed;
        if (x + len > 20) {
            set_win_tiles(0, y++, 20, 1, tiles);
            for (x = 0; x < 20; x++)
                tiles[x] = SPACE;
            x = 0;
        }
        for (d = 1; d <= 9; d++)
            if (m & (1u << d))
                tiles[x++] = dim ? T_DIM + d - 1 : T_FONT + '0' + d - 32;
        x++;
    }
    set_win_tiles(0, y++, 20, 1, tiles);
    return y + 1;
}

void panel_info(uint8_t k)
{
    uint16_t fits_a = 0x3FE, fits_d = 0x3FE, fits;
    uint8_t y, d;
    char *p;

    fill_win_rect(0, 1, 20, 17, SPACE);
    p = put_str(text, "PUZZLE ");
    p = put_num(p, k + 1, 3);
    p = put_str(p, "  ");
    p = put_size(p, n);
    *p++ = ' ';
    p = put_stars(p, stars);
    line(1, p);
    p = put_str(text, "BEST ");
    p = put_best(p, save_best(k));
    p = put_str(p, "  LEFT ");
    p = put_num(p, empty, 2);
    line(2, p);

    y = run_combos(4, 1, &fits_a);
    run_combos(y, 0, &fits_d);

    if (cell[cur] & CELL_FIXED)
        p = put_str(text, "GIVEN DIGIT");
    else {
        fits = fits_a & fits_d;
        p = put_str(text, "POSSIBLE: ");
        for (d = 1; d <= 9; d++)
            if (fits & (1u << d))
                *p++ = (char)('0' + d);
        if (!fits)
            p = put_str(p, "NONE");
    }
    line(16, p);
    line(17, put_str(text, "SELECT: BACK"));
    panel_slide(PANEL_FULL);
}

/* --- pause and solved ---------------------------------------------------- */

static const char *const pause_items[PAUSE_ITEMS] = {
    "RESUME", "REVEAL CELL  +0:30", "START OVER", "QUIT TO MENU"
};

void panel_pause(void)
{
    uint8_t k;
    fill_win_rect(0, 1, 20, 17, SPACE);
    for (k = 0; k < PAUSE_ITEMS; k++)
        text_win(2, 2 + k, pause_items[k]);
    panel_pause_choice(PAUSE_RESUME);
    panel_slide(144 - (PAUSE_ITEMS + 3) * 8);
}

void panel_pause_choice(uint8_t choice)
{
    uint8_t k;
    for (k = 0; k < PAUSE_ITEMS; k++)
        text_win(0, 2 + k, k == choice ? ">" : " ");
}

void panel_solved(uint16_t seconds, uint16_t best, uint8_t record)
{
    char *p;
    fill_win_rect(0, 1, 20, 17, SPACE);
    text_win(6, 2, "SOLVED!");
    p = put_str(text, "  TIME    ");
    p = put_time(p, seconds);
    line(3, p);
    if (record)
        p = put_str(text, "  NEW BEST TIME!");
    else {
        p = put_str(text, "  BEST    ");
        p = put_best(p, best);
    }
    line(4, p);
    line(6, put_str(text, "A: NEXT     B: MENU"));
    panel_slide(144 - 8 * 8);
}
