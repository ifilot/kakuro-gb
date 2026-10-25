/* SPDX-License-Identifier: GPL-3.0-only */
/* panel.h -- the window layer: the status line at the bottom of the screen
 * and the panels that slide up from it (combinations, pause menu, solved).
 *
 * The status line is window row 0; a panel is drawn in the rows below it and
 * the window slides up until it shows. */
#ifndef PANEL_H
#define PANEL_H

#include <stdint.h>

#define PANEL_CLOSED 136                    /* window position: the status line only */
#define PANEL_FULL   0

#define PAUSE_ITEMS 4
enum { PAUSE_RESUME, PAUSE_HINT, PAUSE_RESTART, PAUSE_QUIT };

void panel_setup(void);                     /* empty window, status line, closed */
void panel_slide(uint8_t wy);               /* sets the position the window slides to */
void panel_update(void);                    /* every frame */
uint8_t panel_moving(void);

void status_draw(void);                     /* sums of the cursor's runs and the clock */
void status_time(void);
void status_note(const char *text);         /* replaces the sums */

void panel_info(uint8_t k);                 /* puzzle k, best time, combinations of the cursor's runs */
void panel_pause(void);
void panel_pause_choice(uint8_t choice);
void panel_solved(uint16_t seconds, uint16_t best, uint8_t record);

#endif
