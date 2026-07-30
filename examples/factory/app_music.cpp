/**
 * @file    app_music.cpp
 * @brief   PWM-based tone player. Board has no DAC; uses vibrator pin (GPIO16).
 */
#include "app_music.h"
#include "utilities.h"
#include "lvgl.h"
#include <Arduino.h>

#define TONE_PIN VIBRATING_MOTOR

static const int    notes[]     = {262,294,330,349,392,440,494,523,587,659,698,784,880,988,1047};
static const char  *note_names[] = {"C","D","E","F","G","A","B","C5","D5","E5","F5","G5","A5","C6"};
static const int    note_cnt    = 14;
static lv_obj_t    *scr         = NULL;

lv_obj_t *music_app_create(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, 222, 480);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a0a1a), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Back */
    lv_obj_t *bb = lv_btn_create(scr);
    lv_obj_set_size(bb, 50, 28);
    lv_obj_align(bb, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_add_event_cb(bb, [](lv_event_t *) {
        ledcWriteTone(LEDC_WHITE_CH, 0);
        ledcDetachPin(TONE_PIN);
        lv_obj_del(scr);
        scr = NULL;
    }, LV_EVENT_CLICKED, NULL);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Tone Player  " LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF88FF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);

    /* Info */
    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text(info, "No DAC on board\nPWM tones via vibrator");
    lv_obj_set_style_text_color(info, lv_color_hex(0x888888), 0);
    lv_obj_align(info, LV_ALIGN_CENTER, 0, -60);

    /* Piano keys */
    int x = 4;
    for (int i = 0; i < note_cnt && i < 14; i++) {
        lv_obj_t *key = lv_btn_create(scr);
        lv_obj_set_size(key, 28, 60);
        lv_obj_align(key, LV_ALIGN_BOTTOM_MID, x - 95, -50);
        lv_obj_set_style_bg_color(key, lv_color_hex(0x444488), 0);
        lv_obj_t *kl = lv_label_create(key);
        lv_label_set_text(kl, note_names[i]);
        lv_obj_set_style_text_color(kl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_add_event_cb(key, [](lv_event_t *ev) {
            int idx = (int)(uintptr_t)lv_event_get_user_data(ev);
            ledcSetup(LEDC_WHITE_CH, notes[idx], 8);
            ledcAttachPin(TONE_PIN, LEDC_WHITE_CH);
            ledcWriteTone(LEDC_WHITE_CH, notes[idx]);
            ledcWrite(LEDC_WHITE_CH, 32);
            static lv_timer_t *off = NULL;
            if (off) lv_timer_del(off);
            off = lv_timer_create([](lv_timer_t *) {
                ledcWrite(LEDC_WHITE_CH, 0);
            }, 400, NULL);
            lv_timer_set_repeat_count(off, 1);
        }, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        x += 15;
    }

    /* Stop button */
    lv_obj_t *stop = lv_btn_create(scr);
    lv_obj_set_size(stop, 60, 30);
    lv_obj_align(stop, LV_ALIGN_BOTTOM_MID, 0, -130);
    lv_obj_t *sl = lv_label_create(stop);
    lv_label_set_text(sl, LV_SYMBOL_STOP);
    lv_obj_add_event_cb(stop, [](lv_event_t *) {
        ledcWrite(LEDC_WHITE_CH, 0);
        ledcWriteTone(LEDC_WHITE_CH, 0);
    }, LV_EVENT_CLICKED, NULL);

    lv_scr_load(scr);
    return scr;
}
