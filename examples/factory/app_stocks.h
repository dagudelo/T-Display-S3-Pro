/**
 * @file    app_stocks.h
 * @brief   Live stock quotes via Yahoo Finance API with ticker/time-range selector
 */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *stocks_app_create(void);

#ifdef __cplusplus
}
#endif
