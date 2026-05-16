/**
 * @file    app_mouse.cpp
 * @brief   BLE Composite HID — mouse, media, keyboard, nav keys
 */
#include "app_mouse.h"
#include "lvgl.h"
#include <Arduino.h>
#include "BleCompositeHID.h"

static lv_obj_t *scr = NULL;
static lv_obj_t *status_lbl, *coord_lbl, *page_lbl;
static lv_obj_t *btn_row, *nav_row;
static lv_indev_t *indev;
static lv_point_t last_pt = {-1, -1}, press_start = {-1, -1};
static uint32_t press_time = 0;
static bool was_dragging = false;
static BleCompositeHID hid("S3-Pro HID", "LilyGo", 100);
static int page = 0;
static lv_timer_t *mtimer, *stimer;

#define PAD_TOP  6
#define PAD_H    338
#define BTN_ROW  328
#define NAV_ROW  440

static void send_key(uint8_t usage)
{
    if (hid.isConnected()) { hid.pressKey(usage); hid.releaseKey(usage); }
}

static void mkbtn(lv_obj_t *parent, int x, int y, int w, int h, const char *txt,
                  void (*cb)(lv_event_t *e), void *user_data)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_size(b, w, h); lv_obj_set_pos(b, x, y);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt); lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user_data);
}

static void mkcb(lv_obj_t *parent, int x, int y, int w, int h, const char *txt,
                 void (*fn)(void))
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_size(b, w, h); lv_obj_set_pos(b, x, y);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt); lv_obj_center(l);
    lv_obj_add_event_cb(b, [](lv_event_t *e) {
        void (*f)(void) = (void(*)(void))(intptr_t)lv_event_get_user_data(e);
        if (f) f();
    }, LV_EVENT_CLICKED, (void*)(intptr_t)fn);
}

static void nav_menu(void)  { send_key(0x76); }
static void nav_back(void)  { send_key(0x29); }
static void nav_home_exit(void)
{
    send_key(0x4A);
    struct d { static void c(void*) { if (scr) { lv_obj_del(scr); scr = NULL; } } };
    lv_timer_create([](lv_timer_t *t) { d::c(NULL); lv_timer_del(t); }, 10, NULL);
}

static void kb_cb(lv_event_t *e)
{
    uint8_t code = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (hid.isConnected()) { hid.pressKey(code); hid.releaseKey(code); }
}

static void show_keyboard(void)
{
    lv_obj_clean(btn_row);
    lv_obj_set_size(btn_row, 222, 95);
    auto addkey = [](int x, int y, int w, int h, const char *t, uint8_t c) {
        lv_obj_t *b = lv_btn_create(btn_row);
        lv_obj_set_size(b, w, h); lv_obj_set_pos(b, x, y);
        lv_obj_set_style_radius(b, 4, 0);
        lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, t); lv_obj_center(l);
        lv_obj_add_event_cb(b, kb_cb, LV_EVENT_CLICKED, (void*)(intptr_t)c);
    };
    int y1=2, y2=24, y3=46, y4=68;
    int w=20, g=2, o2=w+g, o3=o2*2;
    for (int i = 0; i < 10; i++) {
        char buf[2] = {"QWERTYUIOP"[i], 0};
        addkey(2+i*(w+g), y1, w, 22, buf, 0x14+i);
    }
    for (int i = 0; i < 9; i++) {
        char buf[2] = {"ASDFGHJKL"[i], 0};
        addkey(2+o2+i*(w+g), y2, w, 22, buf, 0x04+i);
    }
    for (int i = 0; i < 6; i++) {
        char buf[2] = {"ZXCVBNM"[i], 0};
        addkey(2+o3+i*(w+g), y3, w, 22, buf, 0x1D+i);
    }
    addkey(2+o3+6*(w+g), y3, w, 22, ",", 0x36);
    addkey(2+o3+7*(w+g), y3, w, 22, ".", 0x37);
    addkey(2,   y4, 90, 22, "Space", 0x2C);
    addkey(96,  y4, 60, 22, "Bksp",  0x2A);
    addkey(160, y4, 60, 22, "Enter", 0x28);
    lv_label_set_text(page_lbl, " Keyboard ");
}

static void switch_page(int p)
{
    page = p;
    lv_obj_clean(btn_row);
    lv_obj_set_size(btn_row, 222, 70);
    if (page == 1) {
        auto mk = [](int x, int y, int w, int h, const char *t, MediaKeyReport k) {
            lv_obj_t *b = lv_btn_create(btn_row);
            lv_obj_set_size(b, w, h); lv_obj_set_pos(b, x, y);
            lv_obj_set_style_radius(b, 6, 0);
            lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, t); lv_obj_center(l);
            MediaKeyReport *kp = (MediaKeyReport*)malloc(sizeof(MediaKeyReport));
            *kp = k;
            lv_obj_add_event_cb(b, [](lv_event_t *e) {
                MediaKeyReport *kp = (MediaKeyReport*)lv_event_get_user_data(e);
                hid.writeMediaKey(*kp);
            }, LV_EVENT_CLICKED, kp);
        };
        mk(10, 4,  62, 26, LV_SYMBOL_PREV, KEY_MEDIA_PREVIOUS_TRACK);
        mk(80, 4,  62, 26, LV_SYMBOL_PLAY, KEY_MEDIA_PLAY_PAUSE);
        mk(150,4,  62, 26, LV_SYMBOL_NEXT, KEY_MEDIA_NEXT_TRACK);
        mk(30, 36, 78, 28, "-Vol", KEY_MEDIA_VOLUME_DOWN);
        mk(114,36, 78, 28, "+Vol", KEY_MEDIA_VOLUME_UP);
        lv_label_set_text(page_lbl, " Media ");
    } else if (page == 2) {
        show_keyboard();
    } else {
        mkcb(btn_row, 10, 4,  62, 40, "Left",   [](){ if(hid.isConnected()) hid.mouseClick(MOUSE_LEFT); });
        mkcb(btn_row, 80, 4,  62, 40, "Right",  [](){ if(hid.isConnected()) hid.mouseClick(MOUSE_RIGHT); });
        mkcb(btn_row, 150,4,  62, 40, "Scroll", [](){ if(hid.isConnected()) hid.mouseMove(0,0,1); });
        lv_label_set_text(page_lbl, " Mouse ");
    }
}

static void update_touch(lv_timer_t *t)
{
    (void)t;
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);
    lv_indev_state_t state = indev->proc.state;

    if (state == LV_INDEV_STATE_PR && pt.y < PAD_TOP + PAD_H) {
        if (last_pt.x < 0) {
            press_start = pt; press_time = millis(); was_dragging = false;
        } else {
            if (abs(pt.x - press_start.x) > 6 || abs(pt.y - press_start.y) > 6)
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
                int dx = press_start.x - pt.x;
                int dy = press_start.y - pt.y;
                if (abs(dx) > 25 && abs(dy) < 40) {
                    page = (page + 1) % 3;
                    switch_page(page);
                }
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
        lv_label_set_text(status_lbl, "\u25C9 On");
        lv_obj_set_style_text_color(status_lbl, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(status_lbl, "\u25CB Wait");
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
    lv_label_set_text(title, "\u25C9 HID");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    status_lbl = lv_label_create(scr);
    lv_label_set_text(status_lbl, "\u25CB Wait");
    lv_obj_align(status_lbl, LV_ALIGN_TOP_RIGHT, -2, 4);
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xAAAAAA), 0);

    lv_obj_t *pad_cont = lv_obj_create(scr);
    lv_obj_set_size(pad_cont, 218, PAD_H);
    lv_obj_align(pad_cont, LV_ALIGN_TOP_MID, 0, PAD_TOP);
    lv_obj_set_style_border_color(pad_cont, lv_color_hex(0x444466), 0);
    lv_obj_set_style_border_width(pad_cont, 1, 0);
    lv_obj_set_style_bg_opa(pad_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(pad_cont, 4, 0);

    lv_obj_t *pl = lv_label_create(scr);
    lv_label_set_text(pl, "Drag=move  Tap=click  Swipe=page");
    lv_obj_align(pl, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_text_color(pl, lv_color_hex(0x666688), 0);

    coord_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(coord_lbl, lv_color_hex(0x88FF88), 0);
    lv_obj_align(coord_lbl, LV_ALIGN_TOP_MID, 0, 322);

    page_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(page_lbl, lv_color_hex(0x888888), 0);
    lv_obj_align(page_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_label_set_text(page_lbl, " Mouse ");

    btn_row = lv_obj_create(scr);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_row, 222, 70);
    lv_obj_set_pos(btn_row, 0, BTN_ROW);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    switch_page(0);

    nav_row = lv_obj_create(scr);
    lv_obj_clear_flag(nav_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(nav_row, 222, 24);
    lv_obj_set_pos(nav_row, 0, NAV_ROW);
    lv_obj_set_style_bg_opa(nav_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(nav_row, 0, 0);

    mkbtn(nav_row, 30, 0, 36, 22, "\u2261", [](lv_event_t*) { nav_menu(); }, NULL);
    mkbtn(nav_row, 93, 0, 36, 22, "\u2302", [](lv_event_t*) { nav_home_exit(); }, NULL);
    mkbtn(nav_row, 156,0, 36, 22, "\u25C0", [](lv_event_t*) { nav_back(); }, NULL);

    indev = lv_indev_get_next(NULL);
    mtimer = lv_timer_create(update_touch, 50, NULL);
    stimer = lv_timer_create(update_status, 1000, NULL);
    hid.begin();

    lv_scr_load(scr);
    return scr;
}
