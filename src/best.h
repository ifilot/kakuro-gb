/* SPDX-License-Identifier: GPL-3.0-only */
/* best.h -- the best time of every puzzle, while the Game Boy is on.
 *
 * The cartridge is ROM only, without battery RAM: nothing is kept when it
 * is switched off, and a puzzle left unsolved is not kept at all. */
#ifndef BEST_H
#define BEST_H

#include <stdint.h>

void best_init(void);                       /* at power-on: nothing solved */
uint16_t best_time(uint8_t k);              /* seconds, 0 = not solved */
void best_set(uint8_t k, uint16_t seconds);
uint8_t best_solved(uint8_t first, uint8_t count);  /* how many of these puzzles are solved */

#endif
