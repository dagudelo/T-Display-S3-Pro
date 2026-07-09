/**
 * @file    app_pacman.cpp
 * @brief   Classic Pac-Man — 21×23 maze, ghost AI, fruit, level progression.
 *
 * T-Display-S3-Pro: 222×480 ST7796S LCD, CST226SE capacitive touch.
 * All game objects are children of the game screen; deleting it cleans up.
 * Touch D-pad uses LV_EVENT_PRESSING for auto-repeat.
 */
#include "app_pacman.h"
#include "lvgl.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── Display & layout ─────────────────────────────────────────────── */
#define SCR_W     222
#define SCR_H     480
#define CELL      10          /* pixels per tile */
#define COLS      PM_COLS     /* 21 */
#define ROWS      PM_ROWS     /* 23 */
#define OX        ((SCR_W - COLS * CELL) / 2)   /* x=6 */
#define OY        20          /* top margin for compact HUD */
#define MAZE_W    (COLS * CELL)   /* 210 */
#define MAZE_H    (ROWS * CELL)   /* 230 */

/* ── Direction ────────────────────────────────────────────────────── */
enum Dir : uint8_t { DIR_UP=0, DIR_DOWN=1, DIR_LEFT=2, DIR_RIGHT=3, DIR_NONE=4 };
static const int8_t dx[4] = { 0, 0, -1, 1 };
static const int8_t dy[4] = { -1, 1, 0, 0 };
static Dir opposite(Dir d) {
    if (d==DIR_UP)   return DIR_DOWN;
    if (d==DIR_DOWN) return DIR_UP;
    if (d==DIR_LEFT) return DIR_RIGHT;
    return DIR_LEFT;
}

/* ── Ghost types ──────────────────────────────────────────────────── */
enum GhostType : uint8_t { BLINKY=0, PINKY=1, INKY=2, CLYDE=3 };

/* ── Ghost mode ───────────────────────────────────────────────────── */
enum GhostMode : uint8_t {
    GM_SCATTER, GM_CHASE, GM_FRIGHTENED,
    GM_HOUSE, GM_LEAVING, GM_EATEN
};

/* ── Classic 21×23 arcade maze ────────────────────────────────────── */
/* Encoding: 0=empty 1=wall 2=dot 3=energizer 4=ghost-house-interior */
static const uint8_t default_maze[ROWS][COLS] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,1},
    {1,3,1,1,2,1,1,1,2,1,2,1,2,1,2,1,1,1,2,3,1},
    {1,2,2,2,2,2,2,2,2,1,2,1,2,1,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,2,1,1,1,2,1,2,1,1,1,2,1,1,2,1},
    {1,2,2,2,2,1,2,2,2,0,2,2,2,0,2,2,2,1,2,2,1},
    {1,1,1,1,2,1,1,1,0,1,0,1,0,1,0,1,1,1,2,1,1},
    {0,0,0,1,2,1,0,0,0,0,0,0,0,0,0,0,0,1,2,0,0},
    {1,1,1,1,2,1,1,1,0,1,0,1,1,1,0,1,1,1,2,1,1},
    {0,0,0,0,2,0,0,0,0,1,4,4,4,1,0,0,0,0,2,0,0},
    {0,0,0,0,2,1,0,0,0,1,4,4,4,1,0,0,0,1,2,0,0},
    {1,1,1,1,2,1,0,0,0,1,4,4,4,1,0,0,0,1,2,1,1},
    {0,0,0,0,2,1,0,0,0,1,1,1,1,1,0,0,0,1,2,0,0},
    {1,1,1,1,2,1,0,0,0,0,0,0,0,0,0,0,0,1,2,1,1},
    {0,0,0,1,2,1,0,0,0,1,1,1,1,1,0,0,0,1,2,0,0},
    {1,1,1,1,2,1,0,0,0,1,0,0,0,1,0,0,0,1,2,1,1},
    {1,2,2,2,2,2,2,2,2,2,2,0,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,1,1,2,1,1,1,1,1,2,1,1,1,2,1,1},
    {1,2,2,1,2,2,2,2,2,0,0,0,0,0,2,2,2,2,2,2,1},
    {1,1,2,1,2,1,2,1,1,1,1,1,1,1,1,1,2,1,2,1,1},
    {1,2,2,2,2,1,2,2,2,0,2,2,2,0,2,2,2,1,2,2,1},
    {1,3,1,1,1,1,1,1,2,1,1,1,1,1,2,1,1,1,1,3,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
};

/* ── Ghost colour table ───────────────────────────────────────────── */
static const lv_color_t ghost_colors[4] = {
    LV_COLOR_MAKE(0xFF,0x00,0x00), /* Blinky – red       */
    LV_COLOR_MAKE(0xFF,0xB8,0xFF), /* Pinky  – pink      */
    LV_COLOR_MAKE(0x00,0xFF,0xFF), /* Inky   – cyan      */
    LV_COLOR_MAKE(0xFF,0xB8,0x52), /* Clyde  – orange    */
};
static const lv_color_t color_frightened = LV_COLOR_MAKE(0x21,0x21,0xDE);
static const lv_color_t color_fright_flash = LV_COLOR_MAKE(0xFF,0xFF,0xFF);
static const lv_color_t color_pacman      = LV_COLOR_MAKE(0xFF,0xFF,0x00);
static const lv_color_t color_wall        = LV_COLOR_MAKE(0x21,0x21,0xDE);
static const lv_color_t color_dot         = LV_COLOR_MAKE(0xFF,0xB8,0xAE);
static const lv_color_t color_white       = LV_COLOR_MAKE(0xFF,0xFF,0xFF);
static const lv_color_t color_black       = LV_COLOR_MAKE(0x00,0x00,0x00);
static const lv_color_t color_eye_white   = LV_COLOR_MAKE(0xFF,0xFF,0xFF);
static const lv_color_t color_pupil       = LV_COLOR_MAKE(0x00,0x00,0xFF);

/* ── Position helpers ─────────────────────────────────────────────── */
static inline int cx(int c) { return OX + c * CELL; }
static inline int cy(int r) { return OY + r * CELL; }

/* ── Scatter corners ──────────────────────────────────────────────── */
static const uint8_t scatter_col[4] = {20, 1, 20, 1};   /* TR, TL, BR, BL */
static const uint8_t scatter_row[4] = { 1, 1, 21, 21};

/* ── Dot limits for ghost house exit (classic) ───────────────────── */
static const uint8_t dot_limits[4] = {0, 0, 30, 60}; /* B,P,I,C */

/* ── Fruit table ──────────────────────────────────────────────────── */
static const uint16_t fruit_scores[] = {100,300,500,700,1000,2000,3000,5000};
static const char    *fruit_names[] = {"Cherry","Strawberry","Orange","Apple",
                                       "Melon","Galaxian","Bell","Key"};
static uint8_t fruit_for_level(uint8_t lv) {
    if (lv<=1) return 0; if (lv<=2) return 1; if (lv<=4) return 2;
    if (lv<=6) return 3; if (lv<=8) return 4; if (lv<=10) return 5;
    if (lv<=12) return 6; return 7;
}

/* ── Ghost mode timing (seconds, per phase) ──────────────────────── */
static const uint16_t scatter_times_l1[] = {7,7,5,5};
static const uint16_t chase_times_l1[]   = {20,20,20,0xFFFF};
static const uint16_t scatter_times_l24[] = {7,7,5,1};
static const uint16_t chase_times_l24[]   = {20,20,1033,0xFFFF};
static const uint16_t scatter_times_l5[]  = {5,5,5,1};
static const uint16_t chase_times_l5[]    = {20,20,1037,0xFFFF};
static const uint8_t fright_times[] = {6,5,4,3,2,5,2,2,1,5,2,1,1,3,1,1,0,1,0,0};

/* ── Speed tables (% of base at 100ms/tick) ──────────────────────── */
/* Pac-Man base: 80% L1, 90% L2-4, 100% L5-20, 90% L21+              */
/* Ghost base:   75% L1, 85% L2-4, 95% L5+                            */
/* Returns number of ticks to skip between moves (0 = move every tick) */
static uint8_t pacman_speed_skip(uint8_t lv) {
    if (lv==1) return 2;   /* 80% -> move every 3rd tick */
    if (lv<=4) return 1;   /* 90% -> move every 2nd tick */
    if (lv<=20) return 0;  /* 100% */
    return 1;              /* 90% L21+ */
}
static uint8_t ghost_speed_skip(uint8_t lv) {
    if (lv==1) return 2;   /* 75% */
    if (lv<=4) return 1;   /* 85% */
    return 0;              /* 95% */
}
static bool fright_skip(void) { return true; }  /* move every 2nd tick (~50%) */

/* ── Elroy dot thresholds ─────────────────────────────────────────── */
static uint8_t elroy1_thresh(uint8_t lv) {
    if (lv==1) return 20; if (lv==2) return 30; if (lv<=5) return 40;
    if (lv<=8) return 50; if (lv<=11) return 60; if (lv<=14) return 70;
    if (lv<=18) return 80; return 90;
}
static uint8_t elroy2_thresh(uint8_t lv) {
    if (lv==1) return 10; if (lv==2) return 15; if (lv<=5) return 20;
    if (lv<=8) return 25; if (lv<=11) return 30; if (lv<=14) return 35;
    if (lv<=18) return 40; return 45;
}

/* ── Global game state ────────────────────────────────────────────── */
static lv_obj_t  *g_scr;
static lv_obj_t  *maze_obj[ROWS][COLS];
static lv_obj_t  *pacman_obj, *ghost_obj[4];
static lv_obj_t  *fruit_obj, *fruit_label;
static lv_obj_t  *score_lbl, *lives_lbl, *level_lbl;
static lv_obj_t  *ghost_canvas[4], *pacman_canvas;
static lv_obj_t  *score_popup;
static lv_obj_t  *go_label;      /* GAME OVER label */
static lv_timer_t *game_timer;
static lv_timer_t *energizer_timer;
static bool       bonus_given;   /* extra life already awarded */

/* Maze runtime state */
static uint8_t maze[ROWS][COLS];
static uint8_t maze_base[ROWS][COLS];
static bool    maze_loaded;

/* Game state */
static int     score, lives, level;
static int     dots_eaten, total_dots;
static bool    game_over, level_clear;
static uint8_t tick_count;

/* Pac-Man */
static uint8_t pm_col, pm_row;
static Dir      pm_dir, pm_next_dir;
static uint8_t pm_skip_ctr;
static uint8_t pm_mouth;      /* 0-4 mouth animation */
static bool    pm_dying;
static uint8_t pm_death_frame;

/* Ghosts */
static uint8_t  gh_col[4], gh_row[4];
static Dir       gh_dir[4];
static GhostMode gh_mode[4];
static uint8_t   gh_dots_eaten[4]; /* dot counter for house exit */
static uint8_t   gh_skip_ctr[4];
static bool      gh_flash;         /* toggle for frightened flash */

/* Mode system */
static GhostMode global_mode;  /* SCATTER or CHASE */
static uint8_t   mode_phase;
static uint32_t  mode_secs_remaining;
static uint32_t  mode_tick_start;
static uint32_t  fright_start;
static uint8_t   fright_duration;
static uint8_t   ghost_chain;    /* 0-3: consecutive ghosts eaten */

/* Fruit */
static bool     fruit_active, fruit_eaten;
static uint8_t  fruit_type, fruit_col, fruit_row;
static uint32_t fruit_spawn_tick;
static uint8_t  fruit_phase;   /* 0=none, 1=first (70), 2=second (170) */

/* Score popup */
static uint8_t  popup_col, popup_row;
static uint32_t popup_end;

/* Helper: tick in seconds */
static uint32_t tick_sec(void) { return lv_tick_get() / 1000; }

/* ── Forward decls ────────────────────────────────────────────────── */
static void restart_level(void);
static void restart_game(void);
static void refresh_cell(int r, int c);
static bool can_move(int x, int y, Dir d);
static Dir  ghost_choose_dir(int idx, int tx, int ty, bool use_scatter);
static void move_pacman(void);
static void move_ghost(int idx);
static void check_collisions(void);
static void update_modes(void);
static void draw_ghost_canvas(int idx);
static void draw_pacman_canvas(void);
static void draw_fruit(void);

/* ── lv_canvas drawing helpers ────────────────────────────────────── */

/* Draw a classic ghost shape on CELL×CELL canvas */
static void draw_ghost_sprite(lv_obj_t *cv, lv_color_t body, bool eyes_only,
                               bool frightened, bool flash) {
    lv_canvas_fill_bg(cv, lv_color_black(), LV_OPA_COVER);
    lv_img_dsc_t *img = lv_canvas_get_img(cv);
    if (!img || !img->data) return;
    lv_color_t *buf = (lv_color_t*)img->data;
    int w = CELL, h = CELL;

    if (eyes_only) {
        /* Just eyes: white dots with blue pupils */
        for (int py=3; py<=4; py++) {
            if (py==3) { buf[2+py*w]=color_eye_white; buf[4+py*w]=color_eye_white;
                         buf[3+py*w]=lv_color_black(); buf[5+py*w]=lv_color_black(); }
            else       { buf[2+py*w]=color_eye_white; buf[4+py*w]=color_eye_white; }
        }
        return;
    }

    if (frightened) {
        body = flash ? color_fright_flash : color_frightened;
        /* Blue body with wavy mouth */
        for (int py=2; py<=7; py++) {
            for (int px=2; px<=7; px++) {
                if (py<=6) { if(px>=2&&px<=7) buf[px+py*w]=body; }
                else if (py==7) { if(px==2||px==3||px==5||px==6||px==7) buf[px+py*w]=body; }
            }
        }
        /* Eyes */
        buf[3+3*w]=lv_color_black(); buf[5+3*w]=lv_color_black();
        /* Wavy mouth */
        buf[3+5*w]=color_white; buf[4+5*w]=color_white; buf[5+5*w]=color_white;
        buf[2+6*w]=color_white; buf[5+6*w]=color_white;
        return;
    }

    /* Normal ghost body */
    for (int py=2; py<=7; py++) {
        for (int px=1; px<=8; px++) {
            if (py<=5) { if(px>=1&&px<=8) buf[px+py*w]=body; }
            else if (py==6) { if(px==0||px==1||px==3||px==4||px==6||px==7||px==8) buf[px+py*w]=body; }
            else if (py==7) { if(px==1||px==2||px==4||px==5||px==7||px==8) buf[px+py*w]=body; }
        }
    }
    /* Eyes */
    buf[2+3*w]=color_eye_white; buf[3+3*w]=lv_color_black(); buf[4+3*w]=color_pupil;
    buf[5+3*w]=color_eye_white; buf[6+3*w]=lv_color_black(); buf[7+3*w]=color_pupil;
    buf[3+4*w]=color_eye_white; buf[4+4*w]=color_pupil;
    buf[5+4*w]=color_eye_white; buf[6+4*w]=color_pupil;
}

/* Draw Pac-Man on CELL×CELL canvas */
static void draw_pacman_sprite(lv_obj_t *cv, Dir dir, uint8_t mouth) {
    lv_img_dsc_t *img = lv_canvas_get_img(cv);
    if (!img || !img->data) return;
    lv_color_t *buf = (lv_color_t*)img->data;
    int w = CELL, h = CELL, cx = w/2, cy = h/2, r = w/2 - 1;
    int a1=0, a2=0; /* mouth angles in degrees */
    if (mouth==0) { a1=0; a2=360; }
    else if (mouth<=2) { a1=30*mouth; a2=360-30*mouth; }
    else { a1=30*(4-mouth); a2=360-30*(4-mouth); }

    /* Direction angle offset */
    int doff = 0;
    if (dir==DIR_UP) doff=270; else if(dir==DIR_DOWN) doff=90;
    else if(dir==DIR_LEFT) doff=180;

    for (int py=0; py<h; py++) for (int px=0; px<w; px++) {
        int dx=px-cx, dy=py-cy;
        if (dx*dx+dy*dy <= r*r) {
            /* Check mouth cut */
            int ang = (int)(atan2(-dy, dx)*180/3.14159);
            if (ang<0) ang+=360;
            ang = (ang - doff + 360) % 360;
            if (mouth==0 || (ang>=a1 && ang<=a2))
                buf[px+py*w] = color_pacman;
            else
                buf[px+py*w] = color_black;
        } else {
            buf[px+py*w] = color_black;
        }
    }
}

/* ── Maze drawing ─────────────────────────────────────────────────── */

static void draw_maze(void) {
    for (int r=0; r<ROWS; r++) for (int c=0; c<COLS; c++) {
        uint8_t cell = maze_base[r][c];
        if (cell==0) { maze_obj[r][c]=NULL; continue; }
        lv_obj_t *o = lv_obj_create(g_scr);
        lv_obj_set_pos(o, cx(c), cy(r));
        lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_border_width(o, 0, 0);
        if (cell==1) {
            lv_obj_set_size(o, CELL, CELL);
            lv_obj_set_style_bg_color(o, color_wall, 0);
        } else if (cell==2) {
            lv_obj_set_size(o, 3, 3);
            lv_obj_set_pos(o, cx(c)+4, cy(r)+4);
            lv_obj_set_style_bg_color(o, color_dot, 0);
            lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        } else if (cell==3) {
            lv_obj_set_size(o, 7, 7);
            lv_obj_set_pos(o, cx(c)+2, cy(r)+2);
            lv_obj_set_style_bg_color(o, color_dot, 0);
            lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        } else if (cell==4) {
            lv_obj_set_size(o, CELL, CELL);
            lv_obj_set_style_bg_color(o, lv_color_hex(0x333333), 0);
        }
        maze_obj[r][c] = o;
    }
}

/* ── Pac-Man lv_obj (canvas-backed) ───────────────────────────────── */

static void create_pacman_obj(void) {
    lv_color_t *buf = (lv_color_t*)lv_mem_alloc(CELL*CELL*sizeof(lv_color_t));
    pacman_canvas = lv_canvas_create(g_scr);
    lv_canvas_set_buffer(pacman_canvas, buf, CELL, CELL, LV_IMG_CF_TRUE_COLOR);
    pacman_obj = pacman_canvas;
    lv_obj_set_pos(pacman_obj, cx(pm_col)+1, cy(pm_row)+1);
    draw_pacman_sprite(pacman_canvas, pm_dir, 0);
}

static void create_ghost_obj(int idx) {
    lv_color_t *buf = (lv_color_t*)lv_mem_alloc(CELL*CELL*sizeof(lv_color_t));
    ghost_canvas[idx] = lv_canvas_create(g_scr);
    lv_canvas_set_buffer(ghost_canvas[idx], buf, CELL, CELL, LV_IMG_CF_TRUE_COLOR);
    ghost_obj[idx] = ghost_canvas[idx];
    lv_obj_set_pos(ghost_obj[idx], cx(gh_col[idx])+1, cy(gh_row[idx])+1);
    draw_ghost_canvas(idx);
}

static void draw_ghost_canvas(int idx) {
    bool eyes_only = (gh_mode[idx]==GM_EATEN);
    bool fright = (gh_mode[idx]==GM_FRIGHTENED);
    bool flash = fright && (fright_duration>0 && ((fright_start-lv_tick_get())/1000)<=2) && gh_flash;
    lv_color_t body = ghost_colors[idx];
    draw_ghost_sprite(ghost_canvas[idx], body, eyes_only, fright, flash);
}

static void draw_pacman_canvas(void) {
    draw_pacman_sprite(pacman_canvas, pm_dir, pm_mouth);
}

/* ── Movement helpers ─────────────────────────────────────────────── */

static bool can_move(int x, int y, Dir d) {
    int nx=x+dx[d], ny=y+dy[d];
    if (ny<0||ny>=ROWS) return false;
    if (nx<0) nx=COLS-1; if (nx>=COLS) nx=0;
    return maze[ny][nx]!=1 && maze[ny][nx]!=4;
}

static bool can_move_ghost(int x, int y, Dir d) {
    int nx=x+dx[d], ny=y+dy[d];
    if (ny<0||ny>=ROWS) return false;
    if (nx<0) nx=COLS-1; if (nx>=COLS) nx=0;
    uint8_t c = maze[ny][nx];
    return c!=1;  /* ghosts can pass through house interior */
}

static bool can_move_ghost_no_house(int x, int y, Dir d) {
    int nx=x+dx[d], ny=y+dy[d];
    if (ny<0||ny>=ROWS) return false;
    if (nx<0) nx=COLS-1; if (nx>=COLS) nx=0;
    uint8_t c = maze[ny][nx];
    return c!=1 && c!=4;
}

static bool is_intersection(int x, int y) {
    /* Count walkable exits */
    int cnt=0;
    if (can_move(x,y,DIR_UP)) cnt++;
    if (can_move(x,y,DIR_DOWN)) cnt++;
    if (can_move(x,y,DIR_LEFT)) cnt++;
    if (can_move(x,y,DIR_RIGHT)) cnt++;
    if (cnt>=3) return true;
    /* Also intersection if only two exits and they're not opposite */
    if (cnt==2) {
        bool u=can_move(x,y,DIR_UP), d=can_move(x,y,DIR_DOWN);
        bool l=can_move(x,y,DIR_LEFT), r=can_move(x,y,DIR_RIGHT);
        if (u&&d) return false;
        if (l&&r) return false;
        return true;
    }
    return false;
}

/* ── Ghost AI ─────────────────────────────────────────────────────── */

static void ghost_target(int idx, int *tx, int *ty) {
    switch (gh_mode[idx]) {
    case GM_SCATTER:
        *tx = scatter_col[idx]; *ty = scatter_row[idx]; return;
    case GM_CHASE:
        switch ((GhostType)idx) {
        case BLINKY:
            *tx = pm_col; *ty = pm_row; return;
        case PINKY: {
            *tx = pm_col + dx[pm_dir]*4;
            *ty = pm_row + dy[pm_dir]*4;
            /* Classic bug: UP overflows left */
            if (pm_dir==DIR_UP) *tx -= 4;
            return;
        }
        case INKY: {
            int px = pm_col + dx[pm_dir]*2;
            int py = pm_row + dy[pm_dir]*2;
            if (pm_dir==DIR_UP) px -= 2;
            *tx = 2*px - gh_col[BLINKY];
            *ty = 2*py - gh_row[BLINKY];
            return;
        }
        case CLYDE: {
            int dist = abs(gh_col[idx]-pm_col) + abs(gh_row[idx]-pm_row);
            if (dist >= 8) { *tx = pm_col; *ty = pm_row; }
            else           { *tx = scatter_col[idx]; *ty = scatter_row[idx]; }
            return;
        }
        }
    case GM_FRIGHTENED:
        *tx = -1; *ty = -1; return; /* random */
    case GM_EATEN:
        *tx = 10; *ty = 9; return;  /* ghost house entrance */
    default:
        *tx = 10; *ty = 9; return;
    }
}

/* Distance: Manhattan */
static int tile_dist(int x1, int y1, int x2, int y2) {
    return abs(x1-x2) + abs(y1-y2);
}

/* Choose direction: classic priority UP, LEFT, DOWN, RIGHT on ties */
static const Dir dir_priority[] = {DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT};

static Dir ghost_choose_dir(int idx, int tx, int ty, bool use_scatter) {
    Dir reverse = opposite(gh_dir[idx]);
    Dir best = DIR_NONE;
    int best_dist = 999;

    if (use_scatter) { tx = scatter_col[idx]; ty = scatter_row[idx]; }

    for (int i=0; i<4; i++) {
        Dir d = dir_priority[i];
        if (d==reverse) continue;
        if (!can_move_ghost(gh_col[idx], gh_row[idx], d)) continue;
        int nx = gh_col[idx]+dx[d], ny = gh_row[idx]+dy[d];
        if (nx<0) nx=COLS-1; if (nx>=COLS) nx=0;
        int dist;
        if (gh_mode[idx]==GM_FRIGHTENED) {
            dist = rand() & 0xFF; /* random */
        } else {
            dist = tile_dist(nx, ny, tx, ty);
        }
        if (dist < best_dist) { best_dist = dist; best = d; }
    }
    return best;
}

/* ── Game logic ───────────────────────────────────────────────────── */

static void move_pacman(void) {
    if (pm_dying) return;
    pm_skip_ctr++;
    if (pm_skip_ctr <= pacman_speed_skip(level)) return;
    pm_skip_ctr = 0;

    /* Try buffered direction at intersections */
    if (is_intersection(pm_col, pm_row) || pm_dir==DIR_NONE) {
        if (pm_next_dir!=pm_dir && can_move(pm_col, pm_row, pm_next_dir))
            pm_dir = pm_next_dir;
    }
    if (!can_move(pm_col, pm_row, pm_dir)) return;

    int nx=pm_col+dx[pm_dir], ny=pm_row+dy[pm_dir];
    if (nx<0) nx=COLS-1; if (nx>=COLS) nx=0;
    if (ny<0||ny>=ROWS) return;

    pm_col=nx; pm_row=ny;
    lv_obj_set_pos(pacman_obj, cx(nx), cy(ny));

    /* Eating */
    uint8_t *cell = &maze[ny][nx];
    if (*cell==2) { *cell=0; score+=10; dots_eaten++; refresh_cell(ny,nx); }
    else if (*cell==3) {
        *cell=0; score+=50; dots_eaten++; refresh_cell(ny,nx);
        /* Power up! */
        for (int i=0; i<4; i++) {
            if (gh_mode[i]==GM_CHASE || gh_mode[i]==GM_SCATTER) {
                gh_mode[i]=GM_FRIGHTENED;
                gh_dir[i]=opposite(gh_dir[i]); /* reverse */
            }
        }
        ghost_chain=0;
        fright_start=lv_tick_get();
        fright_duration=fright_times[(level-1<20)?(level-1):19];
    }
    /* Fruit spawn */
    if (dots_eaten==70 && fruit_phase==0) {
        fruit_active=true; fruit_eaten=false;
        fruit_type=fruit_for_level(level);
        fruit_col=10; fruit_row=14;
        fruit_spawn_tick=lv_tick_get();
        fruit_phase=1;
        draw_fruit();
    } else if (dots_eaten==170 && fruit_phase==1) {
        fruit_active=true; fruit_eaten=false;
        fruit_type=fruit_for_level(level);
        fruit_col=10; fruit_row=14;
        fruit_spawn_tick=lv_tick_get();
        fruit_phase=2;
        draw_fruit();
    }
    /* Bonus life */
    if (!bonus_given && score>=10000) {
        lives++; bonus_given=true;
        lv_label_set_text_fmt(lives_lbl,"%d",lives);
    }
    /* Level clear */
    if (dots_eaten >= total_dots) {
        level_clear=true;
    }
}

static void move_ghost(int idx) {
    if (gh_mode[idx]==GM_HOUSE) return;

    gh_skip_ctr[idx]++;
    uint8_t base_skip = ghost_speed_skip(level);
    /* Cruise Elroy: Blinky speeds up when dots are low */
    if (idx==BLINKY) {
        uint8_t remaining = total_dots - dots_eaten;
        if (remaining <= elroy2_thresh(level)) base_skip = 0; /* Elroy 2: full speed */
        else if (remaining <= elroy1_thresh(level)) base_skip = (base_skip>0)?(base_skip-1):0; /* Elroy 1 */
    }
    uint8_t skip = base_skip;
    if (gh_mode[idx]==GM_FRIGHTENED && fright_skip()) skip++;
    if (gh_mode[idx]==GM_EATEN) skip=0; /* Eaten ghosts move fast (~200%) */
    /* Tunnel slowdown */
    if (gh_col[idx]<=1 || gh_col[idx]>=COLS-2) skip++;
    if (gh_skip_ctr[idx] <= skip) return;
    gh_skip_ctr[idx]=0;

    /* Leaving house */
    if (gh_mode[idx]==GM_LEAVING) {
        /* Move toward door (col 10, row 9) and exit up */
        if (gh_col[idx]==10 && gh_row[idx]==9) {
            gh_mode[idx] = (global_mode==GM_SCATTER) ? GM_SCATTER : GM_CHASE;
            gh_dir[idx] = DIR_UP;
        } else {
            Dir best=DIR_NONE; int bd=999;
            Dir rev=opposite(gh_dir[idx]);
            for (int i=0;i<4;i++){Dir d=dir_priority[i];if(d==rev)continue;
                if(!can_move_ghost(gh_col[idx],gh_row[idx],d))continue;
                int nx=gh_col[idx]+dx[d],ny=gh_row[idx]+dy[d];
                int dist=tile_dist(nx,ny,10,9);
                if(dist<bd){bd=dist;best=d;}}
            if (best!=DIR_NONE) gh_dir[idx]=best;
        }
    }
    /* Eaten: race to house */
    else if (gh_mode[idx]==GM_EATEN) {
        if (gh_col[idx]>=10 && gh_col[idx]<=12 && gh_row[idx]>=9 && gh_row[idx]<=11) {
            /* Arrived at house */
            gh_mode[idx]=GM_LEAVING;
            gh_col[idx]=11; gh_row[idx]=10;
        } else {
            Dir best=DIR_NONE; int bd=999;
            Dir rev=opposite(gh_dir[idx]);
            for (int i=0;i<4;i++){Dir d=dir_priority[i];if(d==rev)continue;
                if(!can_move_ghost_no_house(gh_col[idx],gh_row[idx],d))continue;
                int nx=gh_col[idx]+dx[d],ny=gh_row[idx]+dy[d];
                int dist=tile_dist(nx,ny,10,9);
                if(dist<bd){bd=dist;best=d;}}
            if (best!=DIR_NONE) gh_dir[idx]=best;
        }
    }
    /* Normal/frightened: choose direction at intersection */
    else if (is_intersection(gh_col[idx], gh_row[idx])) {
        int tx, ty;
        ghost_target(idx, &tx, &ty);
        Dir best = ghost_choose_dir(idx, tx, ty, false);
        if (best!=DIR_NONE) gh_dir[idx]=best;
    }

    /* Move */
    if (!can_move_ghost(gh_col[idx], gh_row[idx], gh_dir[idx])) return;
    if (gh_mode[idx]==GM_EATEN && !can_move_ghost_no_house(gh_col[idx],gh_row[idx],gh_dir[idx])) return;

    int nx=gh_col[idx]+dx[gh_dir[idx]], ny=gh_row[idx]+dy[gh_dir[idx]];
    if (nx<0) nx=COLS-1; if (nx>=COLS) nx=0;
    gh_col[idx]=nx; gh_row[idx]=ny;
    lv_obj_set_pos(ghost_obj[idx], cx(nx), cy(ny));
    draw_ghost_canvas(idx);
}

static void check_collisions(void) {
    if (pm_dying) return;
    for (int i=0; i<4; i++) {
        if (gh_col[i]==pm_col && gh_row[i]==pm_row) {
            if (gh_mode[i]==GM_FRIGHTENED) {
                /* Eat ghost */
                int pts = 200 << ghost_chain; /* 200,400,800,1600 */
                score += pts; ghost_chain++;
                gh_mode[i] = GM_EATEN;
                /* Score popup */
                popup_col=gh_col[i]; popup_row=gh_row[i];
                popup_end=lv_tick_get()+1000;
                lv_label_set_text_fmt(score_popup,"%d",pts);
                lv_obj_set_pos(score_popup, cx(popup_col), cy(popup_row));
                lv_obj_clear_flag(score_popup, LV_OBJ_FLAG_HIDDEN);
            } else if (gh_mode[i]!=GM_EATEN && gh_mode[i]!=GM_HOUSE) {
                /* Pac-Man dies */
                pm_dying=true; pm_death_frame=0;
                lives--;
                lv_label_set_text_fmt(lives_lbl,"%d",lives);
                return;
            }
        }
    }
    /* Fruit collision */
    if (fruit_active && pm_col==fruit_col && pm_row==fruit_row) {
        fruit_active=false; fruit_eaten=true;
        score += fruit_scores[fruit_type];
        int fpts = fruit_scores[fruit_type];
        popup_col=fruit_col; popup_row=fruit_row+1;
        popup_end=lv_tick_get()+1000;
        lv_label_set_text_fmt(score_popup,"%d",fpts);
        lv_obj_set_pos(score_popup, cx(popup_col), cy(popup_row));
        lv_obj_clear_flag(score_popup, LV_OBJ_FLAG_HIDDEN);
        draw_fruit();
    }
}

static void update_modes(void) {
    /* Death animation */
    if (pm_dying) {
        pm_death_frame++;
        if (pm_death_frame>=15) {
            pm_dying=false;
            if (lives<=0) { game_over=true; return; }
            /* Reset positions */
            pm_col=10; pm_row=17; pm_dir=DIR_NONE; pm_next_dir=DIR_LEFT;
            pm_mouth=0;
            lv_obj_set_pos(pacman_obj, cx(10), cy(17));
            draw_pacman_sprite(pacman_canvas, DIR_RIGHT, 0);
            /* Reset ghosts — valid house positions */
            uint8_t gx[4]={10,10,11,12}, gy[4]={9,10,10,10};
            for (int i=0; i<4; i++) {
                gh_col[i]=gx[i]; gh_row[i]=gy[i];
                gh_dir[i]=DIR_UP;
                gh_mode[i]=(i==BLINKY)?((global_mode==GM_SCATTER)?GM_SCATTER:GM_CHASE):GM_HOUSE;
                gh_dots_eaten[i]=0;
                lv_obj_set_pos(ghost_obj[i], cx(gx[i]), cy(gy[i]));
                draw_ghost_canvas(i);
            }
            ghost_chain=0; fright_duration=0;
            mode_phase=0; mode_secs_remaining=scatter_times_l1[0];
            mode_tick_start=tick_sec();
            global_mode=GM_SCATTER;
            return;
        }
        /* Shrink Pac-Man during death */
        if (pm_death_frame%3==0) pm_mouth=(pm_mouth+1)%5;
        draw_pacman_canvas();
        return;
    }

    /* Level clear */
    if (level_clear) {
        /* Flash maze, then advance */
        static uint8_t lc_frames=0;
        lc_frames++;
        if (lc_frames>=40) {
            lc_frames=0; level_clear=false;
            if (level<256) level++;
            restart_level();
        }
        return;
    }

    /* Game over */
    if (game_over) return;

    /* Fruit timer */
    if (fruit_active && !fruit_eaten) {
        if (lv_tick_get()-fruit_spawn_tick > 9500) {
            fruit_active=false;
            draw_fruit();
        }
    }

    /* Score popup timer */
    if (!lv_obj_has_flag(score_popup, LV_OBJ_FLAG_HIDDEN) && lv_tick_get()>=popup_end) {
        lv_obj_add_flag(score_popup, LV_OBJ_FLAG_HIDDEN);
    }

    /* Frightened timer */
    if (fright_duration>0 && lv_tick_get()-fright_start >= (uint32_t)fright_duration*1000) {
        fright_duration=0;
        for (int i=0; i<4; i++)
            if (gh_mode[i]==GM_FRIGHTENED)
                gh_mode[i]=(global_mode==GM_SCATTER)?GM_SCATTER:GM_CHASE;
        ghost_chain=0;
    }
    /* Frightened flash toggle */
    gh_flash = ((lv_tick_get()/200)&1);

    /* Mode timer for scatter/chase cycling */
    if (global_mode==GM_SCATTER || global_mode==GM_CHASE) {
        uint32_t elapsed = tick_sec()-mode_tick_start;
        if (elapsed >= mode_secs_remaining) {
            /* Switch mode */
            if (global_mode==GM_SCATTER) {
                global_mode=GM_CHASE;
                mode_phase++;
                const uint16_t *ct = (level<=1)?chase_times_l1:(level<=4)?chase_times_l24:chase_times_l5;
                mode_secs_remaining = ct[(mode_phase-1<4)?(mode_phase-1):3];
            } else {
                global_mode=GM_SCATTER;
                const uint16_t *st = (level<=1)?scatter_times_l1:(level<=4)?scatter_times_l24:scatter_times_l5;
                mode_secs_remaining = st[(mode_phase<4)?mode_phase:3];
            }
            mode_tick_start=tick_sec();
            /* Update ghost modes (only CHASE/SCATTER) */
            for (int i=0; i<4; i++)
                if (gh_mode[i]==GM_CHASE || gh_mode[i]==GM_SCATTER)
                    gh_mode[i]=global_mode;
        }
    }

    /* Ghost house exit logic */
    for (int i=1; i<4; i++) { /* Blinky starts outside */
        if (gh_mode[i]==GM_HOUSE && dots_eaten>=dot_limits[i]) {
            gh_mode[i]=GM_LEAVING;
        }
    }
}

/* ── LVGL drawing callbacks ───────────────────────────────────────── */

static void refresh_cell(int r, int c) {
    if (maze_obj[r][c]) { lv_obj_del(maze_obj[r][c]); maze_obj[r][c]=NULL; }
}

static void draw_fruit(void) {
    if (!fruit_obj) return;
    if (fruit_active && !fruit_eaten) {
        lv_obj_clear_flag(fruit_obj, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(fruit_label, fruit_names[fruit_type]);
        lv_obj_set_pos(fruit_obj, cx(fruit_col)+1, cy(fruit_row)+1);
    } else {
        lv_obj_add_flag(fruit_obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void draw_energizer_flash(lv_timer_t *t) {
    static bool flash_on = true;
    flash_on = !flash_on;
    for (int r=0; r<ROWS; r++) for (int c=0; c<COLS; c++) {
        if (maze[r][c]==3 && maze_obj[r][c]) {
            if (flash_on) {
                lv_obj_set_style_bg_color(maze_obj[r][c], color_dot, 0);
            } else {
                lv_obj_set_style_bg_color(maze_obj[r][c], color_black, 0);
            }
        }
    }
}

/* ── Game loop ────────────────────────────────────────────────────── */

static void game_tick(lv_timer_t *) {
    if (game_over) return;
    if (pm_dying || level_clear) {
        update_modes();
        return;
    }
    tick_count++;

    move_pacman();
    for (int i=0; i<4; i++) move_ghost(i);
    check_collisions();
    update_modes();

    /* Mouth animation */
    pm_mouth = (tick_count/2)%4;
    draw_pacman_canvas();

    /* Redraw ghosts (for fright flash) */
    if (fright_duration>0) for (int i=0; i<4; i++) draw_ghost_canvas(i);

    /* Update score/lives */
    lv_label_set_text_fmt(score_lbl,"%d",score);
    lv_label_set_text_fmt(lives_lbl,"%d",lives);
}

/* ── Level/game init ──────────────────────────────────────────────── */

static void restart_level(void) {
    /* Reset maze */
    dots_eaten=0; total_dots=0;
    for (int r=0; r<ROWS; r++) for (int c=0; c<COLS; c++) {
        maze[r][c]=maze_base[r][c];
        if (maze[r][c]==2||maze[r][c]==3) total_dots++;
    }
    /* Rebuild dots */
    for (int r=0; r<ROWS; r++) for (int c=0; c<COLS; c++) {
        if (maze_base[r][c]==2||maze_base[r][c]==3) {
            if (maze_obj[r][c]) lv_obj_del(maze_obj[r][c]);
            lv_obj_t *o = lv_obj_create(g_scr);
            lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
            lv_obj_set_style_border_width(o, 0, 0);
            if (maze_base[r][c]==2) {
                lv_obj_set_size(o, 3, 3);
                lv_obj_set_pos(o, cx(c)+4, cy(r)+4);
                lv_obj_set_style_bg_color(o, color_dot, 0);
                lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
            } else {
                lv_obj_set_size(o, 7, 7);
                lv_obj_set_pos(o, cx(c)+2, cy(r)+2);
                lv_obj_set_style_bg_color(o, color_dot, 0);
                lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
            }
            maze_obj[r][c]=o;
        }
    }
    /* Pac-Man */
    pm_col=10; pm_row=17; pm_dir=DIR_NONE; pm_next_dir=DIR_LEFT;
    pm_skip_ctr=0; pm_mouth=0; pm_dying=false;
    lv_obj_set_pos(pacman_obj, cx(10), cy(17));
    draw_pacman_sprite(pacman_canvas, DIR_RIGHT, 0);
    /* Ghosts — valid positions inside house (cols 10-12, rows 9-11) */
    uint8_t gx[4]={10,10,11,12}, gy[4]={9,10,10,10};
    for (int i=0; i<4; i++) {
        gh_col[i]=gx[i]; gh_row[i]=gy[i]; gh_dir[i]=DIR_UP;
        gh_mode[i]=(i==BLINKY)?((global_mode==GM_SCATTER)?GM_SCATTER:GM_CHASE):GM_HOUSE;
        gh_dots_eaten[i]=0; gh_skip_ctr[i]=0;
        lv_obj_set_pos(ghost_obj[i], cx(gx[i]), cy(gy[i]));
        draw_ghost_canvas(i);
    }
    /* Fruit */
    fruit_active=false; fruit_eaten=false; fruit_phase=0;
    lv_obj_add_flag(fruit_obj, LV_OBJ_FLAG_HIDDEN);
    /* Modes */
    ghost_chain=0; fright_duration=0;
    global_mode=GM_SCATTER; mode_phase=0;
    const uint16_t *st = (level<=1)?scatter_times_l1:(level<=4)?scatter_times_l24:scatter_times_l5;
    mode_secs_remaining=st[0]; mode_tick_start=tick_sec();
    /* HUD */
    lv_label_set_text_fmt(level_lbl,"Lv%d",level);
}

static void restart_game(void) {
    score=0; lives=3; level=1; game_over=false; level_clear=false;
    tick_count=0; bonus_given=false;
    lv_label_set_text_fmt(score_lbl,"%d",score);
    lv_label_set_text_fmt(lives_lbl,"%d",lives);
    restart_level();
}

/* ── Public: maze loader ──────────────────────────────────────────── */

void pacman_set_maze(const uint8_t m[ROWS][COLS]) {
    memcpy(maze_base, m, sizeof(maze_base));
    maze_loaded=true;
}

/* ── Create game screen ───────────────────────────────────────────── */

lv_obj_t *pacman_game_create(void) {
    /* Ensure maze is loaded */
    if (!maze_loaded) {
        memcpy(maze_base, default_maze, sizeof(maze_base));
        maze_loaded=true;
    }

    /* Create screen */
    g_scr = lv_obj_create(NULL);
    lv_obj_set_size(g_scr, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(g_scr, color_black, 0);
    lv_obj_clear_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* HUD — single compact row: [Back] SCORE:0   L:3  Lv1 */
    lv_obj_t *bb = lv_btn_create(g_scr);
    lv_obj_set_size(bb, 28, 18); lv_obj_set_pos(bb, 1, 1);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT); lv_obj_center(bl);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(bb, [](lv_event_t*) {
        if (game_timer) { lv_timer_del(game_timer); game_timer=NULL; }
        if (energizer_timer) { lv_timer_del(energizer_timer); energizer_timer=NULL; }
        lv_obj_del(g_scr);
    }, LV_EVENT_CLICKED, NULL);

    score_lbl = lv_label_create(g_scr);
    lv_obj_set_style_text_color(score_lbl, color_white, 0);
    lv_obj_set_style_text_font(score_lbl, &lv_font_montserrat_12, 0);
    lv_label_set_text(score_lbl, "0");
    lv_obj_set_pos(score_lbl, 36, 2);

    lives_lbl = lv_label_create(g_scr);
    lv_obj_set_style_text_color(lives_lbl, color_white, 0);
    lv_obj_set_style_text_font(lives_lbl, &lv_font_montserrat_12, 0);
    lv_label_set_text(lives_lbl, "3");
    lv_obj_align(lives_lbl, LV_ALIGN_TOP_MID, 0, 2);

    level_lbl = lv_label_create(g_scr);
    lv_obj_set_style_text_color(level_lbl, color_white, 0);
    lv_obj_set_style_text_font(level_lbl, &lv_font_montserrat_12, 0);
    lv_label_set_text(level_lbl, "Lv1");
    lv_obj_align(level_lbl, LV_ALIGN_TOP_RIGHT, -2, 2);

    /* Score popup */
    score_popup = lv_label_create(g_scr);
    lv_obj_set_style_text_color(score_popup, lv_color_hex(0x00FFFF), 0);
    lv_obj_add_flag(score_popup, LV_OBJ_FLAG_HIDDEN);

    /* Draw maze */
    draw_maze();

    /* Create fruit object */
    fruit_obj = lv_obj_create(g_scr);
    lv_obj_set_size(fruit_obj, CELL, CELL);
    lv_obj_set_style_bg_color(fruit_obj, color_black, 0);
    lv_obj_set_style_border_width(fruit_obj, 0, 0);
    lv_obj_add_flag(fruit_obj, LV_OBJ_FLAG_HIDDEN);
    fruit_label = lv_label_create(fruit_obj);
    lv_label_set_text(fruit_label, ""); lv_obj_center(fruit_label);
    lv_obj_set_style_text_color(fruit_label, color_white, 0);

    /* Create entity objects */
    create_pacman_obj();
    for (int i=0; i<4; i++) create_ghost_obj(i);

    /* Game Over label (hidden until needed) */
    go_label = lv_label_create(g_scr);
    lv_obj_set_style_text_color(go_label, lv_color_hex(0xFF0000), 0);
    lv_label_set_text(go_label, "GAME OVER");
    lv_obj_align(go_label, LV_ALIGN_CENTER, 0, 60);
    lv_obj_add_flag(go_label, LV_OBJ_FLAG_HIDDEN);

    /* Init game state */
    restart_game();

    /* D-pad controls — below maze at y=260 */
    int dpy = OY + MAZE_H + 2;
    int dpc = SCR_W / 2;
    struct { int x,y; const char *t; Dir d; } btns[] = {
        {dpc-28, dpy,      LV_SYMBOL_UP,    DIR_UP},
        {dpc-28, dpy+64,    LV_SYMBOL_DOWN,  DIR_DOWN},
        {dpc-92, dpy+32,    LV_SYMBOL_LEFT,  DIR_LEFT},
        {dpc+36, dpy+32,    LV_SYMBOL_RIGHT, DIR_RIGHT},
    };
    for (auto &b: btns) {
        lv_obj_t *btn = lv_btn_create(g_scr);
        lv_obj_set_size(btn, 56, 56); lv_obj_set_pos(btn, b.x, b.y);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, b.t); lv_obj_center(l);
        lv_obj_add_event_cb(btn, [](lv_event_t *e) {
            Dir d = (Dir)(intptr_t)lv_event_get_user_data(e);
            pm_next_dir = d;
            if (pm_dir!=d && can_move(pm_col,pm_row,d)) pm_dir=d;
        }, LV_EVENT_PRESSING, (void*)(intptr_t)b.d);
    }

    /* Game timer ~100ms (10 ticks/sec) with game-over detection */
    game_timer = lv_timer_create([](lv_timer_t *t) {
        game_tick(t);
        if (game_over) {
            lv_obj_clear_flag(go_label, LV_OBJ_FLAG_HIDDEN);
            lv_timer_del(t); game_timer=NULL;
            /* Tap anywhere to restart */
            lv_obj_add_event_cb(g_scr, [](lv_event_t*) {
                lv_obj_add_flag(go_label, LV_OBJ_FLAG_HIDDEN);
                restart_game();
                game_timer = lv_timer_create([](lv_timer_t *t2) {
                    game_tick(t2);
                    if (game_over) {
                        lv_obj_clear_flag(go_label, LV_OBJ_FLAG_HIDDEN);
                        lv_timer_del(t2); game_timer=NULL;
                    }
                }, 100, NULL);
            }, LV_EVENT_CLICKED, NULL);
        }
    }, 100, NULL);

    /* Energizer flash timer */
    energizer_timer = lv_timer_create(draw_energizer_flash, 300, NULL);

    lv_scr_load(g_scr);
    return g_scr;
}
