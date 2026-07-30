/**
 * @file    app_wifi_details.cpp
 * @brief   WiFi connection details screen (SSID, IP, MAC, RSSI, Gateway)
 */
#include "app_wifi_details.h"
#include "lvgl.h"
#include <WiFi.h>

static lv_obj_t *scr = NULL;
static lv_obj_t *ssid_lbl = NULL;
static lv_obj_t *ip_lbl = NULL;
static lv_obj_t *mac_lbl = NULL;
static lv_obj_t *rssi_lbl = NULL;
static lv_obj_t *gw_lbl = NULL;

static void add_row(lv_obj_t *parent, int *y, const char *label,
                    const char *value, lv_obj_t **out)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, 210, 36);
    lv_obj_set_pos(row, 6, *y);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x252550), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x8888CC), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 6, 0);

    lv_obj_t *val = lv_label_create(row);
    lv_label_set_text(val, value);
    lv_obj_set_style_text_color(val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, -6, 0);
    if (out) *out = val;
    *y += 42;
}

static void refresh_fields(void)
{
    if (!scr) return;
    if (ssid_lbl) lv_label_set_text(ssid_lbl,
        WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "N/A");
    if (ip_lbl) lv_label_set_text(ip_lbl,
        WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "N/A");
    if (mac_lbl) lv_label_set_text(mac_lbl, WiFi.macAddress().c_str());
    char rs[16];
    snprintf(rs, sizeof(rs), "%d dBm", WiFi.RSSI());
    if (rssi_lbl) lv_label_set_text(rssi_lbl, rs);
    if (gw_lbl) lv_label_set_text(gw_lbl,
        WiFi.status() == WL_CONNECTED ? WiFi.gatewayIP().toString().c_str() : "N/A");
}

lv_obj_t *wifi_details_app_create(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, 222, 480);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Header */
    lv_obj_t *hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, 222, 50);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x252550), 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bb = lv_btn_create(hdr);
    lv_obj_set_size(bb, 50, 30);
    lv_obj_align(bb, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_add_event_cb(bb, [](lv_event_t *) { lv_obj_del(scr); scr = NULL; }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *ht = lv_label_create(hdr);
    lv_label_set_text(ht, LV_SYMBOL_WIFI " WiFi Details");
    lv_obj_set_style_text_color(ht, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(ht);

    /* Detail rows */
    int y = 60;
    add_row(scr, &y, "SSID:",
            WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "N/A", &ssid_lbl);
    add_row(scr, &y, "IP:",
            WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "N/A", &ip_lbl);
    add_row(scr, &y, "MAC:", WiFi.macAddress().c_str(), &mac_lbl);
    char rssi_buf[16];
    snprintf(rssi_buf, sizeof(rssi_buf), "%d dBm", WiFi.RSSI());
    add_row(scr, &y, "RSSI:", rssi_buf, &rssi_lbl);
    add_row(scr, &y, "Gateway:",
            WiFi.status() == WL_CONNECTED ? WiFi.gatewayIP().toString().c_str() : "N/A", &gw_lbl);

    /* Refresh button */
    lv_obj_t *rf = lv_btn_create(scr);
    lv_obj_set_size(rf, 100, 36);
    lv_obj_set_pos(rf, 61, y + 10);
    lv_obj_t *rfl = lv_label_create(rf);
    lv_label_set_text(rfl, LV_SYMBOL_REFRESH " Refresh");
    lv_obj_add_event_cb(rf, [](lv_event_t *) { refresh_fields(); }, LV_EVENT_CLICKED, NULL);

    lv_scr_load(scr);
    return scr;
}
