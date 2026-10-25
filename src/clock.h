/* SPDX-License-Identifier: GPL-3.0-only */
/* clock.h -- the game clock, in whole seconds, from the timer interrupt
 * (4096 Hz / 256 = 16 interrupts a second). */
#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

#define CLOCK_MAX 35999u                    /* 9:59:59 */

extern volatile uint16_t clock_seconds;
extern volatile uint8_t clock_running;
extern volatile uint8_t clock_changed;      /* set every second; the reader clears it */

void clock_init(void);
void clock_start(uint16_t seconds);         /* runs from `seconds` */
uint16_t clock_now(void);                   /* clock_seconds, read with interrupts off */
void clock_add(uint16_t seconds);           /* up to CLOCK_MAX */

#endif
