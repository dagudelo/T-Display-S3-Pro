/**
 * @file    app_clock_weather.h
 * @brief   Digital clock with date and weather via Open-Meteo API
 */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *clock_weather_app_create(void);

#ifdef __cplusplus
}
#endif
