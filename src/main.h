/* SPDX-License-Identifier: GPL-3.0-only */
/* main.h -- the screens and the puzzle being chosen or played. */
#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

enum { SCREEN_TITLE, SCREEN_GROUPS, SCREEN_GRID, SCREEN_HELP, SCREEN_GAME };

extern uint8_t current;                     /* the puzzle (0-based) */

#endif
