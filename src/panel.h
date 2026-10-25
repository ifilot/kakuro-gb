/* SPDX-License-Identifier: GPL-3.0-only */
/* panel.h -- the window layer: the status line at the bottom of the screen,
 * the panels that slide up from it (combinations, pause menu) and the
 * overview after a solved puzzle.
 *
 * The status line is window row 0, in the 3x5 font of the sums; a panel is
 * drawn in the rows below it and the window slides up until it shows. */
#ifndef PANEL_H
#define PANEL_H

#include <stdint.h>

#define PANEL_CLOSED 136                    /* window position: the status line only */
#define PANEL_FULL   0
#define PANEL_HIDDEN 144                    /* out of sight */

#define PAUSE_ITEMS 5
enum { PAUSE_RESUME, PAUSE_HINT, PAUSE_RESTART, PAUSE_MUSIC, PAUSE_QUIT };

void panel_setup(void);                     /* empty window, status line, closed */
void panel_slide(uint8_t wy);               /* sets the position the window slides to */
void panel_update(void);                    /* every frame */
uint8_t panel_moving(void);

extern char status_text[16];                /* the status line's text left of the clock */

void status_draw(void);                     /* sums of the cursor's runs */
void status_time(void);                     /* the clock */
void status_note(const char *text);         /* replaces the sums */

/* Puzzle k and the combinations of the cursor's runs, over the board: call
 * panel_info_draw() every frame while it is open (it draws the panel once it
 * covers the board) and panel_info_close() before it slides away. */
void panel_info(uint8_t k);
void panel_info_draw(void);
void panel_info_close(void);
void panel_pause(void);
void panel_pause_choice(uint8_t choice);
void panel_pause_music(void);               /* the music item, after it changed */

/* QUIT TO MENU asks first: the puzzle is not kept. */
enum { QUIT_KEEP, QUIT_YES };
void panel_quit(void);
void panel_quit_choice(uint8_t choice);
/* Puzzle k solved: its time, the best time before and whether this is a
 * new one, and how many of each group are solved, on a card filling the
 * window. Draw it with the window out of sight; wave_in() brings it in. */
void panel_overview(uint8_t k, uint16_t seconds, uint16_t best, uint8_t record);
void panel_overview_counts(void);           /* the rest, once wave_in() has covered the board */

#endif
