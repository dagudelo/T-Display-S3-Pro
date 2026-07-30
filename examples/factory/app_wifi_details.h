/**
 * @file    app_wifi_details.h
 * @brief   WiFi connection details screen (SSID, IP, MAC, RSSI, Gateway)
 */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *wifi_details_app_create(void);

#ifdef __cplusplus
}
#endif
