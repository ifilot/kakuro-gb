/* SPDX-License-Identifier: GPL-3.0-only */
/* input.h -- the buttons, read once per frame. A direction held down
 * repeats: after 16 frames, then every 5. */
#ifndef INPUT_H
#define INPUT_H

#include <gb/gb.h>
#include <stdint.h>

extern uint8_t pressed;                     /* went down this frame */
extern uint8_t repeated;                    /* pressed, or a held direction repeating */

void input_update(void);
void input_flush(void);                     /* wait until every button is released */

#endif
