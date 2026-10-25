/* SPDX-License-Identifier: GPL-3.0-only */
/* canvas.c -- drawing into tiles of its own.
 *
 * The canvas is kept as its tiles, row by row: dot (x, y) is in tile
 * (y / 8) * w + x / 8, at bit 7 - x % 8 of that tile's row y % 8 (two
 * bytes, one per bit plane). Everything is drawn a row of up to 8 dots at
 * a time, as a mask, a tile's rows at a time: dot by dot, SDCC's code would
 * take frames for a row of cells. */
#include <gb/gb.h>
#include <string.h>
#include "canvas.h"
#include "gfx.h"
#include "video.h"

static uint8_t buf[CANVAS_TILES * 16];
static uint8_t cw, ch;                      /* in tiles */
static uint16_t row_skip;                   /* after a tile's last row: to the next tile row */
/* What a row gets: (byte & keep) | set_a, and set_b in the second plane.
 * Globals, as the small loops below read them: SDCC keeps the rest of
 * those in registers, a few times faster than with more arguments. */
static uint8_t keep, set_a, set_b;

static void use_shade(uint8_t shade, uint8_t m)
{
    keep = ~m;
    set_a = shade & 1 ? m : 0;
    set_b = shade & 2 ? m : 0;
}

/* n rows from p, within a tile */
static void rows_mask(uint8_t *p, uint8_t n)
{
    do {
        *p = (*p & keep) | set_a;
        p++;
        *p = (*p & keep) | set_b;
        p++;
    } while (--n);
}

static void rows_set(uint8_t *p, uint8_t n)
{
    do {
        *p++ = set_a;
        *p++ = set_b;
    } while (--n);
}

void canvas_begin(uint8_t w, uint8_t h, uint8_t bg)
{
    uint8_t r;
    cw = w;
    ch = h;
    row_skip = (uint16_t)w * 16 - 16;
    if (bg == C_WHITE || bg == C_BLACK) {
        memset(buf, bg ? 0xFF : 0, (uint16_t)w * h * 16);
        return;
    }
    use_shade(bg, 0xFF);
    for (r = 0; r < h; r++)
        rows_set(buf + (uint16_t)r * w * 16, w * 8);
}

static uint8_t *row_at(uint8_t x, uint8_t y)
{
    return buf + ((uint16_t)((y >> 3) * cw + (x >> 3)) << 4) + ((y & 7) << 1);
}

/* A tile row at a time, and in it a tile at a time, left to right. */
void canvas_fill(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t shade)
{
    uint8_t x1 = x + w - 1, y1 = y + h - 1, c0 = x >> 3, c1 = x1 >> 3, c, n, m, used;
    uint8_t first = 0xFF >> (x & 7), last = (uint8_t)(0xFF << (7 - (x1 & 7)));
    uint8_t *p;
    if (!w || !h)
        return;
    for (; y <= y1; y += n) {
        n = 8 - (y & 7);                    /* the rest of this tile row */
        if (n > y1 - y + 1)
            n = y1 - y + 1;
        p = row_at(x, y);
        used = 0;
        for (c = c0; c <= c1; c++, p += 16) {
            m = c == c0 ? first : 0xFF;
            if (c == c1)
                m &= last;
            if (m != used) {
                use_shade(shade, m);
                used = m;
            }
            if (m == 0xFF)
                rows_set(p, n);
            else
                rows_mask(p, n);
        }
    }
}

static uint8_t shade_a, shade_b;            /* the text's shade, per plane */

static void put_mask(uint8_t *p, uint8_t m)
{
    p[0] = (p[0] & ~m) | (m & shade_a);
    p[1] = (p[1] & ~m) | (m & shade_b);
}

/* Rows of up to 8 dots (the leftmost in the top bit), one every `step`
 * bytes from g, drawn from (x, y) down. */
static void glyph(uint8_t x, uint8_t y, const uint8_t *g, uint8_t step, uint8_t rows)
{
    uint8_t s = x & 7, two = s && (x >> 3) + 1 < cw, v;
    uint8_t *p = row_at(x, y);
    do {
        v = *g;
        g += step;
        if (v) {
            put_mask(p, v >> s);
            if (two)
                put_mask(p + 16, (uint8_t)(v << (8 - s)));
        }
        p += 2;
        if (!(++y & 7))
            p += row_skip;
    } while (--rows);
}

static void text_shade(uint8_t shade)
{
    shade_a = shade & 1 ? 0xFF : 0;
    shade_b = shade & 2 ? 0xFF : 0;
}

uint8_t canvas_text(uint8_t x, uint8_t y, const char *s, uint8_t shade)
{
    text_shade(shade);
    while (*s) {
        /* the font's tiles: black on white, so plane 0 is the glyph */
        glyph(x, y, game_tiles + ((uint16_t)(T_FONT + *s++ - 32) << 4), 2, 7);
        x += 8;
    }
    return x;
}

/* tiny_font: a width, then 5 rows with a dot of space before the glyph. A
 * glyph up to 3 dots wide takes 4, a wider one its width and a dot. */
static const uint8_t *tiny_glyph(char c)
{
    return tiny_font + (uint16_t)(uint8_t)(c - 32) * 6;
}

static uint8_t tiny_advance(uint8_t w)
{
    return w <= 3 ? 4 : w + 1;
}

uint8_t canvas_tiny(uint8_t x, uint8_t y, const char *s, uint8_t shade)
{
    const uint8_t *g;
    text_shade(shade);
    while (*s) {
        g = tiny_glyph(*s++);
        glyph(x, y, g + 1, 1, 5);
        x += tiny_advance(g[0]);
    }
    return x;
}

uint8_t canvas_tiny_width(const char *s)
{
    uint8_t w = 0;
    while (*s)
        w += tiny_advance(tiny_glyph(*s++)[0]);
    return w;
}

static void show(uint8_t first, uint8_t x, uint8_t y, uint8_t window)
{
    uint8_t row[20], r, c;
    vram_tiles(first, cw * ch, buf);
    for (r = 0; r < ch; r++) {
        for (c = 0; c < cw; c++)
            row[c] = first++;
        if (window)
            set_win_tiles(x, y + r, cw, 1, row);
        else
            set_bkg_tiles(x, y + r, cw, 1, row);
    }
}

void canvas_show(uint8_t first, uint8_t x, uint8_t y)
{
    show(first, x, y, 0);
}

void canvas_show_win(uint8_t first, uint8_t x, uint8_t y)
{
    show(first, x, y, 1);
}
