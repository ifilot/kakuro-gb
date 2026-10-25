/* SPDX-License-Identifier: GPL-3.0-only */
/* menu.h -- start screen, title and puzzle choice. Each returns the next
 * screen. */
#ifndef MENU_H
#define MENU_H

#include <stdint.h>

uint8_t splash_screen(void);                /* at power-on */
uint8_t title_screen(void);                 /* the logo and the groups */
uint8_t grid_screen(void);

#endif
