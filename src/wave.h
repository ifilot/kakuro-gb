/* SPDX-License-Identifier: GPL-3.0-only */
/* wave.h -- the transition from the board to a screen drawn in the window.
 *
 * Line by line the window pushes the board out to the left, each line a
 * little later or earlier than its neighbours, so the edge between them is
 * a wave. */
#ifndef WAVE_H
#define WAVE_H

/* The window map must hold the new screen with its first column black, and
 * the window must be out of sight (WY 144 or more). Returns once the window
 * covers the screen (WX 7, WY 0); sound goes on meanwhile. */
void wave_in(void);

#endif
