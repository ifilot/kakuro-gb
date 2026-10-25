/* SPDX-License-Identifier: GPL-3.0-only */
/* save.h -- best times and the progress of every puzzle in battery RAM.
 *
 * Cartridge RAM (MBC1, 8 KiB at 0xA000) holds a signature, the last puzzle
 * played, a best time per puzzle and, per puzzle, whether it is in progress,
 * its clock and the digits entered. RAM is enabled only while it is
 * accessed. */
#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>

#define NO_PUZZLE 0xFF

void save_init(void);                       /* clears the RAM if the signature is missing */

uint8_t save_last(void);                    /* the puzzle to continue, or NO_PUZZLE */
void save_set_last(uint8_t k);

uint16_t save_best(uint8_t k);              /* seconds, 0 = never solved */
void save_set_best(uint8_t k, uint16_t seconds);

uint8_t save_started(uint8_t k);            /* puzzle k has progress saved */

/* Puts the saved digits of puzzle k (loaded) into cell[] and returns its
 * clock; 0 if nothing was saved. */
uint16_t save_restore(uint8_t k);

void save_cell(uint8_t k, uint8_t i, uint8_t digit);  /* also marks the puzzle started */
void save_time(uint8_t k, uint16_t seconds);
void save_clear(uint8_t k);                 /* forgets the progress */

#endif
