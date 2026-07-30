/**
 * @file    app_music.h
 * @brief   PWM-based tone player (no DAC on T-Display-S3-Pro; uses vibrator pin)
 */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *music_app_create(void);

#ifdef __cplusplus
}
#endif
