/* SPDX-License-Identifier: GPL-3.0-only */
/* wave.c -- the transition from the board to a screen in the window.
 *
 * Progress grows by SPEED every frame; line y has come wave_ease[progress -
 * wave_delay[y]] dots, clamped to 0..WAVE_WIDTH. An interrupt at the start
 * of every horizontal blank sets the next line's WX (the window's left edge)
 * and SCX (the board moved left by as much) from pos[]. The delays repeat
 * every WAVE_PERIOD lines, so pos[] holds one period. The frame's pos[] is
 * filled while the other one is shown.
 *
 * The window has a line counter of its own that only advances on lines
 * where it is drawn, so it is never quite hidden: on a line that has not
 * started its first dot shows at the right edge (WX 166). That dot is black,
 * like the margin of the board, since the new screen's first column is. */
#include <gb/gb.h>
#include <gb/isr.h>
#include "wave.h"
#include "gfx.h"
#include "video.h"

#define SPEED 6                             /* dots of progress per frame */
#define WX_IN 7                             /* WX of a window at the left edge */

static uint8_t pos[2][WAVE_LINES];
static const uint8_t *volatile line_pos;
static uint8_t scx0;

/* On line y's horizontal blank: line y + 1 (the last one sets line 144,
 * which is not shown). Called directly from the vector, so it is short. */
static void hblank_isr(void) CRITICAL INTERRUPT
{
    uint8_t y = LY_REG + 1, p;
    if (y >= WAVE_PERIOD)
        y -= WAVE_PERIOD;                   /* 144 becomes 72: pos[] has WAVE_PERIOD + 1 */
    p = line_pos[y];
    WX_REG = WX_IN + WAVE_WIDTH - p;
    SCX_REG = scx0 + p;
}
ISR_VECTOR(VECTOR_STAT, hblank_isr)

static void fill(uint8_t *out, uint8_t progress)
{
    const uint8_t *d = wave_delay;
    uint8_t k = WAVE_LINES, p;
    do {
        p = *d++;
        p = progress > p ? progress - p : 0;
        *out++ = wave_ease[p < WAVE_WIDTH ? p : WAVE_WIDTH];
    } while (--k);
}

/* Line 0 has no horizontal blank before it: set it in the vertical blank. */
static void first_line(const uint8_t *p)
{
    line_pos = p;
    WX_REG = WX_IN + WAVE_WIDTH - p[0];
    SCX_REG = scx0 + p[0];
}

void wave_in(void)
{
    uint8_t progress = 0, b = 0;
    scx0 = SCX_REG;
    fill(pos[0], 0);
    first_line(pos[0]);
    WY_REG = 0;
    CRITICAL {
        STAT_REG = STATF_MODE00;
        IF_REG &= ~LCD_IFLAG;
    }
    set_interrupts(IE_REG | LCD_IFLAG);
    while (progress < WAVE_END) {
        progress = progress < WAVE_END - SPEED ? progress + SPEED : WAVE_END;
        b ^= 1;
        fill(pos[b], progress);
        frame();
        first_line(pos[b]);
    }
    set_interrupts(IE_REG & ~LCD_IFLAG);
    STAT_REG = 0;
    WX_REG = WX_IN;
}
