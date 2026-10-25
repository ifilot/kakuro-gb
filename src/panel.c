/* SPDX-License-Identifier: GPL-3.0-only */
/* panel.c -- the window layer. */
#include <gb/gb.h>
#include <string.h>
#include "panel.h"
#include "board.h"
#include "clock.h"
#include "gfx.h"
#include "puzzle.h"
#include "puzzles.h"
#include "best.h"
#include "sound.h"
#include "video.h"

#define SPACE (T_FONT)                      /* ' ' */

/* The status line is in the 3x5 font of the sums, in tiles of its own:
 * BAR_LEFT at the left (the runs, or a note) and BAR_RIGHT at the right
 * (the clock), spaces between. The text sits in 4-dot cells, two to a
 * tile; a glyph wider than 3 dots (an arrow) takes two cells. */
#define BAR_LEFT  7
#define BAR_RIGHT 4
#define BAR_TOP   2                         /* the text's top row in the tiles */
#if BAR_LEFT + BAR_RIGHT > STATUS_TILE_MAX
#error "no room for the status line's tiles"
#endif

static uint8_t wy = PANEL_CLOSED, wy_target = PANEL_CLOSED;
static char text[21];
char status_text[16];
static uint8_t bar_data[(BAR_LEFT + BAR_RIGHT) * 16];

/* The window column of status tile k. */
static uint8_t bar_column(uint8_t k)
{
    return k < BAR_LEFT ? k : 20 - BAR_LEFT - BAR_RIGHT + k;
}

void panel_setup(void)
{
    uint8_t k;
    fill_win_rect(0, 0, 32, 18, SPACE);
    for (k = 0; k < BAR_LEFT + BAR_RIGHT; k++)
        set_win_tile_xy(bar_column(k), 0, T_STATUS + k);
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

/* "[20/4" for a run, "[ -  " without one. Sums are below 46 and lengths
 * below 10, so no put_num: its 16-bit division is slow for every step. */
static char *run_label(char *p, char arrow, uint8_t head, uint8_t across)
{
    uint8_t sum, tens = 0;
    *p++ = arrow;
    if (head == NO_RUN)
        return put_str(p, " -  ");
    sum = across ? sum_a[head] : sum_d[head];
    while (sum >= 10) {
        sum -= 10;
        tens++;
    }
    *p++ = tens ? (char)('0' + tens) : ' ';
    *p++ = (char)('0' + sum);
    *p++ = '/';
    *p++ = (char)('0' + (across ? len_a[head] : len_d[head]));
    return p;
}

/* The 5 rows of a glyph, black, into a tile's left half, its right half,
 * and what a wide glyph in the right half spills into the next tile. Small
 * loops over two pointers: SDCC keeps those in registers. */
static void rows_left(uint8_t *p, const uint8_t *g)
{
    uint8_t r = 5, v;
    do {
        v = *g++;
        *p++ |= v;
        *p++ |= v;
    } while (--r);
}

static void rows_right(uint8_t *p, const uint8_t *g)
{
    uint8_t r = 5, v;
    do {
        v = *g++ >> 4;
        *p++ |= v;
        *p++ |= v;
    } while (--r);
}

static void rows_spill(uint8_t *p, const uint8_t *g)
{
    uint8_t r = 5, v;
    do {
        v = *g++ << 4;
        *p++ |= v;
        *p++ |= v;
    } while (--r);
}

/* Draws s into the `count` status tiles from `tile` on, black on white,
 * and has them loaded in the next vertical blank. */
static void bar_print(uint8_t tile, uint8_t count, const char *s)
{
    uint8_t *start = bar_data + tile * 16, *p = start + BAR_TOP * 2;
    const uint8_t *g;
    uint8_t half = 0, w;
    memset(start, 0, count * 16);
    while (*s) {
        g = tiny_font + (uint8_t)(*s++ - 32) * 6;
        w = *g++;
        if (!half)
            rows_left(p, g);
        else {
            rows_right(p, g);
            if (w > 3)
                rows_spill(p + 16, g);
        }
        if (w > 3 || half)
            p += 16;
        if (w <= 3)
            half ^= 1;
    }
    vram_tiles_later(T_STATUS + tile, count, start);
}

void status_draw(void)
{
    char *p = run_label(status_text, '[', head_a[cur], 1);
    *p++ = ' ';
    p = run_label(p, '\\', head_d[cur], 0);
    *p = 0;
    bar_print(0, BAR_LEFT, status_text);
}

void status_time(void)
{
    char clock[8];
    *put_time(clock, clock_now()) = 0;
    bar_print(BAR_LEFT, BAR_RIGHT, clock);
}

void status_note(const char *s)
{
    *put_str(status_text, s) = 0;
    bar_print(0, BAR_LEFT, status_text);
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
    p = put_best(p, best_time(k));
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

/* --- pause --------------------------------------------------------------- */

static const char *const pause_items[PAUSE_ITEMS] = {
    "RESUME", "REVEAL CELL  +0:30", "START OVER", 0, "QUIT TO MENU"
};

void panel_pause_music(void)
{
    text_win(2, 2 + PAUSE_MUSIC, music_enabled() ? "MUSIC: ON " : "MUSIC: OFF");
}

void panel_pause(void)
{
    uint8_t k;
    fill_win_rect(0, 1, 20, 17, SPACE);
    for (k = 0; k < PAUSE_ITEMS; k++)
        if (pause_items[k])
            text_win(2, 2 + k, pause_items[k]);
    panel_pause_music();
    panel_pause_choice(PAUSE_RESUME);
    panel_slide(144 - (PAUSE_ITEMS + 3) * 8);
}

void panel_pause_choice(uint8_t choice)
{
    uint8_t k;
    for (k = 0; k < PAUSE_ITEMS; k++)
        text_win(0, 2 + k, k == choice ? ">" : " ");
}

/* --- quitting ----------------------------------------------------------- */

#define QUIT_Y 7                            /* the first choice's row */

void panel_quit(void)
{
    fill_win_rect(0, 1, 20, 17, SPACE);
    text_win(2, 2, "QUIT THIS PUZZLE?");
    text_win(2, 4, "NOTHING IS KEPT:");
    text_win(2, 5, "YOUR DIGITS AND");
    text_win(2, 6, "TIME ARE LOST.");
    text_win(2, QUIT_Y + 1, "KEEP PLAYING");
    text_win(2, QUIT_Y + 2, "QUIT PUZZLE");
    panel_quit_choice(QUIT_KEEP);
    panel_slide(144 - (QUIT_Y + 4) * 8);
}

void panel_quit_choice(uint8_t choice)
{
    text_win(0, QUIT_Y + 1, choice == QUIT_KEEP ? ">" : " ");
    text_win(0, QUIT_Y + 2, choice == QUIT_YES ? ">" : " ");
}

/* --- the overview after a solved puzzle ---------------------------------- */

/* A white card in a black frame (its first column black, for wave.c): the
 * time, the solved count and a line per group with puzzles, from CARD_X
 * on. */
#define CARD_X 2
#if GROUP_COUNT > 6
#error "the overview has room for 6 groups"
#endif

static void card_line(uint8_t y, char *p)
{
    *p = 0;
    text_win(CARD_X, y, text);
}

void panel_overview(uint8_t k, uint16_t seconds, uint16_t best, uint8_t record)
{
    uint8_t g, y;
    char *p;
    fill_win_rect(0, 0, 20, 18, T_BLACK);
    fill_win_rect(1, 1, 18, 16, SPACE);
    text_win(6, 2, "SOLVED!");
    p = put_str(text, "PUZZLE ");
    card_line(4, put_num(p, k + 1, 0));
    p = put_str(text, "TIME     ");
    card_line(5, put_time(p, seconds));
    if (record)
        p = put_str(text, "NEW BEST TIME!");
    else {
        p = put_str(text, "BEST     ");
        p = put_best(p, best);
    }
    card_line(6, p);
    p = put_str(text, "SOLVED     ");
    p = put_num(p, best_solved(0, PUZZLE_COUNT), 2);
    *p++ = '/';
    card_line(8, put_num(p, PUZZLE_COUNT, 2));
    for (g = 0, y = 9; g < GROUP_COUNT; g++) {
        if (!group_count[g])                /* none yet */
            continue;
        if (g == group)
            text_win(1, y, ">");
        p = put_size(text, group_n[g]);
        *p++ = ' ';
        p = put_stars(p, group_stars[g]);
        while (p < text + 11)
            *p++ = ' ';
        p = put_num(p, best_solved(group_first[g], group_count[g]), 2);
        *p++ = '/';
        card_line(y++, put_num(p, group_count[g], 2));
    }
    text_win(CARD_X, 15, "A: NEXT PUZZLE");
    text_win(CARD_X, 16, "B: CHOOSE");
    wy = wy_target = PANEL_FULL;
}
