/* SPDX-License-Identifier: GPL-3.0-only */
/* puzzle.c -- loading a puzzle, its sums and runs, and the rules.
 *
 * The puzzles come from puzzles.c (tools/gen_puzzles.py). The sums are
 * worked out from the solution when a puzzle is loaded; the puzzle is solved
 * as soon as every run obeys the rules (the generator made sure that only
 * one filling does). The solution is kept for hints.
 */
#include <string.h>
#include "puzzle.h"
#include "puzzles.h"

uint8_t n, ncells, stars, group;
uint8_t cell[MAX_CELLS];
uint8_t solution[MAX_CELLS];
uint8_t sum_a[MAX_CELLS], sum_d[MAX_CELLS];
uint8_t len_a[MAX_CELLS], len_d[MAX_CELLS];
uint8_t head_a[MAX_CELLS], head_d[MAX_CELLS];
uint8_t empty;

uint8_t puzzle_group(uint8_t k)
{
    uint8_t g = 0;
    while (g + 1 < GROUP_COUNT && k >= group_first[g + 1])
        g++;
    return g;
}

/* Walks the run that starts after black cell `head` in steps of `step`
 * (1 across, n down), at most `room` cells to the grid's edge: gives every
 * cell its head and returns the sum and, in *length, the number of cells. */
static uint8_t walk(uint8_t head, uint8_t step, uint8_t room, uint8_t *heads, uint8_t *length)
{
    uint8_t i = head, k = 0, total = 0;
    while (k < room) {
        i += step;
        if (!solution[i])
            break;
        total += solution[i];
        heads[i] = head;
        k++;
    }
    *length = k;
    return total;
}

void puzzle_load(uint8_t k)
{
    const uint8_t *p = puzzle_data + puzzle_offset[k];
    uint8_t g, j, r, c, i, v;

    group = g = puzzle_group(k);
    n = group_n[g];
    stars = group_stars[g];
    ncells = n * n;

    memset(cell, 0, MAX_CELLS);
    memset(solution, 0, MAX_CELLS);
    memset(sum_a, 0, MAX_CELLS);
    memset(sum_d, 0, MAX_CELLS);
    memset(len_a, 0, MAX_CELLS);
    memset(len_d, 0, MAX_CELLS);
    memset(head_a, NO_RUN, MAX_CELLS);
    memset(head_d, NO_RUN, MAX_CELLS);

    /* The inner grid, nibble j at cell i = r * n + c (r, c >= 1). */
    empty = 0;
    j = 0;
    i = n + 1;
    for (r = 1; r < n; r++, i++)
        for (c = 1; c < n; c++, i++, j++) {
            v = (j & 1) ? p[j >> 1] & 0x0F : p[j >> 1] >> 4;
            if (v) {
                solution[i] = v;
                cell[i] = CELL_WHITE;
                empty++;
            }
        }
    p += (j + 1) >> 1;
    for (j = *p++; j; j--) {
        v = *p++;
        i = (v / (n - 1) + 1) * n + v % (n - 1) + 1;
        cell[i] = CELL_WHITE | CELL_FIXED | solution[i];
        empty--;
    }

    i = 0;
    for (r = 0; r < n; r++)
        for (c = 0; c < n; c++, i++) {
            if (solution[i])
                continue;
            v = walk(i, 1, n - 1 - c, head_a, &len_a[i]);
            if (len_a[i] >= 2)
                sum_a[i] = v;
            else if (len_a[i])
                head_a[i + 1] = NO_RUN;      /* a lone cell: no sum across */
            v = walk(i, n, n - 1 - r, head_d, &len_d[i]);
            if (len_d[i] >= 2)
                sum_d[i] = v;
            else if (len_d[i])
                head_d[i + n] = NO_RUN;
        }
}

uint8_t inner_index(uint8_t i)
{
    return (i / n - 1) * (n - 1) + i % n - 1;
}

uint16_t run_digits(uint8_t head, uint8_t step, uint8_t len)
{
    uint16_t seen = 0;
    while (len--) {
        head += step;
        seen |= 1u << (cell[head] & CELL_DIGIT);
    }
    return seen & 0x3FE;
}

/* Does the run after `head` hold different digits adding up to `sum`? */
static uint8_t run_ok(uint8_t head, uint8_t step, uint8_t length, uint8_t sum)
{
    uint16_t seen = 0, bit;
    uint8_t total = 0, i = head;
    while (length--) {
        i += step;
        bit = 1u << (cell[i] & CELL_DIGIT);
        if (seen & bit)
            return 0;
        seen |= bit;
        total += cell[i] & CELL_DIGIT;
    }
    return total == sum;
}

uint8_t puzzle_solved(void)
{
    uint8_t i;
    if (empty)
        return 0;
    for (i = 0; i < ncells; i++) {
        if (sum_a[i] && !run_ok(i, 1, len_a[i], sum_a[i]))
            return 0;
        if (sum_d[i] && !run_ok(i, n, len_d[i], sum_d[i]))
            return 0;
    }
    return 1;
}

/* Depth-first in increasing order: digits from `from` on, `len` still to
 * choose, `sum` still to reach, `mask` the digits chosen so far. */
static uint16_t *write;
static uint8_t found;

static void choose(uint8_t from, uint8_t len, uint8_t sum, uint16_t mask)
{
    uint8_t d;
    if (len == 0) {
        if (!sum && found < MAX_COMBOS) {
            *write++ = mask;
            found++;
        }
        return;
    }
    for (d = from; d <= 9 && d <= sum; d++)
        choose(d + 1, len - 1, sum - d, mask | (1u << d));
}

uint8_t combinations(uint8_t sum, uint8_t len, uint16_t *out)
{
    write = out;
    found = 0;
    choose(1, len, sum, 0);
    return found;
}
