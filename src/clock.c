/* SPDX-License-Identifier: GPL-3.0-only */
/* clock.c -- the game clock from the timer interrupt. */
#include <gb/gb.h>
#include "clock.h"

volatile uint16_t clock_seconds;
volatile uint8_t clock_running;
volatile uint8_t clock_changed;
static volatile uint8_t ticks;

static void timer_isr(void)
{
    if (!clock_running)
        return;
    if (++ticks < 16)
        return;
    ticks = 0;
    if (clock_seconds < CLOCK_MAX)
        clock_seconds++;
    clock_changed = 1;
}

void clock_init(void)
{
    CRITICAL {
        add_TIM(timer_isr);
        TMA_REG = 0;
        TAC_REG = TACF_START | TACF_4KHZ;
    }
    set_interrupts(VBL_IFLAG | TIM_IFLAG);
}

void clock_start(uint16_t seconds)
{
    CRITICAL {
        clock_seconds = seconds;
        ticks = 0;
        clock_running = 1;
        clock_changed = 1;
    }
}

uint16_t clock_now(void)
{
    uint16_t s;
    CRITICAL {
        s = clock_seconds;
    }
    return s;
}

void clock_add(uint16_t seconds)
{
    CRITICAL {
        clock_seconds = clock_seconds < CLOCK_MAX - seconds ? clock_seconds + seconds : CLOCK_MAX;
    }
}
