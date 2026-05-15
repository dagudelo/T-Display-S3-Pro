/**
 * @file    app_files.cpp
 * @brief   SD Card file browser for T-Display-S3-Pro
 */
#include "app_files.h"
#include "sd_card.h"
#include "lvgl.h"
#include <Arduino.h>
#include <SPI.h>

static lv_obj_t *file_scr = NULL;
static lv_obj_t *file_list = NULL;
static char cur_path[64] = "/";
static const int MAX_ENTRIES = 32;

static void browse_dir(const char *path);
static void list_event_cb(lv_event_t *e);

lv_obj_t *files_app_create(void)
{
    file_scr = lv_obj_create(NULL);
    lv_obj_set_size(file_scr, 222, 480);
    lv_obj_set_style_bg_color(file_scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_clear_flag(file_scr, LV_OBJ_FLAG_SCROLLABLE);

    if (!sd_card_get_init_flag()) {
        sd_card_init();
        if (!sd_card_get_init_flag()) {
            lv_obj_t *l = lv_label_create(file_scr);
            lv_label_set_text(l, "No SD card found");
            lv_obj_center(l);
            lv_obj_set_style_text_color(l, lv_color_hex(0xFF4444), 0);
            lv_scr_load(file_scr);
            return file_scr;
        }
    }

    lv_obj_t *title = lv_label_create(file_scr);
    lv_label_set_text(title, LV_SYMBOL_DIRECTORY " SD Browser");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    lv_obj_t *path_lbl = lv_label_create(file_scr);
    lv_label_set_text(path_lbl, cur_path);
    lv_obj_align(path_lbl, LV_ALIGN_TOP_LEFT, 4, 24);
    lv_obj_set_style_text_color(path_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_width(path_lbl, 210);
    lv_label_set_long_mode(path_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);

    lv_obj_t *bb = lv_btn_create(file_scr);
    lv_obj_set_size(bb, 50, 24);
    lv_obj_align(bb, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(bl);
    lv_obj_add_event_cb(bb, [](lv_event_t*) { lv_obj_del(file_scr); }, LV_EVENT_CLICKED, NULL);

    file_list = lv_list_create(file_scr);
    lv_obj_set_size(file_list, 210, 420);
    lv_obj_align(file_list, LV_ALIGN_TOP_LEFT, 6, 50);
    lv_obj_set_style_radius(file_list, 4, 0);
    lv_obj_set_style_bg_color(file_list, lv_color_hex(0x222244), 0);
    lv_obj_set_style_border_width(file_list, 0, 0);

    browse_dir(cur_path);

    lv_scr_load(file_scr);
    return file_scr;
}

static void browse_dir(const char *path)
{
    if (file_list) lv_obj_clean(file_list);

    File root = SD_FD_DRI.open(path);
    if (!root) return;
    if (!root.isDirectory()) return;

    if (strcmp(path, "/") != 0) {
        lv_obj_t *btn = lv_list_add_btn(file_list, LV_SYMBOL_LEFT, "..");
        lv_obj_add_event_cb(btn, list_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
    }

    File file = root.openNextFile();
    while (file && lv_obj_get_child_cnt(file_list) < MAX_ENTRIES) {
        const char *name = file.name();
        if (name[0] == '.') { file = root.openNextFile(); continue; }
        const char *sym = file.isDirectory() ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE;
        lv_obj_t *btn = lv_list_add_btn(file_list, sym, name);
        lv_obj_add_event_cb(btn, list_event_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(file.isDirectory() ? 1 : 0));
        file = root.openNextFile();
    }
}

static void list_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *name = lv_list_get_btn_text(file_list, btn);
    bool is_dir = (bool)(intptr_t)lv_event_get_user_data(e);

    if (is_dir) {
        if (strcmp(name, "..") == 0) {
            char *last = strrchr(cur_path, '/');
            if (last && last != cur_path) *last = '\0';
            else strcpy(cur_path, "/");
        } else {
            size_t len = strlen(cur_path);
            if (cur_path[len-1] != '/') strcat(cur_path, "/");
            strcat(cur_path, name);
        }
        browse_dir(cur_path);
    } else {
        char full[80];
        snprintf(full, sizeof(full), "%s/%s", cur_path, name);
        File f = SD_FD_DRI.open(full);
        if (f) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s\n%llu bytes", name, f.size());
            lv_obj_t *m = lv_msgbox_create(NULL, "File Info", buf, NULL, true);
            lv_obj_center(m);
            f.close();
        }
    }
}
