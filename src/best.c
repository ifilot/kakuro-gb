/* SPDX-License-Identifier: GPL-3.0-only */
/* best.c -- the best times, in work RAM. */
#include <string.h>
#include "best.h"
#include "puzzles.h"

/* Not static: the tests read it. */
uint16_t best_times[PUZZLE_COUNT];

void best_init(void)
{
    memset(best_times, 0, sizeof best_times);
}

uint16_t best_time(uint8_t k)
{
    return best_times[k];
}

void best_set(uint8_t k, uint16_t seconds)
{
    best_times[k] = seconds;
}

uint8_t best_solved(uint8_t first, uint8_t count)
{
    uint8_t solved = 0;
    for (; count; count--, first++)
        if (best_times[first])
            solved++;
    return solved;
}
