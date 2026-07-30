/**
 * @file    app_stocks.cpp
 * @brief   Live stock quotes via Yahoo Finance API with ticker/time-range selector.
 */
#include "app_stocks.h"
#include "lvgl.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

static const char *tickers[]  = {"USDCOP=X", "NU", "AMZN", "AAPL", "NVDA"};
static const char *names[]    = {"USD/COP", "Nu Holdings", "Amazon", "Apple", "NVIDIA"};
static const char *ranges[]   = {"12H", "1D", "1W", "1M"};
static const int   tkr_count  = 5;
static int sel_ticker  = 0;
static int sel_range   = 1;  /* 0=12H, 1=1D, 2=1W, 3=1M */

static lv_obj_t *scr         = NULL;
static lv_obj_t *name_lbl    = NULL;
static lv_obj_t *price_lbl   = NULL;
static lv_obj_t *change_lbl  = NULL;
static lv_obj_t *range_lbl   = NULL;

static void fetch(void)
{
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    String url = "https://query1.finance.yahoo.com/v8/finance/chart/";
    url += tickers[sel_ticker];
    url += "?range=";
    url += ranges[sel_range];
    url += "&interval=5m";
    http.begin(url);
    http.setTimeout(8000);
    if (http.GET() == 200) {
        String p = http.getString();
        int pi = p.indexOf("\"regularMarketPrice\":");
        int ci = p.indexOf("\"regularMarketChange\":");
        if (pi > 0) {
            float price  = p.substring(pi + 22, p.indexOf(",", pi)).toFloat();
            float change = 0;
            if (ci > 0) change = p.substring(ci + 23, p.indexOf(",", ci)).toFloat();
            char buf[48];
            snprintf(buf, sizeof(buf), "$%.2f", price);
            if (price_lbl) lv_label_set_text(price_lbl, buf);
            snprintf(buf, sizeof(buf), "%+.2f", change);
            if (change_lbl) {
                lv_label_set_text(change_lbl, buf);
                lv_obj_set_style_text_color(change_lbl,
                    change >= 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF4444), 0);
            }
        }
    }
    http.end();
}

static void update_display(void)
{
    if (name_lbl)   lv_label_set_text(name_lbl, names[sel_ticker]);
    if (price_lbl)  lv_label_set_text(price_lbl, "Loading...");
    if (change_lbl) lv_label_set_text(change_lbl, "");
    if (range_lbl)  lv_label_set_text(range_lbl, ranges[sel_range]);
    fetch();
}

lv_obj_t *stocks_app_create(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, 222, 480);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0d0d1a), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Back */
    lv_obj_t *bb = lv_btn_create(scr);
    lv_obj_set_size(bb, 50, 28);
    lv_obj_align(bb, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_add_event_cb(bb, [](lv_event_t *) { lv_obj_del(scr); scr = NULL; }, LV_EVENT_CLICKED, NULL);

    /* Prev/Next ticker */
    lv_obj_t *prev = lv_btn_create(scr);
    lv_obj_set_size(prev, 30, 30);
    lv_obj_align(prev, LV_ALIGN_TOP_LEFT, 60, 4);
    lv_obj_t *pl = lv_label_create(prev);
    lv_label_set_text(pl, LV_SYMBOL_LEFT);
    lv_obj_add_event_cb(prev, [](lv_event_t *) {
        sel_ticker = (sel_ticker - 1 + tkr_count) % tkr_count;
        update_display();
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *next = lv_btn_create(scr);
    lv_obj_set_size(next, 30, 30);
    lv_obj_align(next, LV_ALIGN_TOP_RIGHT, -60, 4);
    lv_obj_t *nl = lv_label_create(next);
    lv_label_set_text(nl, LV_SYMBOL_RIGHT);
    lv_obj_add_event_cb(next, [](lv_event_t *) {
        sel_ticker = (sel_ticker + 1) % tkr_count;
        update_display();
    }, LV_EVENT_CLICKED, NULL);

    /* Ticker name */
    name_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(name_lbl, LV_ALIGN_TOP_MID, 0, 10);

    /* Price */
    price_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(price_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(price_lbl, &lv_font_montserrat_32, 0);
    lv_obj_align(price_lbl, LV_ALIGN_CENTER, 0, -40);

    /* Change */
    change_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(change_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(change_lbl, LV_ALIGN_CENTER, 0, 10);

    /* Range label */
    range_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(range_lbl, lv_color_hex(0x8888FF), 0);
    lv_obj_align(range_lbl, LV_ALIGN_CENTER, 0, 60);

    /* Range buttons */
    for (int i = 0; i < 4; i++) {
        lv_obj_t *rb = lv_btn_create(scr);
        lv_obj_set_size(rb, 44, 28);
        lv_obj_align(rb, LV_ALIGN_BOTTOM_MID, (i - 1) * 52, -20);
        lv_obj_t *rbl = lv_label_create(rb);
        lv_label_set_text(rbl, ranges[i]);
        lv_obj_add_event_cb(rb, [](lv_event_t *ev) {
            sel_range = (int)(uintptr_t)lv_event_get_user_data(ev);
            if (range_lbl) lv_label_set_text(range_lbl, ranges[sel_range]);
            fetch();
        }, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }

    lv_scr_load(scr);
    update_display();
    return scr;
}
