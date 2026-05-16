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
static lv_obj_t *btn_row;
static lv_indev_t *indev;
static lv_point_t last_pt = {-1, -1}, press_start = {-1, -1};
static uint32_t press_time = 0;
static bool was_dragging = false;
static BleCompositeHID hid("S3-Pro HID", "LilyGo", 100);
static int page = 0;  /* 0=mouse 1=media 2=keyboard */
static lv_timer_t *mtimer, *stimer;

#define PAD_TOP  30
#define PAD_H    290   /* shorter to fit kb */
#define BTN_ROW  335

/* send a keyboard usage ID to connected device (no delay — must not block LVGL) */
static void send_key(uint8_t usage)
{
    if (hid.isConnected()) {
        hid.pressKey(usage);
        hid.releaseKey(usage);
    }
}

/* send a modifier+key combination */
static void send_modkey(uint8_t mod, uint8_t key)
{
    if (!hid.isConnected()) return;
    hid.pressKey(key);
    hid.releaseKey(key);
}

static void send_home(void) { send_key(0x4A); lv_obj_del(scr); }  /* Keyboard Home + exit */
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

/* keyboard key button — sends HID code via user data */
static void kb_cb(lv_event_t *e) {
    uint8_t code = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (hid.isConnected()) { hid.pressKey(code); hid.releaseKey(code); }
}

/* HID usage codes for common keys */
#define HID_A      0x04
#define HID_B      0x05
#define HID_C      0x06
#define HID_D      0x07
#define HID_E      0x08
#define HID_F      0x09
#define HID_G      0x0A
#define HID_H      0x0B
#define HID_I      0x0C
#define HID_J      0x0D
#define HID_K      0x0E
#define HID_L      0x0F
#define HID_M      0x10
#define HID_N      0x11
#define HID_O      0x12
#define HID_P      0x13
#define HID_Q      0x14
#define HID_R      0x15
#define HID_S      0x16
#define HID_T      0x17
#define HID_U      0x18
#define HID_V      0x19
#define HID_W      0x1A
#define HID_X      0x1B
#define HID_Y      0x1C
#define HID_Z      0x1D
#define HID_1      0x1E
#define HID_2      0x1F
#define HID_3      0x20
#define HID_4      0x21
#define HID_5      0x22
#define HID_6      0x23
#define HID_7      0x24
#define HID_8      0x25
#define HID_9      0x26
#define HID_0      0x27
#define HID_ENTER  0x28
#define HID_ESC    0x29
#define HID_BSPACE 0x2A
#define HID_TAB    0x2B
#define HID_SPACE  0x2C
#define HID_DOT    0x36
#define HID_COMMA  0x37

static void show_keyboard(void)
{
    lv_obj_clean(btn_row);
    lv_obj_set_size(btn_row, 222, 140);
    /* row 1: Q W E R T Y U I O P */
    const char *row1 = "QWERTYUIOP";
    for (int i = 0; i < 10; i++) {
        char buf[2] = {row1[i], 0};
        lv_obj_t *b = lv_btn_create(btn_row);
        lv_obj_set_size(b, 19, 24); lv_obj_set_pos(b, 4 + i*21, 2);
        lv_obj_set_style_radius(b, 4, 0);
        lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, buf); lv_obj_center(l);
        lv_obj_add_event_cb(b, kb_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(HID_Q + i));
    }
    /* row 2: A S D F G H J K L */
    const char *row2 = "ASDFGHJKL";
    for (int i = 0; i < 9; i++) {
        char buf[2] = {row2[i], 0};
        lv_obj_t *b = lv_btn_create(btn_row);
        lv_obj_set_size(b, 19, 24); lv_obj_set_pos(b, 18 + i*21, 28);
        lv_obj_set_style_radius(b, 4, 0);
        lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, buf); lv_obj_center(l);
        lv_obj_add_event_cb(b, kb_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(HID_A + i));
    }
    /* row 3: Z X C V B N M , . */
    const char *row3_l = "ZXCVBNM";
    for (int i = 0; i < 6; i++) {
        char buf[2] = {row3_l[i], 0};
        lv_obj_t *b = lv_btn_create(btn_row);
        lv_obj_set_size(b, 19, 24); lv_obj_set_pos(b, 32 + i*22, 54);
        lv_obj_set_style_radius(b, 4, 0);
        lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, buf); lv_obj_center(l);
        lv_obj_add_event_cb(b, kb_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(HID_Z + i));
    }
    { lv_obj_t *b = lv_btn_create(btn_row); lv_obj_set_size(b, 19, 24); lv_obj_set_pos(b, 32 + 6*22, 54);
      lv_obj_set_style_radius(b, 4, 0); lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, ","); lv_obj_center(l);
      lv_obj_add_event_cb(b, kb_cb, LV_EVENT_CLICKED, (void*)(intptr_t)HID_COMMA); }
    { lv_obj_t *b = lv_btn_create(btn_row); lv_obj_set_size(b, 19, 24); lv_obj_set_pos(b, 32 + 7*22, 54);
      lv_obj_set_style_radius(b, 4, 0); lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, "."); lv_obj_center(l);
      lv_obj_add_event_cb(b, kb_cb, LV_EVENT_CLICKED, (void*)(intptr_t)HID_DOT); }

    /* row 4: Space, Backspace, Enter */
    { lv_obj_t *b = lv_btn_create(btn_row); lv_obj_set_size(b, 60, 26); lv_obj_set_pos(b, 4, 80);
      lv_obj_set_style_radius(b, 4, 0); lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, "Space"); lv_obj_center(l);
      lv_obj_add_event_cb(b, kb_cb, LV_EVENT_CLICKED, (void*)(intptr_t)HID_SPACE); }
    { lv_obj_t *b = lv_btn_create(btn_row); lv_obj_set_size(b, 60, 26); lv_obj_set_pos(b, 68, 80);
      lv_obj_set_style_radius(b, 4, 0); lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, "Bksp"); lv_obj_center(l);
      lv_obj_add_event_cb(b, kb_cb, LV_EVENT_CLICKED, (void*)(intptr_t)HID_BSPACE); }
    { lv_obj_t *b = lv_btn_create(btn_row); lv_obj_set_size(b, 60, 26); lv_obj_set_pos(b, 132, 80);
      lv_obj_set_style_radius(b, 4, 0); lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, "Enter"); lv_obj_center(l);
      lv_obj_add_event_cb(b, kb_cb, LV_EVENT_CLICKED, (void*)(intptr_t)HID_ENTER); }
    lv_label_set_text(page_lbl, " Keyboard ");
}

static void switch_page(int p)
{
    page = p;
    lv_obj_clean(btn_row);
    lv_obj_set_size(btn_row, 222, 80);
    if (page == 1) {  /* media */
        mkbtn(btn_row, 10,  2, 62, 34, LV_SYMBOL_PREV, [](){ hid.writeMediaKey(KEY_MEDIA_PREVIOUS_TRACK); });
        mkbtn(btn_row, 80,  2, 62, 34, LV_SYMBOL_PLAY, [](){ hid.writeMediaKey(KEY_MEDIA_PLAY_PAUSE); });
        mkbtn(btn_row, 150, 2, 62, 34, LV_SYMBOL_NEXT, [](){ hid.writeMediaKey(KEY_MEDIA_NEXT_TRACK); });
        mkbtn(btn_row, 30, 38, 78, 36, "-Vol", [](){ hid.writeMediaKey(KEY_MEDIA_VOLUME_DOWN); });
        mkbtn(btn_row, 114,38, 78, 36, "+Vol", [](){ hid.writeMediaKey(KEY_MEDIA_VOLUME_UP); });
        lv_label_set_text(page_lbl, " Media ");
    } else if (page == 2) {
        show_keyboard();
    } else {  /* mouse */
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
                if (abs(ex - sx) > 40 && abs(ey - sy) < 20) {
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
        lv_label_set_text(status_lbl, "\u25C9 On");  /* Bluetooth dot symbol */
        lv_obj_set_style_text_color(status_lbl, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(status_lbl, "\u25CB Wait");  /* empty dot */
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
    lv_label_set_text(title, "\u25C9 HID");  /* remove "BT" prefix, use bluetooth dot */
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    status_lbl = lv_label_create(scr);
    lv_label_set_text(status_lbl, "\u25CB Wait");
    lv_obj_align(status_lbl, LV_ALIGN_TOP_RIGHT, -2, 4);
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xAAAAAA), 0);

    /* nav buttons — pushed left to avoid status label overlap */
    lv_obj_t *mnu = lv_btn_create(scr);
    lv_obj_set_size(mnu, 28, 20);
    lv_obj_align(mnu, LV_ALIGN_TOP_LEFT, 24, 4);
    lv_obj_set_style_radius(mnu, 4, 0);
    lv_obj_t *mnu_l = lv_label_create(mnu);
    lv_label_set_text(mnu_l, "\u2261");  lv_obj_center(mnu_l);
    lv_obj_add_event_cb(mnu, [](lv_event_t*) { send_menu(); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *home = lv_btn_create(scr);
    lv_obj_set_size(home, 28, 20);
    lv_obj_align(home, LV_ALIGN_TOP_LEFT, 56, 4);
    lv_obj_set_style_radius(home, 4, 0);
    lv_obj_t *home_l = lv_label_create(home);
    lv_label_set_text(home_l, "\u2302");  lv_obj_center(home_l);
    lv_obj_add_event_cb(home, [](lv_event_t*) { send_home(); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back = lv_btn_create(scr);
    lv_obj_set_size(back, 28, 20);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 88, 4);
    lv_obj_set_style_radius(back, 4, 0);
    lv_obj_t *back_l = lv_label_create(back);
    lv_label_set_text(back_l, "\u25C0");  lv_obj_center(back_l);
    lv_obj_add_event_cb(back, [](lv_event_t*) { send_back(); }, LV_EVENT_CLICKED, NULL);

    /* touchpad border */
    lv_obj_t *pad_cont = lv_obj_create(scr);
    lv_obj_set_size(pad_cont, 210, PAD_H);
    lv_obj_align(pad_cont, LV_ALIGN_TOP_MID, 0, PAD_TOP);
    lv_obj_set_style_border_color(pad_cont, lv_color_hex(0x444466), 0);
    lv_obj_set_style_border_width(pad_cont, 1, 0);
    lv_obj_set_style_bg_opa(pad_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(pad_cont, 4, 0);

    lv_obj_t *pl = lv_label_create(scr);
    lv_label_set_text(pl, "Tap=click  Drag=move\nSwipe=page: 3 pages");
    lv_obj_align(pl, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_text_color(pl, lv_color_hex(0x666688), 0);

    coord_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(coord_lbl, lv_color_hex(0x88FF88), 0);
    lv_obj_align(coord_lbl, LV_ALIGN_TOP_MID, 0, 325);

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