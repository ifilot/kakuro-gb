/* SPDX-License-Identifier: GPL-3.0-only */
/* input.c -- buttons with repeat for the directions. */
#include "input.h"
#include "video.h"

#define DPAD         (J_UP | J_DOWN | J_LEFT | J_RIGHT)
#define REPEAT_FIRST 16
#define REPEAT_NEXT  5

uint8_t pressed, repeated;
static uint8_t held, hold_time;

void input_update(void)
{
    uint8_t now = joypad();
    pressed = now & ~held;
    repeated = pressed;
    if (pressed & DPAD)
        hold_time = 0;
    else if (now & DPAD) {
        if (++hold_time >= REPEAT_FIRST) {
            hold_time = REPEAT_FIRST - REPEAT_NEXT;
            repeated |= now & DPAD;
        }
    }
    held = now;
}

void input_flush(void)
{
    while (joypad())
        frame();
    held = 0;
    pressed = repeated = 0;
}
