/* SPDX-License-Identifier: GPL-3.0-only */
/* sound.c -- square-wave blips on channel 1, noise on channel 4, the music
 * (music.c) on channels 2 and 3, and the jingle on channel 2.
 *
 * The music and the jingle advance on the timer interrupt (TIMER_HZ), which
 * goes on while the screen is off; while the jingle plays the music waits.
 * A frequency value x plays 131072 / (2048 - x) Hz, on the wave channel
 * 65536 / (2048 - x) Hz. */
#include <gb/gb.h>
#include "sound.h"
#include "clock.h"
#include "music.h"

#define NOTE_C5 1798
#define NOTE_E5 1849
#define NOTE_C6 1923
#define NOTE_E6 1949
#define NOTE_G6 1964
#define NOTE_C7 1985

#define LEAD_DUTY     0x80                  /* NR21: half the time high */
#define LEAD_ENVELOPE 0x67                  /* NR22: volume 6, fading a step every 7/64 s */
#define BASS_VOLUME   0x40                  /* NR32: half */
#define WAVE_RAM      ((volatile uint8_t *)0xFF30)

/* The bass's wave: a triangle, 32 steps of 4 bits. */
static const uint8_t triangle[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
};

/* Jingle: frequency (0 a rest) and length in ticks. */
static const uint16_t jingle[] = {
    NOTE_C6, 6, NOTE_E6, 6, NOTE_G6, 6, NOTE_C7, 6, 0, 6, NOTE_G6, 5, NOTE_C7, 20
};
#define JINGLE_END (sizeof jingle / sizeof jingle[0])
static volatile uint8_t jingle_pos = JINGLE_END, jingle_wait;

typedef struct {
    const uint8_t *start, *at;              /* music.h: value (2 bytes), ticks */
    uint8_t wait;                           /* ticks until the next note */
} voice_t;

static voice_t lead = { music_lead, music_lead, 1 };
static voice_t bass = { music_bass, music_bass, 1 };
static volatile uint8_t music = 1;

static void lead_note(uint16_t x)
{
    if (!x) {
        NR22_REG = 0;                       /* off */
        return;
    }
    NR21_REG = LEAD_DUTY;
    NR22_REG = LEAD_ENVELOPE;
    NR23_REG = (uint8_t)x;
    NR24_REG = 0x80 | (uint8_t)(x >> 8);
}

static void bass_note(uint16_t x)
{
    /* Off first: on the DMG, starting the wave channel while it plays
     * garbles its wave. */
    NR30_REG = 0;
    if (!x)
        return;
    NR30_REG = 0x80;
    NR32_REG = BASS_VOLUME;
    NR33_REG = (uint8_t)x;
    NR34_REG = 0x80 | (uint8_t)(x >> 8);
}

/* A voice's next note, once its time has come. */
static void voice(voice_t *v, void (*play)(uint16_t))
{
    const uint8_t *at;
    if (--v->wait)
        return;
    at = v->at;
    if (!at[2])
        at = v->start;
    play(at[0] | (uint16_t)at[1] << 8);
    v->wait = at[2];
    v->at = at + 3;
}

static void sound_tick(void)
{
    /* The wave's line interrupt (wave.c) must not wait for the music. */
    enable_interrupts();
    if (jingle_pos < JINGLE_END) {
        if (jingle_wait) {
            jingle_wait--;
            return;
        }
        if (jingle[jingle_pos]) {
            NR21_REG = 0x80;
            NR22_REG = 0x82;
            NR23_REG = (uint8_t)jingle[jingle_pos];
            NR24_REG = 0x80 | (uint8_t)(jingle[jingle_pos] >> 8);
        }
        jingle_wait = (uint8_t)jingle[jingle_pos + 1] - 1;
        jingle_pos += 2;
        return;
    }
    if (music) {
        voice(&lead, lead_note);
        voice(&bass, bass_note);
    }
}

void sound_init(void)
{
    uint8_t k;
    NR52_REG = 0x80;                        /* sound on */
    NR50_REG = 0x77;                        /* full volume both sides */
    NR51_REG = 0xFF;                        /* every channel to both sides */
    NR30_REG = 0;
    for (k = 0; k < 16; k++)
        WAVE_RAM[k] = triangle[k];
    CRITICAL {
        add_TIM(sound_tick);
    }
}

void music_enable(uint8_t on)
{
    CRITICAL {
        music = on;
        if (!on) {
            NR22_REG = 0;
            NR30_REG = 0;
        }
    }
}

uint8_t music_enabled(void)
{
    return music;
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
        CRITICAL {
            NR30_REG = 0;                   /* the bass waits too */
            jingle_pos = jingle_wait = 0;
        }
        break;
    }
}
