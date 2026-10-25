/* SPDX-License-Identifier: GPL-3.0-only */
/* main.c -- Kakuro for the Game Boy: start-up and the screens. */
#include <gb/gb.h>
#include "main.h"
#include "clock.h"
#include "game.h"
#include "gfx.h"
#include "menu.h"
#include "save.h"
#include "sound.h"
#include "video.h"

uint8_t current;

void main(void)
{
    uint8_t screen = SCREEN_TITLE;

    DISPLAY_OFF;
    BGP_REG = PAL_NORMAL;
    OBP0_REG = PAL_SPRITE;
    OBP1_REG = PAL_INVERT;
    set_sprite_data(0, SPRITE_TILE_COUNT, sprite_tiles);
    sound_init();
    save_init();
    clock_init();
    current = save_last();
    if (current == NO_PUZZLE)
        current = 0;

    for (;;) {
        switch (screen) {
        case SCREEN_TITLE:  screen = title_screen(); break;
        case SCREEN_GROUPS: screen = groups_screen(); break;
        case SCREEN_GRID:   screen = grid_screen(); break;
        case SCREEN_HELP:   screen = help_screen(); break;
        default:            screen = game_play(); break;
        }
    }
}
