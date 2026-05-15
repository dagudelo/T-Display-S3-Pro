/**
 * @file    app_mouse.cpp
 * @brief   BLE Remote Mouse (stub — BLE too large for 16MB partition)
 */
#include "app_mouse.h"
#include "lvgl.h"
#include <Arduino.h>

static lv_obj_t *mouse_scr = NULL;

lv_obj_t *mouse_app_create(void)
{
    mouse_scr = lv_obj_create(NULL);
    lv_obj_set_size(mouse_scr, 222, 480);
    lv_obj_set_style_bg_color(mouse_scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_clear_flag(mouse_scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(mouse_scr);
    lv_label_set_text(title, "Remote Mouse");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    lv_obj_t *info = lv_label_create(mouse_scr);
    lv_label_set_text(info, "BLE stack requires ~570KB.\n"
                           "To enable, add BLE Mouse\n"
                           "and NimBLE to lib_deps\n"
                           "in platformio.ini.");
    lv_obj_set_style_text_color(info, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(info, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *bb = lv_btn_create(mouse_scr);
    lv_obj_set_size(bb, 50, 24);
    lv_obj_align(bb, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(bl);
    lv_obj_add_event_cb(bb, [](lv_event_t *e) {
        lv_obj_t *s = lv_obj_get_parent(lv_event_get_target(e));
        lv_obj_del(s);
    }, LV_EVENT_CLICKED, NULL);

    lv_scr_load(mouse_scr);
    return mouse_scr;
}
