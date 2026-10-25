/* SPDX-License-Identifier: GPL-3.0-only */
/* main.c -- Kakuro for the Game Boy: start-up and the screens. */
#include <gb/gb.h>
#include "main.h"
#include "clock.h"
#include "game.h"
#include "gfx.h"
#include "menu.h"
#include "best.h"
#include "sound.h"
#include "video.h"

uint8_t current;

void main(void)
{
    uint8_t screen = SCREEN_SPLASH;

    DISPLAY_OFF;
    video_init();
    BGP_REG = PAL_NORMAL;
    OBP0_REG = PAL_SPRITE;
    OBP1_REG = PAL_INVERT;
    set_sprite_data(0, SPRITE_TILE_COUNT, sprite_tiles);
    sound_init();
    best_init();
    music_enable(1);
    clock_init();
    current = 0;

    for (;;) {
        switch (screen) {
        case SCREEN_SPLASH: screen = splash_screen(); break;
        case SCREEN_TITLE:  screen = title_screen(); break;
        case SCREEN_GRID:   screen = grid_screen(); break;
        default:            screen = game_play(); break;
        }
    }
}
