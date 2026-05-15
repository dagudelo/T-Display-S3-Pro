/**
 * @file    app_pacman.h
 * @brief   Pacman game for T-Display-S3-Pro (222x480 LVGL)
 *
 * Maze-based arcade game with touch controls.
 * Uses LVGL objects for rendering — no external assets needed.
 */

#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Launch the Pacman game on a dedicated LVGL screen.
 * @return Pointer to the game screen (lv_scr_act after call).
 */
lv_obj_t *pacman_game_create(void);

#ifdef __cplusplus
}
#endif
