/* SPDX-License-Identifier: GPL-3.0-only */
/* menu.h -- title screen, puzzle choice and the rules. Each returns the
 * next screen. */
#ifndef MENU_H
#define MENU_H

#include <stdint.h>

uint8_t title_screen(void);
uint8_t groups_screen(void);
uint8_t grid_screen(void);
uint8_t help_screen(void);

#endif
