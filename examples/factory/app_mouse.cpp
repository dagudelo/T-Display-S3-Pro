/**
 * @file    app_mouse.cpp
 * @brief   BLE Composite HID — touch, click, media keys
 *
 * Touchpad: drag = mouse move, tap = left click.
 * Media tab: play/pause, prev, next track.
 */
#include "app_mouse.h"
#include "lvgl.h"
#include <Arduino.h>
#include "BleCompositeHID.h"

static lv_obj_t *scr = NULL;
static lv_obj_t *status_lbl, *coord_lbl;
static lv_obj_t *btn_row;
static lv_indev_t *indev;
static lv_point_t last_pt = {-1, -1}, press_start = {-1, -1};
static uint32_t press_time = 0;
static bool was_dragging = false;
static lv_timer_t *mtimer, *stimer;
static BleCompositeHID hid("S3-Pro HID", "LilyGo", 100);
static int page = 0;

#define PAD_TOP 30
#define PAD_H   350
#define BTN_Y   400

static void mkbtn(lv_obj_t *parent, int x, int y, int w, int h, const char *txt, void (*cb)(void))
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_size(b, w, h); lv_obj_set_pos(b, x, y);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt); lv_obj_center(l);
    lv_obj_add_event_cb(b, [](lv_event_t *e) {
        void (*f)(void) = (void(*)(void))(intptr_t)lv_event_get_user_data(e);
        if (f) f();
    }, LV_EVENT_CLICKED, (void*)(intptr_t)cb);
}

static void show_mouse(void)
{
    lv_obj_clean(btn_row);
    mkbtn(btn_row, 6,  0, 66, 44, "Left",   [](){ if(hid.isConnected()) hid.mouseClick(MOUSE_LEFT); });
    mkbtn(btn_row, 78, 0, 66, 44, "Right",  [](){ if(hid.isConnected()) hid.mouseClick(MOUSE_RIGHT); });
    mkbtn(btn_row, 150,0, 66, 44, "Scroll", [](){ if(hid.isConnected()) hid.mouseMove(0,0,1); });
}

static void show_media(void)
{
    lv_obj_clean(btn_row);
    mkbtn(btn_row, 6,  0, 66, 44, LV_SYMBOL_PREV, [](){ hid.writeMediaKey(KEY_MEDIA_PREVIOUS_TRACK); });
    mkbtn(btn_row, 78, 0, 66, 44, LV_SYMBOL_PLAY, [](){ hid.writeMediaKey(KEY_MEDIA_PLAY_PAUSE); });
    mkbtn(btn_row, 150,0, 66, 44, LV_SYMBOL_NEXT, [](){ hid.writeMediaKey(KEY_MEDIA_NEXT_TRACK); });
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
            int dx = abs(pt.x - press_start.x);
            int dy = abs(pt.y - press_start.y);
            if (dx > 8 || dy > 8) was_dragging = true;
        }
        if (was_dragging && hid.isConnected()) {
            int8_t dx = pt.x - last_pt.x;
            int8_t dy = pt.y - last_pt.y;
            if (dx || dy) hid.mouseMove(dx, dy);
        }
        lv_label_set_text_fmt(coord_lbl, "X:%d Y:%d", pt.x, pt.y);
        last_pt = pt;
    } else {
        if (last_pt.x >= 0 && !was_dragging && hid.isConnected()) {
            hid.mouseClick(MOUSE_LEFT);
            lv_label_set_text(coord_lbl, "Click");
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

    lv_obj_t *tab_btn = lv_btn_create(scr);
    lv_obj_set_size(tab_btn, 44, 24);
    lv_obj_align(tab_btn, LV_ALIGN_TOP_LEFT, 38, 4);
    lv_obj_t *tab_lbl = lv_label_create(tab_btn);
    lv_label_set_text(tab_lbl, "Media");
    lv_obj_center(tab_lbl);
    lv_obj_add_event_cb(tab_btn, [](lv_event_t*) {
        page = !page;
        if (page) show_media(); else show_mouse();
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *bb = lv_btn_create(scr);
    lv_obj_set_size(bb, 50, 24);
    lv_obj_align(bb, LV_ALIGN_TOP_LEFT, 88, 4);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(bl);
    lv_obj_add_event_cb(bb, [](lv_event_t*) { lv_obj_del(scr); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *pad_cont = lv_obj_create(scr);
    lv_obj_set_size(pad_cont, 210, PAD_H);
    lv_obj_align(pad_cont, LV_ALIGN_TOP_MID, 0, PAD_TOP);
    lv_obj_set_style_border_color(pad_cont, lv_color_hex(0x444466), 0);
    lv_obj_set_style_border_width(pad_cont, 1, 0);
    lv_obj_set_style_bg_opa(pad_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(pad_cont, 4, 0);

    lv_obj_t *pl = lv_label_create(scr);
    lv_label_set_text(pl, "Drag=move  Tap=click");
    lv_obj_align(pl, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_text_color(pl, lv_color_hex(0x666688), 0);

    coord_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(coord_lbl, lv_color_hex(0x88FF88), 0);
    lv_obj_align(coord_lbl, LV_ALIGN_TOP_MID, 0, 385);

    btn_row = lv_obj_create(scr);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_row, 222, 80);
    lv_obj_set_pos(btn_row, 0, BTN_Y);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    show_mouse();

    indev = lv_indev_get_next(NULL);
    mtimer = lv_timer_create(update_mouse, 50, NULL);
    stimer = lv_timer_create(update_status, 1000, NULL);
    hid.begin();

    lv_scr_load(scr);
    return scr;
}
