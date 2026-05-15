/**
 * @file    app_mouse.cpp
 * @brief   BLE Composite HID (Keyboard + Mouse) for T-Display-S3-Pro
 *
 * Uses TWatch's BleCompositeHID (NimBLE-based) for a single BLE
 * connection that provides both mouse and keyboard HID services.
 * Touchpad area sends mouse delta; bottom buttons: left/right/scroll.
 */
#include "app_mouse.h"
#include "lvgl.h"
#include <Arduino.h>
#include "BleCompositeHID.h"

static lv_obj_t *mouse_scr = NULL;
static lv_obj_t *status_lbl, *info_lbl;
static lv_indev_t *indev;
static lv_point_t last_pt = {-1, -1};
static lv_timer_t *update_timer = NULL;
static lv_timer_t *status_timer = NULL;
static BleCompositeHID hid("S3-Pro HID", "LilyGo", 100);

#define TPAD_H  380
#define BTN_Y   400

static void update_mouse(lv_timer_t *t)
{
    (void)t;
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);
    lv_indev_state_t state = indev->proc.state;

    if (state == LV_INDEV_STATE_PR && pt.y < TPAD_H && hid.isConnected()) {
        lv_label_set_text_fmt(info_lbl, "X:%d Y:%d", pt.x, pt.y);
        if (last_pt.x >= 0) {
            int8_t dx = pt.x - last_pt.x;
            int8_t dy = pt.y - last_pt.y;
            if (dx || dy) hid.mouseMove(dx, dy);
        }
        last_pt = pt;
    } else {
        last_pt.x = -1; last_pt.y = -1;
        lv_label_set_text(info_lbl, "");
    }
}

static void update_status(lv_timer_t *t)
{
    (void)t;
    if (hid.isConnected()) {
        lv_label_set_text(status_lbl, LV_SYMBOL_WIFI " Connected");
        lv_obj_set_style_text_color(status_lbl, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(status_lbl, LV_SYMBOL_WIFI " Waiting...");
        lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xFF6600), 0);
    }
}

lv_obj_t *mouse_app_create(void)
{
    mouse_scr = lv_obj_create(NULL);
    lv_obj_set_size(mouse_scr, 222, 480);
    lv_obj_set_style_bg_color(mouse_scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_clear_flag(mouse_scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(mouse_scr);
    lv_label_set_text(title, LV_SYMBOL_LIST " Remote HID");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    status_lbl = lv_label_create(mouse_scr);
    lv_label_set_text(status_lbl, "Starting BLE...");
    lv_obj_align(status_lbl, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xAAAAAA), 0);

    lv_obj_t *pad_border = lv_obj_create(mouse_scr);
    lv_obj_set_size(pad_border, 210, TPAD_H - 30);
    lv_obj_align(pad_border, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_border_color(pad_border, lv_color_hex(0x444466), 0);
    lv_obj_set_style_border_width(pad_border, 1, 0);
    lv_obj_set_style_bg_opa(pad_border, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(pad_border, 4, 0);

    lv_obj_t *pad_label = lv_label_create(mouse_scr);
    lv_label_set_text(pad_label, "Touch to move cursor");
    lv_obj_align(pad_label, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_text_color(pad_label, lv_color_hex(0x666688), 0);

    info_lbl = lv_label_create(mouse_scr);
    lv_obj_set_style_text_color(info_lbl, lv_color_hex(0x88FF88), 0);
    lv_obj_align(info_lbl, LV_ALIGN_TOP_MID, 0, 380);

    auto mkbtn = [](int x, int y, int w, int h, const char *txt, void (*cb)(void)) {
        lv_obj_t *b = lv_btn_create(mouse_scr);
        lv_obj_set_size(b, w, h); lv_obj_set_pos(b, x, y);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, txt); lv_obj_center(l);
        lv_obj_add_event_cb(b, [](lv_event_t *e) {
            void (*f)(void) = (void(*)(void))(intptr_t)lv_event_get_user_data(e);
            if (f) f();
        }, LV_EVENT_CLICKED, (void*)(intptr_t)cb);
    };
    mkbtn(6,  BTN_Y, 66, 44, "Left",  [](){ if(hid.isConnected()) hid.mouseClick(MOUSE_LEFT); });
    mkbtn(78, BTN_Y, 66, 44, "Right", [](){ if(hid.isConnected()) hid.mouseClick(MOUSE_RIGHT); });
    mkbtn(150,BTN_Y, 66, 44, "Scroll",[](){ if(hid.isConnected()) hid.mouseMove(0,0,1); });

    lv_obj_t *bb = lv_btn_create(mouse_scr);
    lv_obj_set_size(bb, 50, 24);
    lv_obj_align(bb, LV_ALIGN_TOP_LEFT, 4, 28);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(bl);
    lv_obj_add_event_cb(bb, [](lv_event_t*) { lv_obj_del(mouse_scr); }, LV_EVENT_CLICKED, NULL);

    indev = lv_indev_get_next(NULL);
    update_timer = lv_timer_create(update_mouse, 50, NULL);
    status_timer = lv_timer_create(update_status, 1000, NULL);
    hid.begin();

    lv_scr_load(mouse_scr);
    return mouse_scr;
}
