/* SPDX-License-Identifier: GPL-3.0-only */
/* sound.c -- square-wave blips on channel 1, noise on channel 4, and the
 * jingle on channel 2, one note per entry of a table, advanced every frame.
 *
 * A frequency value x plays 131072 / (2048 - x) Hz. */
#include <gb/gb.h>
#include "sound.h"

#define NOTE_C5 1798
#define NOTE_E5 1849
#define NOTE_C6 1923
#define NOTE_E6 1949
#define NOTE_G6 1964
#define NOTE_C7 1985

/* Jingle: frequency (0 a rest) and length in frames. */
static const uint16_t jingle[] = {
    NOTE_C6, 6, NOTE_E6, 6, NOTE_G6, 6, NOTE_C7, 6, 0, 6, NOTE_G6, 5, NOTE_C7, 20
};
#define JINGLE_END (sizeof jingle / sizeof jingle[0])
static uint8_t jingle_pos = JINGLE_END, jingle_wait;

void sound_init(void)
{
    NR52_REG = 0x80;                        /* sound on */
    NR50_REG = 0x77;                        /* full volume both sides */
    NR51_REG = 0xFF;                        /* every channel to both sides */
}

/* A blip on channel 1: frequency, duty (0-3), envelope (NR12), length (0-63). */
static void blip(uint16_t freq, uint8_t duty, uint8_t envelope, uint8_t length)
{
    NR10_REG = 0;
    NR11_REG = (uint8_t)(duty << 6) | (64 - length);
    NR12_REG = envelope;
    NR13_REG = (uint8_t)freq;
    NR14_REG = 0xC0 | (uint8_t)(freq >> 8);
}

static void note(uint16_t freq)
{
    NR21_REG = 0x80;
    NR22_REG = 0x82;
    NR23_REG = (uint8_t)freq;
    NR24_REG = 0x80 | (uint8_t)(freq >> 8);
}

void sfx(uint8_t effect)
{
    switch (effect) {
    case SFX_MOVE:   blip(NOTE_C6, 2, 0x41, 3); break;
    case SFX_PLACE:  blip(NOTE_G6, 2, 0x81, 6); break;
    case SFX_ERASE:  blip(NOTE_E5, 2, 0x61, 6); break;
    case SFX_OPEN:   blip(NOTE_E6, 1, 0x61, 4); break;
    case SFX_SELECT: blip(NOTE_C7, 1, 0x71, 6); break;
    case SFX_BUMP:   blip(NOTE_C5, 0, 0x51, 4); break;
    case SFX_ERROR:
        NR41_REG = 0x30;
        NR42_REG = 0xA2;
        NR43_REG = 0x63;
        NR44_REG = 0xC0;
        break;
    case SFX_SOLVED:
        jingle_pos = jingle_wait = 0;
        break;
    }
}

void sound_update(void)
{
    if (jingle_pos >= JINGLE_END)
        return;
    if (jingle_wait) {
        jingle_wait--;
        return;
    }
    if (jingle[jingle_pos])
        note(jingle[jingle_pos]);
    jingle_wait = (uint8_t)jingle[jingle_pos + 1] - 1;
    jingle_pos += 2;
}
