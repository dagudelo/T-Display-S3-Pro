/**
 * @file    app_mouse.cpp
 * @brief   BLE Composite HID — mouse, media, nav keys
 */
#include "app_mouse.h"
#include "lvgl.h"
#include <Arduino.h>
#include "BleCompositeHID.h"

static lv_obj_t *scr = NULL;
static lv_obj_t *status_lbl, *coord_lbl, *page_lbl;
static lv_obj_t *btn_row;
static lv_indev_t *indev;
static lv_point_t last_pt = {-1, -1}, press_start = {-1, -1};
static uint32_t press_time = 0;
static bool was_dragging = false;
static BleCompositeHID hid("S3-Pro HID", "LilyGo", 100);
static int page = 0;
static lv_timer_t *mtimer, *stimer;

#define PAD_TOP  30
#define PAD_H    350
#define BTN_ROW  395

/* send a keyboard usage ID to the connected device */
static void send_key(uint8_t usage)
{
    if (hid.isConnected()) {
        hid.pressKey(usage);
        delay(20);
        hid.releaseKey(usage);
    }
}

static void send_home(void) { send_key(0x4A); lv_obj_del(scr); }  /* Keyboard Home, then exit */
static void send_back(void) { send_key(0x29); }                   /* Escape = Android Back */
static void send_menu(void) { send_key(0x76); }                   /* Keyboard Menu = Overview */

static void mkbtn(lv_obj_t *parent, int x, int y, int w, int h, const char *txt, void (*cb)(void))
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_size(b, w, h); lv_obj_set_pos(b, x, y);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt); lv_obj_center(l);
    lv_obj_add_event_cb(b, [](lv_event_t *e) {
        void (*f)(void) = (void(*)(void))(intptr_t)lv_event_get_user_data(e);
        if (f) f();
    }, LV_EVENT_CLICKED, (void*)(intptr_t)cb);
}

static void switch_page(int p)
{
    page = p;
    lv_obj_clean(btn_row);
    if (page) {  /* media page */
        mkbtn(btn_row, 10,  2, 62, 34, LV_SYMBOL_PREV, [](){ hid.writeMediaKey(KEY_MEDIA_PREVIOUS_TRACK); });
        mkbtn(btn_row, 80,  2, 62, 34, LV_SYMBOL_PLAY, [](){ hid.writeMediaKey(KEY_MEDIA_PLAY_PAUSE); });
        mkbtn(btn_row, 150, 2, 62, 34, LV_SYMBOL_NEXT, [](){ hid.writeMediaKey(KEY_MEDIA_NEXT_TRACK); });
        mkbtn(btn_row, 30, 38, 78, 36, "-Vol", [](){ hid.writeMediaKey(KEY_MEDIA_VOLUME_DOWN); });
        mkbtn(btn_row, 114,38, 78, 36, "+Vol", [](){ hid.writeMediaKey(KEY_MEDIA_VOLUME_UP); });
        lv_label_set_text(page_lbl, " Media ");
    } else {  /* mouse page */
        mkbtn(btn_row, 10, 4, 62, 40, "Left",   [](){ if(hid.isConnected()) hid.mouseClick(MOUSE_LEFT); });
        mkbtn(btn_row, 80, 4, 62, 40, "Right",  [](){ if(hid.isConnected()) hid.mouseClick(MOUSE_RIGHT); });
        mkbtn(btn_row, 150,4, 62, 40, "Scroll", [](){ if(hid.isConnected()) hid.mouseMove(0,0,1); });
        lv_label_set_text(page_lbl, " Mouse ");
    }
}

static void update_mouse(lv_timer_t *t)
{
    (void)t;
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);
    lv_indev_state_t state = indev->proc.state;

    if (state == LV_INDEV_STATE_PR && pt.y < PAD_TOP + PAD_H) {
        if (last_pt.x < 0) {
            press_start = pt; press_time = millis(); was_dragging = false;
        } else {
            if (abs(pt.x - press_start.x) > 8 || abs(pt.y - press_start.y) > 8)
                was_dragging = true;
        }
        if (was_dragging && hid.isConnected()) {
            int8_t dx = pt.x - last_pt.x, dy = pt.y - last_pt.y;
            if (dx || dy) hid.mouseMove(dx, dy);
        }
        lv_label_set_text_fmt(coord_lbl, "X:%d Y:%d", pt.x, pt.y);
        last_pt = pt;
    } else {
        if (last_pt.x >= 0 && hid.isConnected()) {
            if (!was_dragging) {
                hid.mouseClick(MOUSE_LEFT);
                lv_label_set_text(coord_lbl, "Click");
            } else {
                int sx = press_start.x, sy = press_start.y;
                int ex = (last_pt.x >= 0) ? last_pt.x : pt.x;
                int ey = (last_pt.y >= 0) ? last_pt.y : pt.y;
                if (abs(ex - sx) > 40 && abs(ey - sy) < 20)
                    switch_page(!page);
            }
        }
        last_pt.x = -1; last_pt.y = -1;
        if (millis() - press_time > 200) lv_label_set_text(coord_lbl, "");
    }
}

static void update_status(lv_timer_t *t)
{
    (void)t;
    if (hid.isConnected()) {
        lv_label_set_text(status_lbl, LV_SYMBOL_WIFI " On");
        lv_obj_set_style_text_color(status_lbl, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(status_lbl, LV_SYMBOL_WIFI " Wait");
        lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xFF6600), 0);
    }
}

lv_obj_t *mouse_app_create(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, 222, 480);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, LV_SYMBOL_LIST " HID");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    status_lbl = lv_label_create(scr);
    lv_label_set_text(status_lbl, "Starting...");
    lv_obj_align(status_lbl, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xAAAAAA), 0);

    lv_obj_t *mnu = lv_btn_create(scr);
    lv_obj_set_size(mnu, 32, 22);
    lv_obj_align(mnu, LV_ALIGN_TOP_LEFT, 38, 3);
    lv_obj_set_style_radius(mnu, 4, 0);
    lv_obj_t *mnu_l = lv_label_create(mnu);
    lv_label_set_text(mnu_l, "\u2261");  lv_obj_center(mnu_l);
    lv_obj_add_event_cb(mnu, [](lv_event_t*) { send_menu(); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *home = lv_btn_create(scr);
    lv_obj_set_size(home, 32, 22);
    lv_obj_align(home, LV_ALIGN_TOP_LEFT, 74, 3);
    lv_obj_set_style_radius(home, 4, 0);
    lv_obj_t *home_l = lv_label_create(home);
    lv_label_set_text(home_l, "\u2302");  lv_obj_center(home_l);
    lv_obj_add_event_cb(home, [](lv_event_t*) { send_home(); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back = lv_btn_create(scr);
    lv_obj_set_size(back, 32, 22);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 110, 3);
    lv_obj_set_style_radius(back, 4, 0);
    lv_obj_t *back_l = lv_label_create(back);
    lv_label_set_text(back_l, "\u25C0");  lv_obj_center(back_l);
    lv_obj_add_event_cb(back, [](lv_event_t*) { send_back(); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *pad_cont = lv_obj_create(scr);
    lv_obj_set_size(pad_cont, 210, PAD_H);
    lv_obj_align(pad_cont, LV_ALIGN_TOP_MID, 0, PAD_TOP);
    lv_obj_set_style_border_color(pad_cont, lv_color_hex(0x444466), 0);
    lv_obj_set_style_border_width(pad_cont, 1, 0);
    lv_obj_set_style_bg_opa(pad_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(pad_cont, 4, 0);

    lv_obj_t *pl = lv_label_create(scr);
    lv_label_set_text(pl, "Tap=click  Drag=move\nSwipe=page");
    lv_obj_align(pl, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_text_color(pl, lv_color_hex(0x666688), 0);

    coord_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(coord_lbl, lv_color_hex(0x88FF88), 0);
    lv_obj_align(coord_lbl, LV_ALIGN_TOP_MID, 0, 383);

    page_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(page_lbl, lv_color_hex(0x888888), 0);
    lv_obj_align(page_lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_label_set_text(page_lbl, " Mouse ");

    btn_row = lv_obj_create(scr);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_row, 222, 80);
    lv_obj_set_pos(btn_row, 0, BTN_ROW);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    switch_page(0);

    indev = lv_indev_get_next(NULL);
    mtimer = lv_timer_create(update_mouse, 50, NULL);
    stimer = lv_timer_create(update_status, 1000, NULL);
    hid.begin();

    lv_scr_load(scr);
    return scr;
}
