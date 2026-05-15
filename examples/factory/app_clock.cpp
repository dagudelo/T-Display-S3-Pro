/**
 * @file    app_clock.cpp
 * @brief   Analog LVGL clock with NTP sync via WiFi.
 *
 * On open: connect WiFi, sync NTP, then disconnect WiFi.
 * Renders LVGL analog clock with hour/minute/second hands.
 */
#include "app_clock.h"
#include "lvgl.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>

/* WiFi credentials — stored in EEPROM by factory, fallback defaults */
#ifndef CLIENT_SSID
#define CLIENT_SSID "Agudelo Bonilla Mesh"
#endif
#ifndef CLIENT_PASS
#define CLIENT_PASS ""
#endif

static lv_obj_t *clock_scr = NULL;
static lv_obj_t *hour_line, *min_line, *sec_line;
static lv_obj_t *sync_label;
static lv_timer_t *tick_timer = NULL;
static lv_timer_t *sync_timer = NULL;
static bool ntp_done = false;

/* clock center and radius */
#define CX 111
#define CY 240
#define R  100

static void update_hands(void)
{
    struct tm ti;
    if (!getLocalTime(&ti)) return;

    float h_deg = (ti.tm_hour % 12) * 30.0f + ti.tm_min * 0.5f;
    float m_deg = ti.tm_min * 6.0f;
    float s_deg = ti.tm_sec * 6.0f;

    float rad;
    rad = h_deg * 3.14159f / 180.0f;
    lv_obj_set_pos(hour_line, CX + R * 0.5f * cosf(rad - 3.14159f/2) - 3,
                            CY + R * 0.5f * sinf(rad - 3.14159f/2) - 3);
    lv_obj_set_size(hour_line, 6, R * 0.5f);
    lv_obj_set_style_transform_angle(hour_line, (int16_t)(h_deg * 10), 0);

    rad = m_deg * 3.14159f / 180.0f;
    lv_obj_set_pos(min_line, CX + R * 0.7f * cosf(rad - 3.14159f/2) - 2,
                            CY + R * 0.7f * sinf(rad - 3.14159f/2) - 2);
    lv_obj_set_size(min_line, 4, R * 0.7f);
    lv_obj_set_style_transform_angle(min_line, (int16_t)(m_deg * 10), 0);

    rad = s_deg * 3.14159f / 180.0f;
    lv_obj_set_pos(sec_line, CX + R * 0.85f * cosf(rad - 3.14159f/2) - 1,
                             CY + R * 0.85f * sinf(rad - 3.14159f/2) - 1);
    lv_obj_set_size(sec_line, 2, R * 0.85f);
    lv_obj_set_style_transform_angle(sec_line, (int16_t)(s_deg * 10), 0);
}

static void draw_clock_face(lv_obj_t *parent)
{
    /* center dot */
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_pos(dot, CX-5, CY-5);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(dot, 0, 0);

    /* hour ticks */
    for (int i = 0; i < 12; i++) {
        float a = i * 30.0f * 3.14159f / 180.0f;
        int len = (i % 3 == 0) ? 12 : 6;
        lv_obj_t *tick = lv_obj_create(parent);
        lv_obj_set_size(tick, 2, len);
        lv_obj_set_pos(tick, CX + (R-10)*cosf(a-3.14159f/2) - 1,
                              CY + (R-10)*sinf(a-3.14159f/2));
        lv_obj_set_style_bg_color(tick, lv_color_hex(0x555555), 0);
        lv_obj_set_style_border_width(tick, 0, 0);
    }

    /* hour hands */
    hour_line = lv_obj_create(parent);
    lv_obj_set_style_bg_color(hour_line, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(hour_line, 0, 0);
    lv_obj_set_style_radius(hour_line, 3, 0);

    min_line = lv_obj_create(parent);
    lv_obj_set_style_bg_color(min_line, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_border_width(min_line, 0, 0);
    lv_obj_set_style_radius(min_line, 2, 0);

    sec_line = lv_obj_create(parent);
    lv_obj_set_style_bg_color(sec_line, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_border_width(sec_line, 0, 0);

    /* digital time label */
    lv_obj_t *dl = lv_label_create(parent);
    lv_obj_align(dl, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_text_color(dl, lv_color_hex(0x888888), 0);
    lv_label_set_text(dl, "--:--:--");
}

static void do_ntp_sync(void)
{
    if (ntp_done) return;
    lv_label_set_text(sync_label, "Connecting WiFi...");
    WiFi.begin(CLIENT_SSID, CLIENT_PASS);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries++ < 40) { delay(500); }
    if (WiFi.status() != WL_CONNECTED) {
        lv_label_set_text(sync_label, "#ff4444 WiFi failed#");
        lv_obj_set_style_text_color(sync_label, lv_color_hex(0xFF4444), 0);
        return;
    }
    lv_label_set_text(sync_label, "#00ff00 Syncing NTP...#");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    struct tm ti;
    tries = 0;
    while (!getLocalTime(&ti) && tries++ < 20) { delay(500); }
    const char *tz = "CST-8";
    setenv("TZ", tz, 1); tzset();
    ntp_done = true;
    lv_label_set_text(sync_label, LV_SYMBOL_WIFI " Synced");
    lv_obj_set_style_text_color(sync_label, lv_color_hex(0x00FF00), 0);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    update_hands();
}

lv_obj_t *clock_app_create(void)
{
    clock_scr = lv_obj_create(NULL);
    lv_obj_set_size(clock_scr, 222, 480);
    lv_obj_set_style_bg_color(clock_scr, lv_color_hex(0x111122), 0);
    lv_obj_clear_flag(clock_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* back button */
    lv_obj_t *bb = lv_btn_create(clock_scr);
    lv_obj_set_size(bb, 50, 24);
    lv_obj_align(bb, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(bl);
    lv_obj_add_event_cb(bb, [](lv_event_t*) { lv_obj_del(clock_scr); }, LV_EVENT_CLICKED, NULL);

    /* sync status */
    sync_label = lv_label_create(clock_scr);
    lv_label_set_recolor(sync_label, true);
    lv_label_set_text(sync_label, "Syncing...");
    lv_obj_align(sync_label, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_text_color(sync_label, lv_color_hex(0xAAAAAA), 0);

    draw_clock_face(clock_scr);

    /* NTP sync (async via timer so WiFi doesn't block UI) */
    sync_timer = lv_timer_create([](lv_timer_t*) { do_ntp_sync(); }, 100, NULL);
    lv_timer_set_repeat_count(sync_timer, 1);  /* fire once */

    /* tick every 1s */
    tick_timer = lv_timer_create([](lv_timer_t*) { update_hands(); }, 1000, NULL);

    lv_scr_load(clock_scr);
    return clock_scr;
}
