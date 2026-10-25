/* SPDX-License-Identifier: GPL-3.0-only */
/* board.h -- the puzzle on the background layer, the cursor and the keypad
 * as sprites.
 *
 * Every cell is 16x16 dots (2x2 tiles). The sum row and column show only
 * their lower and right halves, which hold the sums, so an n x n grid is
 * (2n-1) tiles square: up to 152 dots. The board is centred in the 160x136
 * dots above the status line; a 10x10 grid is taller and scrolls with the
 * cursor. */
#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#define VIEW_H 136                          /* dots above the status line */

extern uint8_t cur;                         /* the cursor's cell */

void board_setup(void);                     /* with the screen off: tiles, whole board, view */
void board_draw_cell(uint8_t i);
void board_update(void);                    /* every frame: cursor and view glide to their targets */
void board_snap(void);                      /* cursor and view straight to their targets */
void board_cursor(uint8_t visible);

/* The keypad: keys 1-9 and 10 (erase), next to the cursor. */
extern uint8_t key;
void keypad_open(uint8_t digit);
void keypad_close(void);
uint8_t keypad_move(uint8_t buttons);       /* J_UP etc.; returns whether the key changed */

#endif
