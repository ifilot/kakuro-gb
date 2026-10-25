/* SPDX-License-Identifier: GPL-3.0-only */
/* game.h -- playing a puzzle. */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

/* Plays puzzle `current` (resuming its saved progress); returns the next
 * screen: SCREEN_GAME for the next puzzle (current is advanced) or
 * SCREEN_GRID. */
uint8_t game_play(void);

#endif
