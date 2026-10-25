/* SPDX-License-Identifier: GPL-3.0-only */
/* puzzle.h -- the puzzle: cells, sums, runs, the solved test and the
 * combinations of a sum.
 *
 * The grid is n x n (6..8) with the sums in row 0 and column 0; cells are
 * row-major (index = row * n + column). A black cell holds the sum of the
 * run of white cells to its right (across) and/or below it (down); a run of
 * a single white cell has no sum in that direction. */
#ifndef PUZZLE_H
#define PUZZLE_H

#include <stdint.h>

#define MAX_N     8
#define MAX_CELLS (MAX_N * MAX_N)
#define NO_RUN    0xFF

/* cell[] */
#define CELL_WHITE 0x80
#define CELL_FIXED 0x10                     /* a given digit */
#define CELL_DIGIT 0x0F                     /* the digit shown, 0 = empty */

#define MAX_COMBOS 12                       /* the most combinations of any sum */

extern uint8_t n, ncells, stars, group;
extern uint8_t cell[MAX_CELLS];
extern uint8_t solution[MAX_CELLS];                    /* white cells: the digit */
extern uint8_t sum_a[MAX_CELLS], sum_d[MAX_CELLS];     /* black cells: the sums (0: none) */
extern uint8_t len_a[MAX_CELLS], len_d[MAX_CELLS];     /* ... and their runs' lengths */
extern uint8_t head_a[MAX_CELLS], head_d[MAX_CELLS];   /* white cells: the black cell holding their sum, or NO_RUN */
extern uint8_t empty;                                  /* white cells without a digit */

/* The group (size and difficulty) of puzzle k. */
uint8_t puzzle_group(uint8_t k);

/* Loads puzzle k (0-based) with only the given digits filled in. */
void puzzle_load(uint8_t k);

/* Every white cell filled, every run's digits different and adding up to its sum? */
uint8_t puzzle_solved(void);

/* Digits (bit d for digit d) in the run after black cell head, step 1 or n. */
uint16_t run_digits(uint8_t head, uint8_t step, uint8_t len);

/* The combinations of len different digits 1-9 adding up to sum, as digit
 * masks (bit d for digit d) in increasing order; returns how many. */
uint8_t combinations(uint8_t sum, uint8_t len, uint16_t *out);

#endif
