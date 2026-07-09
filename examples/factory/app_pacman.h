/**
 * @file    app_pacman.h
 * @brief   Classic Pac-Man for T-Display-S3-Pro (222x480 ST7796S LCD)
 *
 * Faithful arcade recreation: 21×23 maze, 4-ghost AI with scatter/chase/
 * frightened/eaten modes, fruit, level progression, speed tables.
 * All rendering via LVGL objects and lv_canvas — no external assets.
 */

#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maze dimensions — classic arcade */
#define PM_COLS 21
#define PM_ROWS 23

/**
 * @brief  Override the default maze before calling pacman_game_create().
 * @param  maze  21×23 uint8_t grid: 0=empty, 1=wall, 2=dot, 3=energizer.
 */
void pacman_set_maze(const uint8_t maze[PM_ROWS][PM_COLS]);

/**
 * @brief  Launch the classic Pac-Man game on a dedicated LVGL screen.
 * @return Pointer to the game screen (lv_scr_act after call).
 */
lv_obj_t *pacman_game_create(void);

#ifdef __cplusplus
}
#endif
