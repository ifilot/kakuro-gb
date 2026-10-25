/* SPDX-License-Identifier: GPL-3.0-only */
/* save.c -- the layout of the battery RAM:
 *
 *   0xA000  signature "KAKURO" and a format number
 *   0xA008  last puzzle played (NO_PUZZLE: none)
 *   0xA010  best times, 2 bytes per puzzle (little endian, 0 = none)
 *   0xA200  progress, RECORD bytes per puzzle: started flag, clock (2
 *           bytes), the digits of the inner grid as nibbles (high first)
 */
#include <gb/gb.h>
#include <string.h>
#include "save.h"
#include "puzzle.h"
#include "puzzles.h"

#define SRAM      ((uint8_t *)0xA000)
#define SIG_LEN   7
#define LAST      (SRAM + 0x08)
#define BEST      (SRAM + 0x10)
#define PROGRESS  (SRAM + 0x200)
#define RECORD    (3 + ((MAX_N - 1) * (MAX_N - 1) + 1) / 2)

static const char signature[SIG_LEN] = { 'K', 'A', 'K', 'U', 'R', 'O', 1 };

#define record(k) (PROGRESS + (uint16_t)(k) * RECORD)

void save_init(void)
{
    ENABLE_RAM;
    if (memcmp(SRAM, signature, SIG_LEN)) {
        memset(SRAM, 0, 0x200 + (uint16_t)PUZZLE_COUNT * RECORD);
        memcpy(SRAM, signature, SIG_LEN);
        *LAST = NO_PUZZLE;
    }
    DISABLE_RAM;
}

uint8_t save_last(void)
{
    uint8_t k;
    ENABLE_RAM;
    k = *LAST;
    DISABLE_RAM;
    return k < PUZZLE_COUNT ? k : NO_PUZZLE;
}

void save_set_last(uint8_t k)
{
    ENABLE_RAM;
    *LAST = k;
    DISABLE_RAM;
}

uint16_t save_best(uint8_t k)
{
    uint16_t s;
    ENABLE_RAM;
    s = BEST[2 * k] | (uint16_t)BEST[2 * k + 1] << 8;
    DISABLE_RAM;
    return s;
}

void save_set_best(uint8_t k, uint16_t seconds)
{
    ENABLE_RAM;
    BEST[2 * k] = (uint8_t)seconds;
    BEST[2 * k + 1] = (uint8_t)(seconds >> 8);
    DISABLE_RAM;
}

uint8_t save_started(uint8_t k)
{
    uint8_t s;
    ENABLE_RAM;
    s = record(k)[0];
    DISABLE_RAM;
    return s;
}

uint16_t save_restore(uint8_t k)
{
    uint8_t *rec = record(k);
    uint8_t i, j, r, c, d;
    uint16_t seconds = 0;
    ENABLE_RAM;
    if (rec[0]) {
        seconds = rec[1] | (uint16_t)rec[2] << 8;
        j = 0;
        i = n + 1;
        for (r = 1; r < n; r++, i++)
            for (c = 1; c < n; c++, i++, j++) {
                if ((cell[i] & (CELL_WHITE | CELL_FIXED)) != CELL_WHITE)
                    continue;
                d = (j & 1) ? rec[3 + (j >> 1)] & 0x0F : rec[3 + (j >> 1)] >> 4;
                if (d >= 1 && d <= 9) {
                    cell[i] = CELL_WHITE | d;
                    empty--;
                }
            }
    }
    DISABLE_RAM;
    return seconds;
}

void save_cell(uint8_t k, uint8_t i, uint8_t digit)
{
    uint8_t *rec = record(k);
    uint8_t j = inner_index(i);
    uint8_t *b = rec + 3 + (j >> 1);
    ENABLE_RAM;
    rec[0] = 1;
    *b = (j & 1) ? (*b & 0xF0) | digit : (*b & 0x0F) | (uint8_t)(digit << 4);
    DISABLE_RAM;
}

void save_time(uint8_t k, uint16_t seconds)
{
    uint8_t *rec = record(k);
    ENABLE_RAM;
    rec[1] = (uint8_t)seconds;
    rec[2] = (uint8_t)(seconds >> 8);
    DISABLE_RAM;
}

void save_clear(uint8_t k)
{
    ENABLE_RAM;
    memset(record(k), 0, RECORD);
    DISABLE_RAM;
}
