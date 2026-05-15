/**
 * @file    app_pacman.cpp
 * @brief   Pacman game for T-Display-S3-Pro (222x480 LVGL)
 *
 * Classic maze-chase arcade game. All rendering via LVGL objects.
 * Touch controls via on-screen D-pad buttons.
 *
 * Maze: 13×20 cells (each 16px) = 208×320, fits within 222×480.
 * Layout (from top): score bar (28px), maze (320px), D-pad (132px) = 480px.
 */

#include "app_pacman.h"
#include "lvgl.h"
#include <stdlib.h>

/* ── display geometry ───────────────────────────────────────────────── */
#define SCR_W        222
#define SCR_H        480
#define CELL         16
#define MAZE_COLS    13
#define MAZE_ROWS    20
#define MAZE_OX      ((SCR_W - MAZE_COLS * CELL) / 2)   /* left margin  */
#define MAZE_OY      30                                  /* below score   */

/* ── classic Pacman maze (adapted for 13×20) ─────────────────────────── */
/* Legend:  0 = empty, 1 = wall, 2 = pellet, 3 = power pellet, 4 = ghost house */
static const uint8_t maze_init[MAZE_ROWS][MAZE_COLS] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,1,1,2,1,1,2,1},
    {1,3,1,1,2,1,1,1,2,1,1,3,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,2,1,2,1,1,2,1},
    {1,2,2,2,2,1,2,1,2,2,2,2,1},
    {1,1,1,1,2,1,0,1,2,1,1,1,1},
    {0,0,0,0,2,1,0,1,2,0,0,0,0},
    {0,0,0,0,2,0,4,0,2,0,0,0,0},
    {0,0,0,0,2,1,4,1,2,0,0,0,0},
    {0,0,0,0,2,1,4,1,2,0,0,0,0},
    {1,1,1,1,2,1,0,1,2,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,1,1,2,1,1,2,1},
    {1,3,2,2,2,2,2,2,2,2,2,3,1},
    {1,1,1,2,1,2,1,2,1,2,1,1,1},
    {1,2,2,2,1,2,1,2,1,2,2,2,1},
    {1,2,1,1,1,1,2,1,1,1,1,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,1},
};

/* ── runtime maze state (mutable copy) ──────────────────────────────── */
static uint8_t maze[MAZE_ROWS][MAZE_COLS];
static int     pellets_remaining;
static int     score;
static int     lives;

/* ── game object handles ─────────────────────────────────────────────── */
static lv_obj_t *maze_objs[MAZE_ROWS][MAZE_COLS];
static lv_obj_t *pacman_obj;
static lv_obj_t *ghost_obj[4];
static lv_obj_t *score_label;
static lv_obj_t *lives_label;

/* ── directions ──────────────────────────────────────────────────────── */
enum { DIR_UP = 0, DIR_DOWN, DIR_LEFT, DIR_RIGHT, DIR_NONE };
static int pacman_dir   = DIR_RIGHT;
static int pacman_next  = DIR_RIGHT;
static int ghost_dir[4] = {DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT};

/* ── positions (cell coordinates) ───────────────────────────────────── */
static int pacman_cx, pacman_cy;
static int ghost_cx[4], ghost_cy[4];

/* ── timers ──────────────────────────────────────────────────────────── */
static lv_timer_t *game_timer = NULL;
static lv_timer_t *ghost_timer = NULL;
static lv_timer_t *power_timer = NULL;
static bool power_mode = false;
static uint32_t power_end  = 0;

/* ── forward declarations ────────────────────────────────────────────── */
static void draw_maze(lv_obj_t *parent);
static void spawn_pacman(void);
static void spawn_ghosts(void);
static void move_pacman(void);
static bool can_move(int cx, int cy, int dir);
static void eat_pellet(int cx, int cy);
static void die(void);
static void game_over(void);
static void game_loop(lv_timer_t *t);
static void ghost_ai(lv_timer_t *t);
static void power_end_cb(lv_timer_t *t);
static void restart_game(void);

/* ── helper ──────────────────────────────────────────────────────────── */
static inline int cell_x(int cx) { return MAZE_OX + cx * CELL; }
static inline int cell_y(int cy) { return MAZE_OY + cy * CELL; }

/* ═══════════════════════════════════════════════════════════════════════
 *  PUBLIC
 * ═══════════════════════════════════════════════════════════════════════ */

lv_obj_t *pacman_game_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    /* ── score bar ────────────────────────────────────────────────── */
    score_label = lv_label_create(scr);
    lv_label_set_text(score_label, "Score: 0");
    lv_obj_set_style_text_color(score_label, lv_color_white(), 0);
    lv_obj_align(score_label, LV_ALIGN_TOP_LEFT, 4, 4);

    lives_label = lv_label_create(scr);
    lv_label_set_text(lives_label, "Lives: 3");
    lv_obj_set_style_text_color(lives_label, lv_color_white(), 0);
    lv_obj_align(lives_label, LV_ALIGN_TOP_RIGHT, -4, 4);

    /* ── back button ──────────────────────────────────────────────── */
    lv_obj_t *btn_back = lv_btn_create(scr);
    lv_obj_set_size(btn_back, 50, 24);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 60, 2);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Back");
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, [](lv_event_t *e) {
        lv_obj_t *scr = lv_obj_get_parent(lv_event_get_target(e));
        if (game_timer) { lv_timer_del(game_timer); game_timer = NULL; }
        if (ghost_timer) { lv_timer_del(ghost_timer); ghost_timer = NULL; }
        if (power_timer) { lv_timer_del(power_timer); power_timer = NULL; }
        lv_obj_del(scr);
    }, LV_EVENT_CLICKED, NULL);

    /* ── draw maze and start game ─────────────────────────────────── */
    draw_maze(scr);
    restart_game();

    /* ── D-pad controls (below maze) ──────────────────────────────── */
    int dpad_base = MAZE_OY + MAZE_ROWS * CELL + 16;

    auto make_btn = [&](int x, int y, int w, int h, const char *txt, int dir) {
        lv_obj_t *b = lv_btn_create(scr);
        lv_obj_set_size(b, w, h);
        lv_obj_set_pos(b, x, y);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, [](lv_event_t *e) {
            int d = (int)(intptr_t)lv_event_get_user_data(e);
            pacman_next = d;
        }, LV_EVENT_CLICKED, (void *)(intptr_t)dir);
    };

    make_btn(SCR_W/2 - 28, dpad_base - 60, 56, 56, LV_SYMBOL_UP,    DIR_UP);
    make_btn(SCR_W/2 - 28, dpad_base + 04, 56, 56, LV_SYMBOL_DOWN,  DIR_DOWN);
    make_btn(SCR_W/2 - 84, dpad_base - 28, 56, 56, LV_SYMBOL_LEFT,  DIR_LEFT);
    make_btn(SCR_W/2 + 28, dpad_base - 28, 56, 56, LV_SYMBOL_RIGHT, DIR_RIGHT);

    lv_scr_load(scr);
    return scr;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  INTERNAL
 * ═══════════════════════════════════════════════════════════════════════ */

static void restart_game(void)
{
    pellets_remaining = 0;
    for (int r = 0; r < MAZE_ROWS; r++)
        for (int c = 0; c < MAZE_COLS; c++) {
            maze[r][c] = maze_init[r][c];
            if (maze[r][c] == 2 || maze[r][c] == 3) pellets_remaining++;
        }
    score  = 0;
    lives  = 3;
    power_mode = false;

    lv_label_set_text_fmt(score_label, "Score: %d", score);
    lv_label_set_text_fmt(lives_label, "Lives: %d", lives);

    spawn_pacman();
    spawn_ghosts();

    if (game_timer) lv_timer_del(game_timer);
    game_timer = lv_timer_create(game_loop, 200, NULL);

    if (ghost_timer) lv_timer_del(ghost_timer);
    ghost_timer = lv_timer_create(ghost_ai, 300, NULL);
}

static void draw_maze(lv_obj_t *parent)
{
    for (int r = 0; r < MAZE_ROWS; r++) {
        for (int c = 0; c < MAZE_COLS; c++) {
            uint8_t cell = maze_init[r][c];
            if (cell == 0 || cell == 4) { maze_objs[r][c] = NULL; continue; }

            lv_obj_t *obj = lv_obj_create(parent);
            lv_obj_set_size(obj, CELL - 1, CELL - 1);
            lv_obj_set_pos(obj, cell_x(c), cell_y(r));
            lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

            if (cell == 1) {
                lv_obj_set_style_bg_color(obj, lv_color_hex(0x2121DE), 0);
                lv_obj_set_style_border_width(obj, 0, 0);
            } else if (cell == 2) {
                lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFB8AE), 0);
                lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_size(obj, 4, 4);
                lv_obj_set_pos(obj, cell_x(c) + 6, cell_y(r) + 6);
            } else if (cell == 3) {
                lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFB8AE), 0);
                lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_size(obj, 10, 10);
                lv_obj_set_pos(obj, cell_x(c) + 3, cell_y(r) + 3);
            }
            maze_objs[r][c] = obj;
        }
    }
}

static void refresh_cell(int r, int c)
{
    if (maze_objs[r][c]) {
        lv_obj_del(maze_objs[r][c]);
        maze_objs[r][c] = NULL;
    }
}

static void spawn_pacman(void)
{
    pacman_cx = 1;  pacman_cy = MAZE_ROWS - 2;
    pacman_dir  = DIR_RIGHT;
    pacman_next = DIR_RIGHT;

    if (pacman_obj) lv_obj_del(pacman_obj);
    pacman_obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(pacman_obj, CELL - 2, CELL - 2);
    lv_obj_set_style_bg_color(pacman_obj, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_radius(pacman_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(pacman_obj, 0, 0);
    lv_obj_set_pos(pacman_obj, cell_x(pacman_cx) + 1, cell_y(pacman_cy) + 1);
}

static void spawn_ghosts(void)
{
    static const lv_color_t gcolors[4] = {
        LV_COLOR_MAKE(0xFF, 0x00, 0x00),
        LV_COLOR_MAKE(0xFF, 0xB8, 0xFF),
        LV_COLOR_MAKE(0x00, 0xFF, 0xFF),
        LV_COLOR_MAKE(0xFF, 0xB8, 0x52),
    };
    int sx[4] = {5,7,6,6};
    int sy[4] = {9,9,10,11};
    for (int i = 0; i < 4; i++) {
        ghost_cx[i] = sx[i];  ghost_cy[i] = sy[i];
        ghost_dir[i] = DIR_UP;
        if (ghost_obj[i]) lv_obj_del(ghost_obj[i]);
        ghost_obj[i] = lv_obj_create(lv_scr_act());
        lv_obj_set_size(ghost_obj[i], CELL - 2, CELL - 2);
        lv_obj_set_style_bg_color(ghost_obj[i], gcolors[i], 0);
        lv_obj_set_style_radius(ghost_obj[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ghost_obj[i], 0, 0);
        lv_obj_set_pos(ghost_obj[i], cell_x(ghost_cx[i])+1, cell_y(ghost_cy[i])+1);
    }
}

static bool can_move(int cx, int cy, int dir)
{
    int nx = cx, ny = cy;
    if (dir == DIR_UP)    ny--;
    if (dir == DIR_DOWN)  ny++;
    if (dir == DIR_LEFT)  nx--;
    if (dir == DIR_RIGHT) nx++;
    if (nx < 0)          nx = MAZE_COLS - 1;
    if (nx >= MAZE_COLS) nx = 0;
    if (ny < 0 || ny >= MAZE_ROWS) return false;
    return maze[ny][nx] != 1;
}

static void eat_pellet(int cx, int cy)
{
    if (maze[cy][cx] == 2) {
        maze[cy][cx] = 0;
        score += 10;
        pellets_remaining--;
        refresh_cell(cy, cx);
    } else if (maze[cy][cx] == 3) {
        maze[cy][cx] = 0;
        score += 50;
        pellets_remaining--;
        refresh_cell(cy, cx);
        power_mode = true;
        power_end  = lv_tick_get() + 7000;
        if (power_timer) lv_timer_del(power_timer);
        power_timer = lv_timer_create(power_end_cb, 100, NULL);
    }
    lv_label_set_text_fmt(score_label, "Score: %d", score);
    if (pellets_remaining <= 0) restart_game();
}

static void die(void)
{
    lives--;
    lv_label_set_text_fmt(lives_label, "Lives: %d", lives);
    if (lives <= 0) { game_over(); return; }
    if (pacman_obj) { lv_obj_del(pacman_obj); pacman_obj = NULL; }
    for (int i = 0; i < 4; i++)
        if (ghost_obj[i]) { lv_obj_del(ghost_obj[i]); ghost_obj[i] = NULL; }
    spawn_pacman();
    spawn_ghosts();
    power_mode = false;
    if (power_timer) { lv_timer_del(power_timer); power_timer = NULL; }
}

static void game_over(void)
{
    if (game_timer)  { lv_timer_del(game_timer);  game_timer  = NULL; }
    if (ghost_timer) { lv_timer_del(ghost_timer); ghost_timer = NULL; }
    if (power_timer) { lv_timer_del(power_timer); power_timer = NULL; }
    lv_obj_t *mbox = lv_msgbox_create(NULL, "Game Over",
        "Tap anywhere to restart", NULL, true);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, [](lv_event_t *e) {
        lv_obj_del(lv_event_get_current_target(e));
        restart_game();
    }, LV_EVENT_CLICKED, NULL);
}

static void game_loop(lv_timer_t *t)
{
    (void)t;
    if (pacman_next != pacman_dir && can_move(pacman_cx, pacman_cy, pacman_next))
        pacman_dir = pacman_next;
    if (!can_move(pacman_cx, pacman_cy, pacman_dir)) return;
    move_pacman();
    for (int i = 0; i < 4; i++) {
        if (ghost_cx[i] == pacman_cx && ghost_cy[i] == pacman_cy) {
            if (power_mode) {
                score += 200;
                lv_label_set_text_fmt(score_label, "Score: %d", score);
                ghost_cx[i] = 6; ghost_cy[i] = 10;
                lv_obj_set_pos(ghost_obj[i], cell_x(6)+1, cell_y(10)+1);
            } else { die(); return; }
        }
    }
}

static void move_pacman(void)
{
    int nx = pacman_cx, ny = pacman_cy;
    if (pacman_dir == DIR_UP)    ny--;
    if (pacman_dir == DIR_DOWN)  ny++;
    if (pacman_dir == DIR_LEFT)  nx--;
    if (pacman_dir == DIR_RIGHT) nx++;
    if (nx < 0) nx = MAZE_COLS - 1;
    if (nx >= MAZE_COLS) nx = 0;
    if (ny < 0 || ny >= MAZE_ROWS) return;
    pacman_cx = nx; pacman_cy = ny;
    lv_obj_set_pos(pacman_obj, cell_x(pacman_cx)+1, cell_y(pacman_cy)+1);
    eat_pellet(pacman_cx, pacman_cy);
}

static void ghost_ai(lv_timer_t *t)
{
    (void)t;
    for (int i = 0; i < 4; i++) {
        int best_dir = ghost_dir[i];
        int best_dist = 999;
        if ((lv_tick_get() / 1000) % 7 == 0) {
            int dirs[4] = {DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT};
            for (int attempt = 0; attempt < 8; attempt++) {
                int d = dirs[rand() % 4];
                if (can_move(ghost_cx[i], ghost_cy[i], d)) { best_dir = d; break; }
            }
        } else {
            int dirs[4] = {DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT};
            for (int j = 0; j < 4; j++) {
                int d = dirs[j];
                if (!can_move(ghost_cx[i], ghost_cy[i], d)) continue;
                int nx = ghost_cx[i], ny = ghost_cy[i];
                if (d == DIR_UP) ny--; if (d == DIR_DOWN) ny++;
                if (d == DIR_LEFT) nx--; if (d == DIR_RIGHT) nx++;
                if (nx < 0) nx = MAZE_COLS - 1; if (nx >= MAZE_COLS) nx = 0;
                int dist = abs(nx - pacman_cx) + abs(ny - pacman_cy);
                if (dist < best_dist) { best_dist = dist; best_dir = d; }
            }
        }
        if (!can_move(ghost_cx[i], ghost_cy[i], best_dir)) {
            int dirs[4] = {DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT};
            bool found = false;
            for (int j = 0; j < 4; j++)
                if (can_move(ghost_cx[i], ghost_cy[i], dirs[j]))
                    { best_dir = dirs[j]; found = true; break; }
            if (!found) continue;
        }
        int nx = ghost_cx[i], ny = ghost_cy[i];
        if (best_dir == DIR_UP) ny--; if (best_dir == DIR_DOWN) ny++;
        if (best_dir == DIR_LEFT) nx--; if (best_dir == DIR_RIGHT) nx++;
        if (nx < 0) nx = MAZE_COLS - 1; if (nx >= MAZE_COLS) nx = 0;
        ghost_cx[i] = nx; ghost_cy[i] = ny;
        ghost_dir[i] = best_dir;
        lv_obj_set_pos(ghost_obj[i], cell_x(nx)+1, cell_y(ny)+1);
    }
}

static void power_end_cb(lv_timer_t *t)
{
    if (lv_tick_get() >= power_end) {
        power_mode = false;
        lv_timer_del(t);
        power_timer = NULL;
    }
}
