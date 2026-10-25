/* SPDX-License-Identifier: GPL-3.0-only */
/* video.h -- tile loading, text on the window, string building, fades, and
 * the Game Boy Color's palettes.
 *
 * The background and window take their tile data from 0x8800-0x97FF
 * (indices 0-127 at 0x9000, 128-255 at 0x8800), the sprites from
 * 0x8000-0x87FF, so the two never share tiles. The background map is at
 * 0x9800, the window map at 0x9C00.
 *
 * On the Color every tile of the maps has a palette of its own (the
 * attribute map, VRAM bank 1); the game's look there is "paper and ink":
 * navy lines on cream, clue triangles in terracotta, entered digits in blue
 * ink. The DMG's shades map to a palette's four colours as they are. */
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

/* Background palettes on the Color; 0 is in every attribute until set. */
enum { CPAL_PAPER, CPAL_CLUE, CPAL_CELL, CPAL_GIVEN };
/* Sprite palettes on the Color (OAM attribute bits 0-2; the DMG ignores
 * them, as the Color ignores bit 4, the DMG's OBP1). */
enum { CPAL_CURSOR, CPAL_KEY, CPAL_KEY_CHOSEN };

extern uint8_t cgb;                         /* running on a Color */

void video_init(void);                      /* at start-up, before the screen is on */

/* The background palettes the next screen_on() fades in (count of them, 4
 * colours each, up to 8); 0 for the game's own (CPAL_PAPER...). */
void video_palettes(const uint16_t *colours, uint8_t count);

/* The palette of map tiles on the Color; nothing on the DMG. */
void attr_bkg(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t palette);
void attr_tiles(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *palettes);

/* Loads `count` background tiles at index `first` (signed addressing). */
void vram_tiles(uint8_t first, uint8_t count, const uint8_t *data);

/* The same at the next frame(), at the start of the vertical blank, where
 * the copy need not wait for the LCD; a second request for `first` replaces
 * the first. `data` must stay valid until then. */
void vram_tiles_later(uint8_t first, uint8_t count, const uint8_t *data);

/* Fades to white and switches the screen off / on and back from white. */
void screen_off(void);
void screen_on(void);

/* Writes a string of ASCII 32..95 (at most 20 characters) at a window map
 * position. */
void text_win(uint8_t x, uint8_t y, const char *s);

/* String building: each writes at p and returns the end (not terminated). */
char *put_str(char *p, const char *s);
char *put_num(char *p, uint16_t v, uint8_t width);  /* right-aligned in width (0: as long as needed) */
char *put_time(char *p, uint16_t seconds);           /* h:mm:ss */
char *put_best(char *p, uint16_t seconds);           /* h:mm:ss, or -:--:-- for 0 (none) */
char *put_size(char *p, uint8_t n);                  /* "8X8" for the grid size n (with the sums) */
char *put_stars(char *p, uint8_t stars);         /* out of three: '^' a star, '@' a missing one (dim) */

/* Waits for the next frame and loads the queued tiles. */
void frame(void);

#endif
