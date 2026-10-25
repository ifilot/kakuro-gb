/* SPDX-License-Identifier: GPL-3.0-only */
/* video.c -- tile loading, text, fades. */
#include "video.h"
#include "sound.h"

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

static const uint8_t fade_steps[] = { 0xE4, 0x90, 0x40, 0x00 };

void screen_off(void)
{
    uint8_t k, t;
    if (!(LCDC_REG & LCDCF_ON))
        return;
    for (k = 1; k < 4; k++) {
        BGP_REG = fade_steps[k];
        OBP0_REG = fade_steps[k] & 0xFC;
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

void text_bkg(uint8_t x, uint8_t y, const char *s)
{
    uint8_t k = to_tiles(s);
    if (k)
        set_bkg_tiles(x, y, k, 1, line);
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
    while (stars--)
        *p++ = '^';
    return p;
}

void frame(void)
{
    vsync();
    sound_update();
}
