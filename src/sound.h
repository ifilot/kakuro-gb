/* SPDX-License-Identifier: GPL-3.0-only */
/* sound.h -- short sound effects, the jingle for a solved puzzle and the
 * background music. */
#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

enum { SFX_MOVE, SFX_PLACE, SFX_ERASE, SFX_BUMP, SFX_ERROR, SFX_OPEN, SFX_SELECT, SFX_SOLVED };

void sound_init(void);                      /* before clock_init, which starts the timer */
void sfx(uint8_t effect);

void music_enable(uint8_t on);              /* on at the start */
uint8_t music_enabled(void);

#endif
