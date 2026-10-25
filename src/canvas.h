/* SPDX-License-Identifier: GPL-3.0-only */
/* canvas.h -- drawing into a rectangle of background tiles of its own, for
 * the menus: fills, the 5x7 font and the 3x5 font in any of the four
 * shades, anywhere. One canvas at a time: begin, draw, then show it. */
#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>

#define CANVAS_TILES 80                     /* the most tiles a canvas has */

enum { C_WHITE, C_LIGHT, C_DARK, C_BLACK };

/* A canvas w x h tiles, all shade bg. */
void canvas_begin(uint8_t w, uint8_t h, uint8_t bg);

/* In dots from the canvas's top-left corner. */
void canvas_fill(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t shade);
uint8_t canvas_text(uint8_t x, uint8_t y, const char *s, uint8_t shade);  /* 5x7, 8 dots a character */
uint8_t canvas_tiny(uint8_t x, uint8_t y, const char *s, uint8_t shade);  /* 3x5 */
uint8_t canvas_tiny_width(const char *s);
/* Both texts return the x after them; only the glyphs' dots are drawn. */

/* Loads the canvas into the tiles from `first` on (cw x ch of them) and puts
 * them at map tile (x, y). */
void canvas_show(uint8_t first, uint8_t x, uint8_t y);
void canvas_show_win(uint8_t first, uint8_t x, uint8_t y);  /* the same on the window */

#endif
