/**
 * @file    app_tetris.cpp
 * @brief   Tetris game for T-Display-S3-Pro (222x480 LVGL)
 *
 * Classic falling-blocks puzzle. 10×20 board, 7 tetrominoes.
 * Touch controls: left/right/down/rotate buttons at bottom.
 *
 * Board: 10 cols × 20 rows × 16px cells = 160×320, centered.
 * Layout: score (28px), board (320px), next-piece + D-pad (132px).
 */

#include "app_tetris.h"
#include "lvgl.h"
#include <stdlib.h>
#include <string.h>

/* ── geometry ────────────────────────────────────────────────────────── */
#define SCR_W   222
#define SCR_H   480
#define CELL_SZ 16
#define COLS    10
#define ROWS    20
#define BX      ((SCR_W - COLS * CELL_SZ) / 2)   /* board left */
#define BY      28                                /* board top  */

/* ── tetromino definitions (4 rotation states each) ─────────────────── */
static const int PIECES[7][4][4][2] = {
    /* I */ {{{0,0},{1,0},{2,0},{3,0}}, {{0,0},{0,1},{0,2},{0,3}},
             {{0,0},{1,0},{2,0},{3,0}}, {{0,0},{0,1},{0,2},{0,3}}},
    /* O */ {{{0,0},{1,0},{0,1},{1,1}}, {{0,0},{1,0},{0,1},{1,1}},
             {{0,0},{1,0},{0,1},{1,1}}, {{0,0},{1,0},{0,1},{1,1}}},
    /* T */ {{{0,0},{1,0},{2,0},{1,1}}, {{0,0},{0,1},{0,2},{1,1}},
             {{1,0},{0,1},{1,1},{2,1}}, {{0,0},{0,1},{0,2},{-1,1}}},
    /* S */ {{{1,0},{2,0},{0,1},{1,1}}, {{0,0},{0,1},{1,1},{1,2}},
             {{1,0},{2,0},{0,1},{1,1}}, {{0,0},{0,1},{1,1},{1,2}}},
    /* Z */ {{{0,0},{1,0},{1,1},{2,1}}, {{1,0},{0,1},{1,1},{0,2}},
             {{0,0},{1,0},{1,1},{2,1}}, {{1,0},{0,1},{1,1},{0,2}}},
    /* J */ {{{0,0},{0,1},{1,1},{2,1}}, {{0,0},{1,0},{0,1},{0,2}},
             {{0,0},{1,0},{2,0},{2,1}}, {{0,0},{0,1},{0,2},{-1,2}}},
    /* L */ {{{2,0},{0,1},{1,1},{2,1}}, {{0,0},{0,1},{0,2},{1,2}},
             {{0,0},{1,0},{2,0},{0,1}}, {{0,0},{1,0},{1,1},{1,2}}},
};
static const lv_color_t PIECE_COLORS[7] = {
    LV_COLOR_MAKE(0x00,0xFF,0xFF),  /* I — cyan    */
    LV_COLOR_MAKE(0xFF,0xFF,0x00),  /* O — yellow  */
    LV_COLOR_MAKE(0xAA,0x00,0xFF),  /* T — purple  */
    LV_COLOR_MAKE(0x00,0xFF,0x00),  /* S — green   */
    LV_COLOR_MAKE(0xFF,0x00,0x00),  /* Z — red     */
    LV_COLOR_MAKE(0x00,0x00,0xFF),  /* J — blue    */
    LV_COLOR_MAKE(0xFF,0x88,0x00),  /* L — orange  */
};

/* ── game state ──────────────────────────────────────────────────────── */
static uint8_t board[ROWS][COLS];           /* 0=empty, 1-7=piece type   */
static lv_color_t board_colors[ROWS][COLS]; /* stored colour per cell    */
static int current_piece, current_rot;
static int piece_x, piece_y;                /* piece origin in board coords */
static int next_piece;
static int score, level, lines;
static bool game_running;

/* ── LVGL objects ────────────────────────────────────────────────────── */
static lv_obj_t *cell_objs[ROWS][COLS];
static lv_obj_t *score_label, *lines_label, *level_label;
static lv_obj_t *next_preview[4][4];        /* 4×4 preview grid          */
static lv_timer_t *fall_timer = NULL;

/* ── forward ─────────────────────────────────────────────────────────── */
static void draw_board(lv_obj_t *parent);
static void draw_cell(int r, int c, lv_color_t color);
static void clear_cell(int r, int c);
static void spawn_piece(void);
static bool check_collision(int px, int py, int piece, int rot);
static void lock_piece(void);
static void clear_lines(void);
static void move_down(void);
static void move_left(void);
static void move_right(void);
static void rotate_piece(void);
static void game_over(void);
static void restart_game(void);
static void fall_cb(lv_timer_t *t);

/* ═══════════════════════════════════════════════════════════════════════ */
lv_obj_t *tetris_game_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* score bar */
    score_label = lv_label_create(scr);
    lv_obj_set_style_text_color(score_label, lv_color_white(), 0);
    lv_obj_align(score_label, LV_ALIGN_TOP_LEFT, 4, 4);

    lines_label = lv_label_create(scr);
    lv_obj_set_style_text_color(lines_label, lv_color_white(), 0);
    lv_obj_align(lines_label, LV_ALIGN_TOP_LEFT, 4, 20);

    level_label = lv_label_create(scr);
    lv_obj_set_style_text_color(level_label, lv_color_white(), 0);
    lv_obj_align(level_label, LV_ALIGN_TOP_RIGHT, -4, 4);

    /* back button */
    lv_obj_t *btn_back = lv_btn_create(scr);
    lv_obj_set_size(btn_back, 50, 24);
    lv_obj_align(btn_back, LV_ALIGN_TOP_RIGHT, -4, 20);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Back");
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, [](lv_event_t *e) {
        lv_obj_t *scr = lv_obj_get_parent(lv_event_get_target(e));
        if (fall_timer) { lv_timer_del(fall_timer); fall_timer = NULL; }
        lv_obj_del(scr);
    }, LV_EVENT_CLICKED, NULL);

    /* next-piece preview area */
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            next_preview[r][c] = NULL;

    draw_board(scr);
    restart_game();

    /* D-pad at bottom */
    int base_y = BY + ROWS * CELL_SZ + 8;
    auto btn = [&](int x, int y, int w, int h, const char *t, void (*cb)(void)) {
        lv_obj_t *b = lv_btn_create(scr);
        lv_obj_set_size(b, w, h); lv_obj_set_pos(b, x, y);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, t); lv_obj_center(l);
        lv_obj_add_event_cb(b, [](lv_event_t *e) {
            void (*f)(void) = (void (*)(void))(intptr_t)lv_event_get_user_data(e);
            if (f && game_running) f();
        }, LV_EVENT_CLICKED, (void *)(intptr_t)cb);
    };
    btn(10,  base_y,      48, 40, LV_SYMBOL_LEFT,        move_left);
    btn(62,  base_y,      48, 40, LV_SYMBOL_DOWN,        move_down);
    btn(114, base_y,      48, 40, LV_SYMBOL_RIGHT,       move_right);
    btn(62,  base_y - 44, 48, 40, LV_SYMBOL_REFRESH,     rotate_piece);

    lv_scr_load(scr);
    return scr;
}

/* ═══════════════════════════════════════════════════════════════════════ */

static void draw_board(lv_obj_t *parent)
{
    /* draw grid lines using thin rectangles */
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            lv_obj_t *obj = lv_obj_create(parent);
            lv_obj_set_size(obj, CELL_SZ - 1, CELL_SZ - 1);
            lv_obj_set_pos(obj, BX + c * CELL_SZ, BY + r * CELL_SZ);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x202020), 0);
            lv_obj_set_style_border_width(obj, 1, 0);
            lv_obj_set_style_border_color(obj, lv_color_hex(0x404040), 0);
            lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
            cell_objs[r][c] = obj;
        }
    }

    /* next-piece preview border */
    lv_obj_t *preview_border = lv_obj_create(parent);
    lv_obj_set_size(preview_border, 4 * CELL_SZ + 2, 4 * CELL_SZ + 2);
    lv_obj_set_pos(preview_border, 160, BY);
    lv_obj_set_style_border_color(preview_border, lv_color_hex(0x606060), 0);
    lv_obj_set_style_border_width(preview_border, 2, 0);
    lv_obj_set_style_bg_opa(preview_border, LV_OPA_TRANSP, 0);

    lv_obj_t *plabel = lv_label_create(parent);
    lv_label_set_text(plabel, "Next");
    lv_obj_set_style_text_color(plabel, lv_color_hex(0x808080), 0);
    lv_obj_set_pos(plabel, 160, BY - 14);
}

static void draw_cell(int r, int c, lv_color_t color)
{
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return;
    lv_obj_set_style_bg_color(cell_objs[r][c], color, 0);
    lv_obj_set_style_border_width(cell_objs[r][c], 0, 0);
}

static void clear_cell(int r, int c)
{
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return;
    board[r][c] = 0;
    board_colors[r][c] = lv_color_black();
    lv_obj_set_style_bg_color(cell_objs[r][c], lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_width(cell_objs[r][c], 1, 0);
    lv_obj_set_style_border_color(cell_objs[r][c], lv_color_hex(0x404040), 0);
}

static void spawn_piece(void)
{
    current_piece = next_piece;
    current_rot   = 0;
    piece_x = COLS / 2 - 2;
    piece_y = 0;
    next_piece = rand() % 7;

    if (check_collision(piece_x, piece_y, current_piece, current_rot)) {
        game_over();
        return;
    }

    /* draw preview */
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            if (next_preview[r][c]) { lv_obj_del(next_preview[r][c]); next_preview[r][c] = NULL; }
        }

    for (int i = 0; i < 4; i++) {
        int cx = PIECES[next_piece][0][i][0];
        int cy = PIECES[next_piece][0][i][1];
        lv_obj_t *obj = lv_obj_create(lv_scr_act());
        lv_obj_set_size(obj, CELL_SZ - 2, CELL_SZ - 2);
        lv_obj_set_pos(obj, 161 + cx * CELL_SZ, BY + 1 + cy * CELL_SZ);
        lv_obj_set_style_bg_color(obj, PIECE_COLORS[next_piece], 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
        next_preview[cy][cx] = obj;
    }

    /* draw current piece */
    for (int i = 0; i < 4; i++) {
        int cx = PIECES[current_piece][current_rot][i][0];
        int cy = PIECES[current_piece][current_rot][i][1];
        draw_cell(piece_y + cy, piece_x + cx, PIECE_COLORS[current_piece]);
    }
}

static bool check_collision(int px, int py, int piece, int rot)
{
    for (int i = 0; i < 4; i++) {
        int x = px + PIECES[piece][rot][i][0];
        int y = py + PIECES[piece][rot][i][1];
        if (x < 0 || x >= COLS || y >= ROWS) return true;
        if (y >= 0 && board[y][x]) return true;
    }
    return false;
}

static void lock_piece(void)
{
    for (int i = 0; i < 4; i++) {
        int x = piece_x + PIECES[current_piece][current_rot][i][0];
        int y = piece_y + PIECES[current_piece][current_rot][i][1];
        if (y < 0) { game_over(); return; }
        board[y][x] = current_piece + 1;
        board_colors[y][x] = PIECE_COLORS[current_piece];
    }
    clear_lines();
    spawn_piece();
}

static void clear_lines(void)
{
    int cleared = 0;
    for (int r = ROWS - 1; r >= 0; r--) {
        bool full = true;
        for (int c = 0; c < COLS; c++) { if (!board[r][c]) { full = false; break; } }
        if (full) {
            cleared++;
            /* shift everything down */
            for (int rr = r; rr > 0; rr--) {
                for (int c = 0; c < COLS; c++) {
                    board[rr][c] = board[rr-1][c];
                    board_colors[rr][c] = board_colors[rr-1][c];
                }
            }
            for (int c = 0; c < COLS; c++) { board[0][c] = 0; board_colors[0][c] = lv_color_black(); }
            r++; /* re-check this row */
        }
    }

    /* redraw board */
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (board[r][c])
                draw_cell(r, c, board_colors[r][c]);
            else
                clear_cell(r, c);

    if (cleared > 0) {
        static const int line_scores[] = {0, 100, 300, 500, 800};
        score += line_scores[cleared > 4 ? 4 : cleared] * (level + 1);
        lines += cleared;
        level = lines / 10;
        lv_label_set_text_fmt(score_label, "Score: %d", score);
        lv_label_set_text_fmt(lines_label, "Lines: %d", lines);
        lv_label_set_text_fmt(level_label, "Lvl %d", level);

        /* speed up */
        if (fall_timer) {
            int period = 500 - level * 40;
            if (period < 80) period = 80;
            lv_timer_set_period(fall_timer, period);
        }
    }
}

static void move_down(void)
{
    /* erase */
    for (int i = 0; i < 4; i++) {
        int x = piece_x + PIECES[current_piece][current_rot][i][0];
        int y = piece_y + PIECES[current_piece][current_rot][i][1];
        if (y >= 0 && y < ROWS && x >= 0 && x < COLS) clear_cell(y, x);
    }
    if (!check_collision(piece_x, piece_y + 1, current_piece, current_rot)) {
        piece_y++;
    } else {
        /* lock and redraw */
        for (int i = 0; i < 4; i++) {
            int x = piece_x + PIECES[current_piece][current_rot][i][0];
            int y = piece_y + PIECES[current_piece][current_rot][i][1];
            if (y >= 0 && y < ROWS && x >= 0 && x < COLS) draw_cell(y, x, PIECE_COLORS[current_piece]);
        }
        lock_piece();
        return;
    }
    /* redraw */
    for (int i = 0; i < 4; i++) {
        int x = piece_x + PIECES[current_piece][current_rot][i][0];
        int y = piece_y + PIECES[current_piece][current_rot][i][1];
        if (y >= 0 && y < ROWS && x >= 0 && x < COLS) draw_cell(y, x, PIECE_COLORS[current_piece]);
    }
}

static void move_left(void)
{
    for (int i = 0; i < 4; i++) {
        int x = piece_x + PIECES[current_piece][current_rot][i][0];
        int y = piece_y + PIECES[current_piece][current_rot][i][1];
        if (y >= 0 && y < ROWS && x >= 0 && x < COLS) clear_cell(y, x);
    }
    if (!check_collision(piece_x - 1, piece_y, current_piece, current_rot))
        piece_x--;
    for (int i = 0; i < 4; i++) {
        int x = piece_x + PIECES[current_piece][current_rot][i][0];
        int y = piece_y + PIECES[current_piece][current_rot][i][1];
        if (y >= 0 && y < ROWS && x >= 0 && x < COLS) draw_cell(y, x, PIECE_COLORS[current_piece]);
    }
}

static void move_right(void)
{
    for (int i = 0; i < 4; i++) {
        int x = piece_x + PIECES[current_piece][current_rot][i][0];
        int y = piece_y + PIECES[current_piece][current_rot][i][1];
        if (y >= 0 && y < ROWS && x >= 0 && x < COLS) clear_cell(y, x);
    }
    if (!check_collision(piece_x + 1, piece_y, current_piece, current_rot))
        piece_x++;
    for (int i = 0; i < 4; i++) {
        int x = piece_x + PIECES[current_piece][current_rot][i][0];
        int y = piece_y + PIECES[current_piece][current_rot][i][1];
        if (y >= 0 && y < ROWS && x >= 0 && x < COLS) draw_cell(y, x, PIECE_COLORS[current_piece]);
    }
}

static void rotate_piece(void)
{
    for (int i = 0; i < 4; i++) {
        int x = piece_x + PIECES[current_piece][current_rot][i][0];
        int y = piece_y + PIECES[current_piece][current_rot][i][1];
        if (y >= 0 && y < ROWS && x >= 0 && x < COLS) clear_cell(y, x);
    }
    int new_rot = (current_rot + 1) % 4;
    /* wall kick: try original, then left, then right */
    if (!check_collision(piece_x, piece_y, current_piece, new_rot)) {
        current_rot = new_rot;
    } else if (!check_collision(piece_x - 1, piece_y, current_piece, new_rot)) {
        piece_x--; current_rot = new_rot;
    } else if (!check_collision(piece_x + 1, piece_y, current_piece, new_rot)) {
        piece_x++; current_rot = new_rot;
    }
    for (int i = 0; i < 4; i++) {
        int x = piece_x + PIECES[current_piece][current_rot][i][0];
        int y = piece_y + PIECES[current_piece][current_rot][i][1];
        if (y >= 0 && y < ROWS && x >= 0 && x < COLS) draw_cell(y, x, PIECE_COLORS[current_piece]);
    }
}

static void game_over(void)
{
    game_running = false;
    if (fall_timer) { lv_timer_del(fall_timer); fall_timer = NULL; }
    lv_obj_t *mbox = lv_msgbox_create(NULL, "Game Over",
        "Tap anywhere to restart", NULL, true);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, [](lv_event_t *e) {
        lv_obj_del(lv_event_get_current_target(e));
        restart_game();
    }, LV_EVENT_CLICKED, NULL);
}

static void restart_game(void)
{
    memset(board, 0, sizeof(board));
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            clear_cell(r, c);
    score = 0; level = 0; lines = 0;
    lv_label_set_text(score_label, "Score: 0");
    lv_label_set_text(lines_label, "Lines: 0");
    lv_label_set_text(level_label, "Lvl 0");
    game_running = true;
    next_piece = rand() % 7;
    spawn_piece();
    if (fall_timer) lv_timer_del(fall_timer);
    fall_timer = lv_timer_create(fall_cb, 500, NULL);
}

static void fall_cb(lv_timer_t *t)
{
    (void)t;
    if (!game_running) return;
    move_down();
}
