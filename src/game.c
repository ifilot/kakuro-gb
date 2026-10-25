/* SPDX-License-Identifier: GPL-3.0-only */
/* game.c -- the course of a puzzle.
 *
 * Buttons on the board: the D-pad moves the cursor to the nearest white
 * cell in that direction, A opens the keypad, B wipes the cell, SELECT shows
 * the combinations and START the pause menu. Every change is saved at once;
 * the clock is saved every second once the puzzle has been touched.
 */
#include <gb/gb.h>
#include "game.h"
#include "main.h"
#include "board.h"
#include "clock.h"
#include "input.h"
#include "panel.h"
#include "puzzle.h"
#include "puzzles.h"
#include "save.h"
#include "sound.h"
#include "video.h"

#define HINT_PENALTY 30                     /* seconds */
#define NOTE_FRAMES  150

enum { M_BOARD, M_KEYPAD, M_INFO, M_PAUSE, M_SOLVED };

static uint8_t state, touched, note_time, choice;

/* The first white cell without a digit, else the first one not given. */
static uint8_t first_open(void)
{
    uint8_t i;
    for (i = 0; i < ncells; i++)
        if (cell[i] == CELL_WHITE)
            return i;
    for (i = 0; i < ncells; i++)
        if ((cell[i] & (CELL_WHITE | CELL_FIXED)) == CELL_WHITE)
            return i;
    return 0;
}

static void start_puzzle(void)
{
    uint16_t seconds;
    screen_off();
    puzzle_load(current);
    seconds = save_restore(current);
    touched = save_started(current);
    save_set_last(current);
    cur = first_open();
    board_setup();
    panel_setup();
    status_draw();
    state = M_BOARD;
    note_time = 0;
    screen_on();
    clock_start(seconds);
}

static void note(const char *text)
{
    status_note(text);
    note_time = NOTE_FRAMES;
}

static void solved(void)
{
    uint16_t seconds, best;
    uint8_t record;
    clock_running = 0;
    seconds = clock_now();
    if (!seconds)
        seconds = 1;
    best = save_best(current);
    record = !best || seconds < best;
    if (record)
        save_set_best(current, seconds);
    save_clear(current);
    touched = 0;
    sfx(SFX_SOLVED);
    board_cursor(0);
    panel_solved(seconds, best, record);
    state = M_SOLVED;
}

/* A digit 1-9, or 0 to wipe the cell. */
static void put_digit(uint8_t d)
{
    uint8_t old = cell[cur] & CELL_DIGIT;
    if (cell[cur] & CELL_FIXED) {
        sfx(SFX_ERROR);
        return;
    }
    if (d == old) {
        sfx(SFX_BUMP);
        return;
    }
    if (!old)
        empty--;
    if (!d)
        empty++;
    cell[cur] = CELL_WHITE | d;
    board_draw_cell(cur);
    save_cell(current, cur, d);
    if (!touched) {
        touched = 1;
        save_time(current, clock_now());
    }
    if (!empty) {
        if (puzzle_solved()) {
            solved();
            return;
        }
        sfx(SFX_ERROR);
        note("NOT YET RIGHT");
        return;
    }
    sfx(d ? SFX_PLACE : SFX_ERASE);
}

static uint8_t white_at(int16_t r, int16_t c)
{
    return r >= 0 && r < n && c >= 0 && c < n && (cell[r * n + c] & CELL_WHITE);
}

/* The nearest white cell in a direction: straight ahead first, else the
 * closest one in the quarter plane before the cursor. */
static void move_dir(uint8_t dir)
{
    int16_t r = cur / n, c = cur % n, k, o, j, rr, cc;
    for (j = 0; j <= 2 * n; j++) {
        o = (j & 1) ? (j + 1) / 2 : -(j / 2);   /* 0, 1, -1, 2, -2, ... */
        for (k = 1; k < n; k++) {
            if (o > k || -o > k)
                continue;
            if (dir & J_UP) {
                rr = r - k; cc = c + o;
            } else if (dir & J_DOWN) {
                rr = r + k; cc = c + o;
            } else if (dir & J_LEFT) {
                rr = r + o; cc = c - k;
            } else {
                rr = r + o; cc = c + k;
            }
            if (white_at(rr, cc)) {
                cur = (uint8_t)(rr * n + cc);
                sfx(SFX_MOVE);
                if (!note_time)
                    status_draw();
                return;
            }
        }
    }
    sfx(SFX_BUMP);
}

static void restart(void)
{
    uint8_t i;
    for (i = 0; i < ncells; i++)
        if ((cell[i] & (CELL_WHITE | CELL_FIXED)) == CELL_WHITE && (cell[i] & CELL_DIGIT)) {
            cell[i] = CELL_WHITE;
            empty++;
            board_draw_cell(i);
        }
    save_clear(current);
    touched = 0;
    cur = first_open();
    clock_start(0);
    status_draw();
}

static void hint(void)
{
    uint8_t d = solution[cur];
    if ((cell[cur] & CELL_FIXED) || (cell[cur] & CELL_DIGIT) == d) {
        sfx(SFX_BUMP);
        return;
    }
    clock_add(HINT_PENALTY);
    put_digit(d);
}

static void close_panel(void)
{
    panel_slide(PANEL_CLOSED);
    board_cursor(1);
    clock_running = 1;
    state = M_BOARD;
}

uint8_t game_play(void)
{
    start_puzzle();
    input_flush();
    for (;;) {
        frame();
        input_update();
        board_update();
        panel_update();
        if (clock_changed) {
            clock_changed = 0;
            status_time();
            if (touched)
                save_time(current, clock_now());
        }
        if (note_time && !--note_time)
            status_draw();

        switch (state) {
        case M_BOARD:
            if (repeated & (J_UP | J_DOWN | J_LEFT | J_RIGHT))
                move_dir(repeated);
            else if (pressed & J_A) {
                if (cell[cur] & CELL_FIXED)
                    sfx(SFX_ERROR);
                else {
                    keypad_open(cell[cur] & CELL_DIGIT);
                    sfx(SFX_OPEN);
                    state = M_KEYPAD;
                }
            } else if (pressed & J_B)
                put_digit(0);
            else if (pressed & J_SELECT) {
                board_cursor(0);
                panel_info(current);
                sfx(SFX_OPEN);
                state = M_INFO;
            } else if (pressed & J_START) {
                clock_running = 0;
                board_cursor(0);
                panel_pause();
                choice = PAUSE_RESUME;
                sfx(SFX_OPEN);
                state = M_PAUSE;
            }
            break;

        case M_KEYPAD:
            if (repeated & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) {
                if (keypad_move(repeated))
                    sfx(SFX_MOVE);
            } else if (pressed & J_A) {
                keypad_close();
                state = M_BOARD;
                put_digit(key == 10 ? 0 : key);
            } else if (pressed & (J_B | J_START | J_SELECT)) {
                keypad_close();
                state = M_BOARD;
            }
            break;

        case M_INFO:
            if (pressed & (J_SELECT | J_B | J_A | J_START))
                close_panel();
            break;

        case M_PAUSE:
            if (repeated & (J_UP | J_DOWN)) {
                choice = (choice + (repeated & J_UP ? PAUSE_ITEMS - 1 : 1)) % PAUSE_ITEMS;
                panel_pause_choice(choice);
                sfx(SFX_MOVE);
            } else if (pressed & (J_B | J_START))
                close_panel();
            else if (pressed & J_A) {
                sfx(SFX_SELECT);
                switch (choice) {
                case PAUSE_RESUME:
                    close_panel();
                    break;
                case PAUSE_HINT:
                    close_panel();
                    hint();
                    break;
                case PAUSE_RESTART:
                    close_panel();
                    restart();
                    break;
                case PAUSE_QUIT:
                    clock_running = 0;
                    if (touched)
                        save_time(current, clock_now());
                    return SCREEN_GRID;
                }
            }
            break;

        case M_SOLVED:
            if (panel_moving())
                break;
            if (pressed & (J_A | J_START)) {
                current = current + 1 < PUZZLE_COUNT ? current + 1 : 0;
                return SCREEN_GAME;
            }
            if (pressed & J_B)
                return SCREEN_GRID;
            break;
        }
    }
}
