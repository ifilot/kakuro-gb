/* SPDX-License-Identifier: GPL-3.0-only */
/* panel.c -- the window layer. */
#include <gb/gb.h>
#include <string.h>
#include "panel.h"
#include "board.h"
#include "canvas.h"
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
static char text[32];
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

/* --- drawing in the 3x5 font --------------------------------------------- */

static const char three_stars[] = "^^^";

/* The difficulty out of three stars, the missing ones light. */
static uint8_t tiny_stars(uint8_t x, uint8_t y, uint8_t count)
{
    canvas_tiny(x, y, three_stars, C_LIGHT);
    canvas_tiny(x, y, three_stars + 3 - count, C_DARK);
    return x + canvas_tiny_width(three_stars);
}

/* --- combinations -------------------------------------------------------- */

/* A panel over the board, in the 3x5 font: the puzzle at the top, per run
 * its sum and its combinations as strips of cells, a tile each (digits
 * already in the run dark; a combination without all of them light), and
 * the digits the cursor's cell can take under them. The panel slides up
 * white at once; once it covers the board, panel_info_draw() puts the cells
 * (chip_tiles) and canvases for the texts in the font's tiles (the puzzle)
 * and the board's, and panel_info_close() loads those back. */
#define T_INFO_TOP   (T_FONT + 1)           /* 40 tiles: the puzzle */
#define T_INFO_LINES T_BLACK                /* 20 a line: the runs' headings, the possible digits */
#define T_INFO_CHIPS (T_INFO_LINES + 60)    /* 27: the cells */
#define INFO_RUN_Y   4                      /* the first run's window row */
#define INFO_RIGHT   152                    /* texts end before this dot */
enum { CHIP_FIT, CHIP_PLACED = 9, CHIP_DIM = 18, CHIP_RULE = 27 };   /* chip_tiles: digit d at kind + d - 1 */

/* What the panel shows, per run (0 across, 1 down); not static: the tests
 * read them. */
uint8_t info_count[2];                      /* combinations, NO_RUN without a sum */
uint8_t info_sum[2], info_len[2];
uint16_t info_masks[2][MAX_COMBOS];         /* bit d: digit d */
uint16_t info_dim[2];                       /* bit k: combination k lacks a digit already in the run */
uint16_t info_placed[2];                    /* the digits in the run */
uint16_t info_fits;                         /* the digits the cell can take, 0 for a given one */
static uint8_t info_puzzle, drawn, run_rows[2];

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

/* Run r (0 across, 1 down) into info_*; returns the digits its combinations
 * allow the cursor's cell (all of them without a run). */
static uint16_t run_info(uint8_t r)
{
    uint8_t head = r ? head_d[cur] : head_a[cur], step = r ? n : 1, len, k;
    uint16_t other, m, fits = 0;
    info_count[r] = NO_RUN;
    if (head == NO_RUN)
        return 0x3FE;
    info_len[r] = len = r ? len_d[head] : len_a[head];
    info_sum[r] = r ? sum_d[head] : sum_a[head];
    info_placed[r] = run_digits(head, step, len);
    other = others(head, step, len, cur);
    info_count[r] = combinations(info_sum[r], len, info_masks[r]);
    info_dim[r] = 0;
    for (k = 0; k < info_count[r]; k++) {
        m = info_masks[r][k];
        if ((m & other) == other)
            fits |= m & ~other;
        if ((m & info_placed[r]) != info_placed[r])
            info_dim[r] |= 1u << k;
    }
    return fits;
}

/* The tile rows of run r: its heading, then its strips from column 1, a
 * column between them (with ceil, as in run_strips). */
static uint8_t run_height(uint8_t r)
{
    uint8_t rows = 1, x = 1, k;
    if (info_count[r] == NO_RUN)
        return 1;
    for (k = 0; k < info_count[r]; k++) {
        if (x + info_len[r] > 20) {
            rows++;
            x = 1;
        }
        x += info_len[r] + 1;
    }
    return rows + 1;
}

void panel_info(uint8_t k)
{
    info_puzzle = k;
    info_fits = run_info(0) & run_info(1);
    if (cell[cur] & CELL_FIXED)
        info_fits = 0;
    run_rows[0] = run_height(0);
    run_rows[1] = run_height(1);
    drawn = 0;
    fill_win_rect(0, 1, 20, 17, SPACE);     /* it slides up white, at once */
    panel_slide(PANEL_FULL);
}

/* Run r's heading on a canvas from tile `first` at window row y: the arrow,
 * the sum in black as on the board, its length, the combinations left. */
static void run_heading(uint8_t r, uint8_t first, uint8_t y)
{
    uint8_t count = info_count[r], x, w, k, fit;
    char *p;
    canvas_begin(20, 1, C_WHITE);
    if (count == NO_RUN)
        canvas_tiny(7, 2, r ? "\\ NO SUM DOWN" : "[ NO SUM ACROSS", C_LIGHT);
    else {
        text[0] = r ? '\\' : '[';
        text[1] = 0;
        x = canvas_tiny(7, 2, text, C_DARK);
        *put_num(text, info_sum[r], 0) = 0;
        w = canvas_tiny_width(text) + 3;
        canvas_fill(x + 1, 1, w, 7, C_BLACK);
        canvas_tiny(x + 2, 2, text, C_WHITE);
        p = put_str(text, "IN ");
        *put_num(p, info_len[r], 0) = 0;
        canvas_tiny(x + w + 3, 2, text, C_BLACK);
        for (fit = count, k = 0; k < count; k++)
            if (info_dim[r] & (1u << k))
                fit--;
        p = text;
        if (fit != count) {
            p = put_num(p, fit, 0);
            p = put_str(p, " OF ");
        }
        p = put_num(p, count, 0);
        p = put_str(p, count == 1 ? " WAY" : " WAYS");
        if (fit != count)
            p = put_str(p, " LEFT");
        *p = 0;
        canvas_tiny(INFO_RIGHT - canvas_tiny_width(text), 2, text, C_DARK);
    }
    canvas_show_win(first, 0, y);
}

/* A divider across window row y, within the texts' margins. */
static void divider(uint8_t y)
{
    fill_win_rect(1, y, 18, 1, (uint8_t)(T_INFO_CHIPS + CHIP_RULE));
}

/* Run r's combinations from window row y on, a cell tile per digit. */
static void run_strips(uint8_t r, uint8_t y)
{
    uint8_t row[20], x = 1, k, d, kind;
    uint16_t m;
    if (info_count[r] == NO_RUN)
        return;
    memset(row, SPACE, 20);
    for (k = 0; k < info_count[r]; k++) {
        if (x + info_len[r] > 20) {
            set_win_tiles(0, y++, 20, 1, row);
            memset(row, SPACE, 20);
            x = 1;
        }
        m = info_masks[r][k];
        for (d = 1; d <= 9; d++)
            if (m & (1u << d)) {
                kind = CHIP_FIT;
                if (info_dim[r] & (1u << k))
                    kind = CHIP_DIM;
                else if (info_placed[r] & (1u << d))
                    kind = CHIP_PLACED;
                row[x++] = T_INFO_CHIPS + kind + d - 1;
            }
        x++;
    }
    set_win_tiles(0, y, 20, 1, row);
}

void panel_info_draw(void)
{
    uint8_t x, d, y, row[9];
    char *p;
    if (drawn || panel_moving())
        return;
    drawn = 1;
    vram_tiles(T_INFO_CHIPS, CHIP_TILE_COUNT, chip_tiles);

    canvas_begin(20, 2, C_WHITE);           /* the puzzle */
    p = put_str(text, "PUZZLE ");
    *put_num(p, info_puzzle + 1, 0) = 0;
    x = canvas_text(8, 2, text, C_BLACK);
    *put_size(text, n) = 0;
    x = canvas_tiny(x + 4, 3, text, C_DARK);
    tiny_stars(x + 2, 3, stars);
    p = put_str(text, "BEST ");
    p = put_best(p, best_time(info_puzzle));
    p = put_str(p, "   ");
    p = put_num(p, empty, 0);
    *put_str(p, empty == 1 ? " CELL LEFT" : " CELLS LEFT") = 0;
    canvas_tiny(8, 11, text, C_DARK);
    canvas_show_win(T_INFO_TOP, 0, 1);

    y = INFO_RUN_Y;                         /* the runs, a divider before each part */
    divider(y - 1);
    run_heading(0, T_INFO_LINES, y);
    run_strips(0, y + 1);
    y += run_rows[0] + 1;
    divider(y - 1);
    run_heading(1, T_INFO_LINES + 20, y);
    run_strips(1, y + 1);
    y += run_rows[1] + 1;
    divider(y - 1);

    canvas_begin(20, 1, C_WHITE);           /* the digits the cell can take */
    canvas_tiny(INFO_RIGHT - canvas_tiny_width("B: BACK"), 2, "B: BACK", C_LIGHT);
    if (cell[cur] & CELL_FIXED)
        canvas_tiny(7, 2, "GIVEN DIGIT", C_DARK);
    else {
        canvas_tiny(7, 2, "POSSIBLE", C_DARK);
        if (!info_fits)
            canvas_tiny(48, 2, "NONE", C_DARK);
    }
    canvas_show_win(T_INFO_LINES + 40, 0, y);
    if (info_fits) {
        for (d = 1; d <= 9; d++) {          /* plain ifs: SDCC 4.5 has miscompiled ?: (menu.c) */
            x = T_INFO_CHIPS + CHIP_DIM + d - 1;
            if (info_fits & (1u << d))
                x = T_INFO_CHIPS + CHIP_FIT + d - 1;
            row[d - 1] = x;
        }
        set_win_tiles(6, y, 9, 1, row);
    }
}

void panel_info_close(void)
{
    fill_win_rect(0, 1, 20, 17, SPACE);     /* it slides away white */
    if (!drawn)
        return;
    drawn = 0;
    vram_tiles(T_INFO_TOP, T_BLACK - T_INFO_TOP, game_tiles + (uint16_t)T_INFO_TOP * 16);
    vram_tiles(T_INFO_LINES, 60 + CHIP_TILE_COUNT, game_tiles + (uint16_t)T_INFO_LINES * 16);
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

/* A white card in a black frame (its first column black, for wave.c), on
 * canvases in the 3x5 font: the title and the time in the font's tiles,
 * drawn before the wave; the solved counts with their bars in the board's,
 * once the wave has covered it (panel_overview_counts). The game loads the
 * board's tiles afresh afterwards, for the next puzzle or the menus. */
#define T_OVER_TITLE (T_FONT + 1)           /* 36 tiles */
#define T_OVER_TIME  (T_OVER_TITLE + 36)    /* 24 */
#define T_OVER_LINES (T_BLACK + 1)          /* 18 a line */
#define OVER_W       144                    /* the card's inside, in dots */
#define TIME_W       96                     /* the time's canvas, centred on it */
#if T_OVER_TIME + 24 > T_BLACK
#error "the overview's title and time do not fit in the font's tiles"
#endif
#if GROUP_COUNT > 6
#error "the overview has room for 6 groups"
#endif

void panel_overview(uint8_t k, uint16_t seconds, uint16_t best, uint8_t record)
{
    uint8_t x, w;
    char *p;
    fill_win_rect(0, 0, 20, 18, T_BLACK);
    fill_win_rect(1, 1, 18, 16, SPACE);

    canvas_begin(18, 2, C_WHITE);           /* the title, the puzzle */
    canvas_text(36, 1, "]", C_DARK);
    canvas_text(52, 1, "SOLVED!", C_BLACK);
    p = put_str(text, "PUZZLE ");
    p = put_num(p, k + 1, 0);
    p = put_str(p, "  ");
    *put_size(p, n) = 0;
    w = canvas_tiny_width(text);
    x = canvas_tiny((OVER_W - w - 14) / 2, 10, text, C_DARK);
    tiny_stars(x + 2, 10, stars);
    canvas_show_win(T_OVER_TITLE, 1, 2);

    canvas_begin(TIME_W / 8, 2, C_WHITE);   /* the time, a new best time */
    *put_time(text, seconds) = 0;
    canvas_text((TIME_W - 8 * 7) / 2, 0, text, C_BLACK);
    if (record) {
        w = canvas_tiny_width("NEW BEST TIME!");
        canvas_fill((TIME_W - w) / 2 - 3, 9, w + 5, 7, C_DARK);
        canvas_tiny((TIME_W - w) / 2, 10, "NEW BEST TIME!", C_WHITE);
    } else {
        p = put_str(text, "BEST ");
        *put_best(p, best) = 0;
        canvas_tiny((TIME_W - canvas_tiny_width(text)) / 2, 10, text, C_DARK);
    }
    canvas_show_win(T_OVER_TIME, 1 + (OVER_W - TIME_W) / 16, 5);
    wy = wy_target = PANEL_FULL;
}

/* A line of counts from tile `first` at window row y: group g (its size and
 * stars; the puzzle's own on a light band) or, for NO_RUN, all of them. */
static void count_line(uint8_t first, uint8_t y, uint8_t g, uint8_t solved, uint8_t count)
{
    uint8_t w, x1;
    char *p;
    canvas_begin(18, 1, C_WHITE);
    if (g == group) {
        canvas_fill(2, 0, (uint8_t)(OVER_W - 4), 8, C_LIGHT);
        canvas_tiny(3, 2, "[", C_DARK);
    }
    if (g == NO_RUN)
        canvas_tiny(12, 2, "SOLVED", C_BLACK);
    else {
        *put_size(text, group_n[g]) = 0;
        canvas_tiny(12, 2, text, C_BLACK);
        tiny_stars(27, 2, group_stars[g]);
    }
    p = put_num(text, solved, 0);
    *p++ = '/';
    *put_num(p, count, 0) = 0;
    w = canvas_tiny_width(text);
    x1 = OVER_W - 6 - w;
    canvas_tiny(x1, 2, text, C_BLACK);
    w = x1 - 4 - 50;                        /* the bar, from dot 50 */
    canvas_fill(50, 2, w, 5, C_BLACK);
    canvas_fill(51, 3, w - 2, 3, C_WHITE);
    canvas_fill(51, 3, (uint8_t)((uint16_t)(w - 2) * solved / count), 3, C_DARK);
    canvas_show_win(first, 1, y);
}

void panel_overview_counts(void)
{
    uint8_t g, y = 8, t = T_OVER_LINES;
    count_line(t, y++, NO_RUN, best_solved(0, PUZZLE_COUNT), PUZZLE_COUNT);
    for (g = 0; g < GROUP_COUNT; g++) {
        if (!group_count[g])                /* none yet */
            continue;
        t += 18;
        count_line(t, ++y, g, best_solved(group_first[g], group_count[g]), group_count[g]);
    }
    canvas_begin(18, 1, C_WHITE);
    canvas_tiny(4, 1, "A: NEXT PUZZLE", C_DARK);
    canvas_tiny(OVER_W - 6 - canvas_tiny_width("B: CHOOSE"), 1, "B: CHOOSE", C_DARK);
    canvas_show_win(t + 18, 1, 16);
}
