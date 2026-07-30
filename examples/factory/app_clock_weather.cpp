/**
 * @file    app_clock_weather.cpp
 * @brief   Digital clock with date + weather via Open-Meteo free API.
 */
#include "app_clock_weather.h"
#include "lvgl.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

static lv_obj_t *scr = NULL;
static lv_obj_t *digitime_lbl = NULL;
static lv_obj_t *digidate_lbl = NULL;
static lv_obj_t *weather_lbl = NULL;
static lv_timer_t *tick_timer = NULL;
static lv_timer_t *fetch_timer = NULL;

static void fetch_weather(void)
{
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin("http://api.open-meteo.com/v1/forecast?latitude=4.711&longitude=-74.072&current=temperature_2m,relative_humidity_2m,weather_code&timezone=America/Bogota");
    http.setTimeout(5000);
    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        int t_idx = payload.indexOf("\"temperature_2m\":");
        int w_idx = payload.indexOf("\"weather_code\":");
        char buf[64];
        if (t_idx > 0 && w_idx > 0) {
            float temp = payload.substring(t_idx + 18, payload.indexOf(",", t_idx)).toFloat();
            int wcode = payload.substring(w_idx + 16, payload.indexOf(",", w_idx)).toInt();
            const char *wdesc = "Clear";
            if (wcode <= 3) wdesc = "Clear";
            else if (wcode <= 48) wdesc = "Cloudy";
            else if (wcode <= 57) wdesc = "Drizzle";
            else if (wcode <= 67) wdesc = "Rain";
            else if (wcode <= 77) wdesc = "Snow";
            else if (wcode <= 82) wdesc = "Showers";
            else wdesc = "Storm";
            snprintf(buf, sizeof(buf), "%.1f C  %s", temp, wdesc);
        } else {
            snprintf(buf, sizeof(buf), "Weather N/A");
        }
        if (weather_lbl) lv_label_set_text(weather_lbl, buf);
    }
    http.end();
}

static void update_clock(lv_timer_t *)
{
    struct tm ti;
    time_t now;
    time(&now);
    localtime_r(&now, &ti);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
    if (digitime_lbl) lv_label_set_text(digitime_lbl, buf);
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
    if (digidate_lbl) lv_label_set_text(digidate_lbl, buf);
}

lv_obj_t *clock_weather_app_create(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, 222, 480);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Back button */
    lv_obj_t *bb = lv_btn_create(scr);
    lv_obj_set_size(bb, 50, 28);
    lv_obj_align(bb, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_add_event_cb(bb, [](lv_event_t *) {
        if (tick_timer) { lv_timer_del(tick_timer); tick_timer = NULL; }
        if (fetch_timer) { lv_timer_del(fetch_timer); fetch_timer = NULL; }
        lv_obj_del(scr);
        scr = NULL;
    }, LV_EVENT_CLICKED, NULL);

    /* Date */
    digidate_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(digidate_lbl, lv_color_hex(0xAAAAFF), 0);
    lv_obj_set_style_text_font(digidate_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(digidate_lbl, LV_ALIGN_TOP_MID, 0, 40);
    lv_label_set_text(digidate_lbl, "----.--.--");

    /* Digital time */
    digitime_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(digitime_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(digitime_lbl, &lv_font_montserrat_36, 0);
    lv_obj_align(digitime_lbl, LV_ALIGN_CENTER, 0, -20);
    lv_label_set_text(digitime_lbl, "--:--:--");

    /* Weather */
    weather_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(weather_lbl, lv_color_hex(0x88CC88), 0);
    lv_obj_set_style_text_font(weather_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(weather_lbl, LV_ALIGN_CENTER, 0, 60);
    lv_label_set_text(weather_lbl, "Fetching...");

    /* Timers */
    tick_timer = lv_timer_create(update_clock, 1000, NULL);
    fetch_timer = lv_timer_create([](lv_timer_t *) { fetch_weather(); }, 600000, NULL);
    lv_timer_set_repeat_count(fetch_timer, -1);
    update_clock(NULL);
    fetch_weather();

    lv_scr_load(scr);
    return scr;
}
