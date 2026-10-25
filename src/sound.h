/* SPDX-License-Identifier: GPL-3.0-only */
/* sound.h -- short sound effects and the jingle for a solved puzzle. */
#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

enum { SFX_MOVE, SFX_PLACE, SFX_ERASE, SFX_BUMP, SFX_ERROR, SFX_OPEN, SFX_SELECT, SFX_SOLVED };

void sound_init(void);
void sfx(uint8_t effect);
void sound_update(void);                    /* once per frame: plays the jingle on */

#endif
