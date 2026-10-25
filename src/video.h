/* SPDX-License-Identifier: GPL-3.0-only */
/* video.h -- tile loading, text on the background and window, fades.
 *
 * The background and window take their tile data from 0x8800-0x97FF
 * (indices 0-127 at 0x9000, 128-255 at 0x8800), the sprites from
 * 0x8000-0x87FF, so the two never share tiles. The background map is at
 * 0x9800, the window map at 0x9C00. */
#ifndef VIDEO_H
#define VIDEO_H

#include <gb/gb.h>
#include <stdint.h>

/* LCDC while the screen is on: everything enabled, see above. */
#define LCDC_GAME (LCDCF_ON | LCDCF_WIN9C00 | LCDCF_WINON | LCDCF_BG8800 | LCDCF_BG9800 | \
                   LCDCF_OBJ8 | LCDCF_OBJON | LCDCF_BGON)

#define PAL_NORMAL 0xE4                     /* shades 0-3: white .. black */
#define PAL_SPRITE 0xE0                     /* 0 transparent, 1 white, 2 dark grey, 3 black */
#define PAL_INVERT 0x2C                     /* the same with white and black swapped */

/* Loads `count` background tiles at index `first` (signed addressing). */
void vram_tiles(uint8_t first, uint8_t count, const uint8_t *data);

/* Fades to white and switches the screen off / on and back from white. */
void screen_off(void);
void screen_on(void);

/* Writes a string of ASCII 32..95 at a map position. */
void text_bkg(uint8_t x, uint8_t y, const char *s);
void text_win(uint8_t x, uint8_t y, const char *s);

/* String building: each writes at p and returns the end (not terminated). */
char *put_str(char *p, const char *s);
char *put_num(char *p, uint16_t v, uint8_t width);  /* right-aligned in width (0: as long as needed) */
char *put_time(char *p, uint16_t seconds);           /* h:mm:ss */
char *put_best(char *p, uint16_t seconds);           /* h:mm:ss, or -:--:-- for 0 (none) */
char *put_size(char *p, uint8_t n);                  /* "9X9" for the grid size n (with the sums) */
char *put_stars(char *p, uint8_t stars);

/* Waits for the next frame and runs the per-frame services (sound). */
void frame(void);

#endif
