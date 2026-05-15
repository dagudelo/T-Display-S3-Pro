/**
 * @file    app_clock.h
 * @brief   Analog clock with NTP sync
 */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *clock_app_create(void);

#ifdef __cplusplus
}
#endif
