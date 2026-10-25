/* SPDX-License-Identifier: GPL-3.0-only */
/* video.c -- tile loading, text, fades, the Color's palettes. */
#include <gb/cgb.h>
#include "video.h"

uint8_t cgb;

#define NAVY  RGBHTML(0x1E2440)
#define CREAM RGBHTML(0xF8F0DC)
#define SAND  RGBHTML(0xE6D4A8)
#define PEACH RGBHTML(0xF0B070)
#define TERRA RGBHTML(0xD8643C)
#define INK   RGBHTML(0x2F5FB4)
#define BROWN RGBHTML(0x3A2E28)
#define RED   RGBHTML(0xE8402C)

/* Per palette the colours of shades 0-3 (white .. black on the DMG). */
static const palette_color_t bkg_colours[] = {
    CREAM, SAND, TERRA, NAVY,               /* CPAL_PAPER: text, menus, black */
    CREAM, PEACH, TERRA, NAVY,              /* CPAL_CLUE: sums, the logo */
    CREAM, SAND, INK, NAVY,                 /* CPAL_CELL: the digit (dark grey) in ink */
    CREAM, SAND, BROWN, NAVY,               /* CPAL_GIVEN: on sand (light grey) */
};
static const palette_color_t sprite_colours[] = {
    0, CREAM, RED, NAVY,                    /* CPAL_CURSOR: the brackets dark grey */
    0, CREAM, NAVY, NAVY,                   /* CPAL_KEY: white key, its line and digit */
    0, TERRA, NAVY, CREAM,                  /* CPAL_KEY_CHOSEN */
};
#define BKG_PALETTES    (sizeof bkg_colours / sizeof bkg_colours[0] / 4)
#define SPRITE_PALETTES (sizeof sprite_colours / sizeof sprite_colours[0] / 4)

static const palette_color_t *bkg_set = bkg_colours;   /* the screen's, video_palettes() */
static uint8_t bkg_count = BKG_PALETTES;

/* The palettes a part of the way (step of 3) to white, as the DMG's fade. */
static void cgb_fade(uint8_t step)
{
    static palette_color_t faded[4 * 8];
    const palette_color_t *from;
    uint8_t k, n, r, g, b;
    for (n = 0; n < 2; n++) {
        from = n ? sprite_colours : bkg_set;
        for (k = 0; k < (n ? 4 * SPRITE_PALETTES : 4 * bkg_count); k++) {
            r = from[k] & 0x1F;
            g = (from[k] >> 5) & 0x1F;
            b = (from[k] >> 10) & 0x1F;
            r += (uint8_t)((31 - r) * step / 3);
            g += (uint8_t)((31 - g) * step / 3);
            b += (uint8_t)((31 - b) * step / 3);
            faded[k] = r | (uint16_t)g << 5 | (uint16_t)b << 10;
        }
        if (n)
            set_sprite_palette(0, SPRITE_PALETTES, faded);
        else
            set_bkg_palette(0, bkg_count, faded);
    }
}

void video_palettes(const uint16_t *colours, uint8_t count)
{
    bkg_set = colours ? colours : bkg_colours;
    bkg_count = colours ? count : BKG_PALETTES;
}

void video_init(void)
{
    cgb = _cpu == CGB_TYPE;
    if (!cgb)
        return;
    cgb_fade(3);
    VBK_REG = VBK_BANK_1;                   /* both maps' attributes: palette 0 */
    fill_bkg_rect(0, 0, 32, 32, CPAL_PAPER);
    fill_win_rect(0, 0, 32, 32, CPAL_PAPER);
    VBK_REG = VBK_BANK_0;
}

void attr_bkg(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t palette)
{
    if (!cgb)
        return;
    VBK_REG = VBK_BANK_1;
    fill_bkg_rect(x, y, w, h, palette);
    VBK_REG = VBK_BANK_0;
}

void attr_tiles(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *palettes)
{
    if (!cgb)
        return;
    VBK_REG = VBK_BANK_1;
    set_bkg_tiles(x, y, w, h, palettes);
    VBK_REG = VBK_BANK_0;
}

void vram_tiles(uint8_t first, uint8_t count, const uint8_t *data)
{
    uint16_t chunk;
    uint8_t *dst;
    while (count) {
        if (first < 128) {
            dst = (uint8_t *)(0x9000u + (uint16_t)first * 16);
            chunk = 128 - first;
        } else {
            dst = (uint8_t *)(0x8800u + (uint16_t)(first - 128) * 16);
            chunk = 256 - first;
        }
        if (chunk > count)
            chunk = count;
        set_data(dst, data, chunk * 16);
        data += chunk * 16;
        first += (uint8_t)chunk;
        count -= (uint8_t)chunk;
    }
}

#define LATER_MAX 4

static uint8_t later_first[LATER_MAX], later_count[LATER_MAX], later_n;
static const uint8_t *later_data[LATER_MAX];

void vram_tiles_later(uint8_t first, uint8_t count, const uint8_t *data)
{
    uint8_t k;
    for (k = 0; k < later_n && later_first[k] != first; k++)
        ;
    if (k == LATER_MAX) {                   /* full: load it now */
        vram_tiles(first, count, data);
        return;
    }
    if (k == later_n)
        later_n++;
    later_first[k] = first;
    later_count[k] = count;
    later_data[k] = data;
}

static const uint8_t fade_steps[] = { 0xE4, 0x90, 0x40, 0x00 };

void screen_off(void)
{
    uint8_t k, t;
    if (!(LCDC_REG & LCDCF_ON))
        return;
    for (k = 1; k < 4; k++) {
        BGP_REG = fade_steps[k];            /* also on the Color: the tests watch it */
        OBP0_REG = fade_steps[k] & 0xFC;
        if (cgb)
            cgb_fade(k);
        for (t = 0; t < 3; t++)
            frame();
    }
    DISPLAY_OFF;
}

void screen_on(void)
{
    uint8_t k, t;
    BGP_REG = OBP0_REG = 0;
    LCDC_REG = LCDC_GAME;
    for (k = 3; k-- > 0;) {
        for (t = 0; t < 3; t++)
            frame();
        BGP_REG = fade_steps[k];
        OBP0_REG = k ? fade_steps[k] & 0xFC : PAL_SPRITE;
        if (cgb)
            cgb_fade(k);
    }
}

static uint8_t line[20];

static uint8_t to_tiles(const char *s)
{
    uint8_t k = 0;
    while (*s && k < 20)
        line[k++] = (uint8_t)(*s++ - 32);
    return k;
}

void text_win(uint8_t x, uint8_t y, const char *s)
{
    uint8_t k = to_tiles(s);
    if (k)
        set_win_tiles(x, y, k, 1, line);
}

char *put_str(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    return p;
}

char *put_num(char *p, uint16_t v, uint8_t width)
{
    char digits[5];
    uint8_t k = 0;
    do {
        digits[k++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (width > k) {
        *p++ = ' ';
        width--;
    }
    while (k)
        *p++ = digits[--k];
    return p;
}

char *put_time(char *p, uint16_t seconds)
{
    uint8_t m, s;
    s = seconds % 60;
    seconds /= 60;
    m = seconds % 60;
    *p++ = (char)('0' + seconds / 60);
    *p++ = ':';
    *p++ = (char)('0' + m / 10);
    *p++ = (char)('0' + m % 10);
    *p++ = ':';
    *p++ = (char)('0' + s / 10);
    *p++ = (char)('0' + s % 10);
    return p;
}

char *put_best(char *p, uint16_t seconds)
{
    return seconds ? put_time(p, seconds) : put_str(p, "-:--:--");
}

char *put_size(char *p, uint8_t n)
{
    p = put_num(p, n, 0);
    *p++ = 'X';
    return put_num(p, n, 0);
}

char *put_stars(char *p, uint8_t stars)
{
    uint8_t k;
    for (k = 0; k < 3; k++)
        *p++ = k < stars ? '^' : '@';
    return p;
}

void frame(void)
{
    vsync();
    while (later_n) {
        later_n--;
        vram_tiles(later_first[later_n], later_count[later_n], later_data[later_n]);
    }
}
