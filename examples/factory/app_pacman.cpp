/**
 * @file    app_pacman.cpp
 * @brief   Pacman game for T-Display-S3-Pro (222x480 LVGL)
 *
 * All game objects are children of the game screen — deleting the screen
 * cleans up everything. D-pad uses LV_EVENT_PRESSING for auto-repeat.
 */
#include "app_pacman.h"
#include "lvgl.h"
#include <stdlib.h>

#define SCR_W  222
#define SCR_H  480
#define CELL   16
#define COLS   13
#define ROWS   20
#define OX     ((SCR_W - COLS * CELL) / 2)
#define OY     30

static const uint8_t maze_init[ROWS][COLS] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1}, {1,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,1,1,2,1,1,2,1}, {1,3,1,1,2,1,1,1,2,1,1,3,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,1}, {1,2,1,1,2,1,2,1,2,1,1,2,1},
    {1,2,2,2,2,1,2,1,2,2,2,2,1}, {1,1,1,1,2,1,0,1,2,1,1,1,1},
    {0,0,0,0,2,1,0,1,2,0,0,0,0}, {0,0,0,0,2,0,4,0,2,0,0,0,0},
    {0,0,0,0,2,1,4,1,2,0,0,0,0}, {0,0,0,0,2,1,4,1,2,0,0,0,0},
    {1,1,1,1,2,1,0,1,2,1,1,1,1}, {1,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,1,1,2,1,1,2,1}, {1,3,2,2,2,2,2,2,2,2,2,3,1},
    {1,1,1,2,1,2,1,2,1,2,1,1,1}, {1,2,2,2,1,2,1,2,1,2,2,2,1},
    {1,2,1,1,1,1,2,1,1,1,1,2,1}, {1,2,2,2,2,2,2,2,2,2,2,2,1},
};

static uint8_t    maze[ROWS][COLS];
static int        pellets_left, score, lives;
static lv_obj_t  *game_scr = NULL;
static lv_obj_t  *maze_obj[ROWS][COLS];
static lv_obj_t  *pacman_obj, *ghost_obj[4];
static lv_obj_t  *score_lbl, *lives_lbl;
static lv_timer_t *gtimer, *htimer, *ptimer;
static bool       power_mode;
static uint32_t   power_end;
static int        pacman_dir = 1, pacman_next = 1; /* 0=U 1=D 2=L 3=R */
static int        ghost_dir[4] = {0,1,2,3};
static int        pacman_cx, pacman_cy, ghost_cx[4], ghost_cy[4];

static inline int cx(int c) { return OX + c*CELL; }
static inline int cy(int r) { return OY + r*CELL; }
static bool can_move(int x, int y, int d);
static void restart_game(void);
static void refresh_cell(int r, int c);

lv_obj_t *pacman_game_create(void)
{
    game_scr = lv_obj_create(NULL);
    lv_obj_set_size(game_scr, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(game_scr, lv_color_black(), 0);
    lv_obj_clear_flag(game_scr, LV_OBJ_FLAG_SCROLLABLE);

    score_lbl = lv_label_create(game_scr);
    lv_obj_set_style_text_color(score_lbl, lv_color_white(), 0);
    lv_obj_align(score_lbl, LV_ALIGN_TOP_LEFT, 4, 4);
    lives_lbl = lv_label_create(game_scr);
    lv_obj_set_style_text_color(lives_lbl, lv_color_white(), 0);
    lv_obj_align(lives_lbl, LV_ALIGN_TOP_RIGHT, -4, 4);

    lv_obj_t *bb = lv_btn_create(game_scr);
    lv_obj_set_size(bb, 50, 24); lv_obj_align(bb, LV_ALIGN_TOP_LEFT, 60, 2);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back"); lv_obj_center(bl);
    lv_obj_add_event_cb(bb, [](lv_event_t*) {
        if(gtimer){lv_timer_del(gtimer);gtimer=NULL;}
        if(htimer){lv_timer_del(htimer);htimer=NULL;}
        if(ptimer){lv_timer_del(ptimer);ptimer=NULL;}
        lv_obj_del(game_scr);
    }, LV_EVENT_CLICKED, NULL);

    /* draw maze */
    for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++){
        uint8_t cell = maze_init[r][c];
        if(cell==0||cell==4){maze_obj[r][c]=NULL;continue;}
        lv_obj_t *o = lv_obj_create(game_scr);
        lv_obj_set_pos(o, cx(c), cy(r));
        lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
        if(cell==1){
            lv_obj_set_size(o, CELL-1, CELL-1);
            lv_obj_set_style_bg_color(o, lv_color_hex(0x2121DE), 0);
            lv_obj_set_style_border_width(o, 0, 0);
        }else if(cell==2){
            lv_obj_set_size(o, 4, 4); lv_obj_set_pos(o, cx(c)+6, cy(r)+6);
            lv_obj_set_style_bg_color(o, lv_color_hex(0xFFB8AE), 0);
            lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        }else if(cell==3){
            lv_obj_set_size(o, 10,10); lv_obj_set_pos(o, cx(c)+3, cy(r)+3);
            lv_obj_set_style_bg_color(o, lv_color_hex(0xFFB8AE), 0);
            lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        }
        maze_obj[r][c] = o;
    }
    restart_game();

    /* D-pad */
    int base = OY + ROWS*CELL + 16;
    struct { int x,y; const char *t; int d; } btns[] = {
        {SCR_W/2-28, base-60, LV_SYMBOL_UP,    0},
        {SCR_W/2-28, base+4,  LV_SYMBOL_DOWN,  1},
        {SCR_W/2-84, base-28, LV_SYMBOL_LEFT,  2},
        {SCR_W/2+28, base-28, LV_SYMBOL_RIGHT, 3},
    };
    for(auto &b: btns){
        lv_obj_t *btn = lv_btn_create(game_scr);
        lv_obj_set_size(btn, 56, 56); lv_obj_set_pos(btn, b.x, b.y);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, b.t); lv_obj_center(l);
        lv_obj_add_event_cb(btn, [](lv_event_t *e) {
            int d = (int)(intptr_t)lv_event_get_user_data(e);
            pacman_next = d;
            if(pacman_dir!=d && can_move(pacman_cx,pacman_cy,d)) pacman_dir=d;
        }, LV_EVENT_PRESSING, (void*)(intptr_t)b.d);
    }
    lv_scr_load(game_scr);
    return game_scr;
}

static void restart_game(void)
{
    pellets_left=0;
    for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++){
        maze[r][c]=maze_init[r][c];
        if(maze[r][c]==2||maze[r][c]==3) pellets_left++;
    }
    score=0; lives=3; power_mode=false;
    lv_label_set_text_fmt(score_lbl,"Score: %d",score);
    lv_label_set_text_fmt(lives_lbl,"Lives: %d",lives);

    pacman_cx=1; pacman_cy=ROWS-2; pacman_dir=3; pacman_next=3;
    if(pacman_obj) lv_obj_del(pacman_obj);
    pacman_obj = lv_obj_create(game_scr);
    lv_obj_set_size(pacman_obj, CELL-2, CELL-2);
    lv_obj_set_style_bg_color(pacman_obj, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_radius(pacman_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(pacman_obj, 0, 0);
    lv_obj_set_pos(pacman_obj, cx(1)+1, cy(ROWS-2)+1);

    static const lv_color_t gc[4] = {
        LV_COLOR_MAKE(0xFF,0x00,0x00), LV_COLOR_MAKE(0xFF,0xB8,0xFF),
        LV_COLOR_MAKE(0x00,0xFF,0xFF), LV_COLOR_MAKE(0xFF,0xB8,0x52),
    };
    int sx[4]={5,7,6,6}, sy[4]={9,9,10,11};
    for(int i=0;i<4;i++){
        ghost_cx[i]=sx[i]; ghost_cy[i]=sy[i]; ghost_dir[i]=0;
        if(ghost_obj[i]) lv_obj_del(ghost_obj[i]);
        ghost_obj[i] = lv_obj_create(game_scr);
        lv_obj_set_size(ghost_obj[i], CELL-2, CELL-2);
        lv_obj_set_style_bg_color(ghost_obj[i], gc[i], 0);
        lv_obj_set_style_radius(ghost_obj[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ghost_obj[i], 0, 0);
        lv_obj_set_pos(ghost_obj[i], cx(sx[i])+1, cy(sy[i])+1);
    }
    if(gtimer) lv_timer_del(gtimer); gtimer=lv_timer_create([](lv_timer_t*){
        if(pacman_next!=pacman_dir && can_move(pacman_cx,pacman_cy,pacman_next))
            pacman_dir=pacman_next;
        if(!can_move(pacman_cx,pacman_cy,pacman_dir)) return;
        int nx=pacman_cx, ny=pacman_cy;
        if(pacman_dir==0)ny--; if(pacman_dir==1)ny++; if(pacman_dir==2)nx--; if(pacman_dir==3)nx++;
        if(nx<0)nx=COLS-1; if(nx>=COLS)nx=0;
        if(ny<0||ny>=ROWS) return;
        pacman_cx=nx; pacman_cy=ny;
        lv_obj_set_pos(pacman_obj, cx(nx)+1, cy(ny)+1);
        if(maze[ny][nx]==2){maze[ny][nx]=0;score+=10;pellets_left--;refresh_cell(ny,nx);}
        else if(maze[ny][nx]==3){maze[ny][nx]=0;score+=50;pellets_left--;refresh_cell(ny,nx);
            power_mode=true; power_end=lv_tick_get()+7000;
            if(ptimer)lv_timer_del(ptimer);ptimer=lv_timer_create([](lv_timer_t*t){
                if(lv_tick_get()>=power_end){power_mode=false;lv_timer_del(t);ptimer=NULL;}
            },100,NULL);
        }
        lv_label_set_text_fmt(score_lbl,"Score: %d",score);
        if(pellets_left<=0) restart_game();
        for(int i=0;i<4;i++) if(ghost_cx[i]==pacman_cx && ghost_cy[i]==pacman_cy){
            if(power_mode){score+=200;lv_label_set_text_fmt(score_lbl,"Score: %d",score);
                ghost_cx[i]=6;ghost_cy[i]=10;lv_obj_set_pos(ghost_obj[i],cx(6)+1,cy(10)+1);}
            else { lives--; lv_label_set_text_fmt(lives_lbl,"Lives: %d",lives);
                if(lives<=0){ if(gtimer)lv_timer_del(gtimer); if(htimer)lv_timer_del(htimer);
                    lv_obj_t*m=lv_msgbox_create(NULL,"Game Over","Tap to restart",NULL,true);
                    lv_obj_center(m); lv_obj_add_event_cb(m,[](lv_event_t*e){
                        lv_obj_del(lv_event_get_current_target(e));restart_game();
                    },LV_EVENT_CLICKED,NULL); return; }
                lv_obj_del(pacman_obj); pacman_obj=NULL;
                for(int j=0;j<4;j++){lv_obj_del(ghost_obj[j]);ghost_obj[j]=NULL;}
                restart_game(); return;
            }
        }
    },200,NULL);
    if(htimer) lv_timer_del(htimer); htimer=lv_timer_create([](lv_timer_t*){
        for(int i=0;i<4;i++){
            int best_dir=ghost_dir[i], best_dist=999;
            if((lv_tick_get()/1000)%7==0){
                int ds[4]={0,1,2,3};
                for(int a=0;a<8;a++){int d=ds[rand()%4];if(can_move(ghost_cx[i],ghost_cy[i],d)){best_dir=d;break;}}
            }else{
                int ds[4]={0,1,2,3};
                for(int j=0;j<4;j++){int d=ds[j];if(!can_move(ghost_cx[i],ghost_cy[i],d))continue;
                    int nx=ghost_cx[i],ny=ghost_cy[i];
                    if(d==0)ny--;if(d==1)ny++;if(d==2)nx--;if(d==3)nx++;
                    if(nx<0)nx=COLS-1;if(nx>=COLS)nx=0;
                    int dist=abs(nx-pacman_cx)+abs(ny-pacman_cy);
                    if(dist<best_dist){best_dist=dist;best_dir=d;}
                }
            }
            if(!can_move(ghost_cx[i],ghost_cy[i],best_dir)){
                int ds[4]={0,1,2,3};bool ok=false;
                for(int j=0;j<4;j++)if(can_move(ghost_cx[i],ghost_cy[i],ds[j])){best_dir=ds[j];ok=true;break;}
                if(!ok) continue;
            }
            int nx=ghost_cx[i],ny=ghost_cy[i];
            if(best_dir==0)ny--;if(best_dir==1)ny++;if(best_dir==2)nx--;if(best_dir==3)nx++;
            if(nx<0)nx=COLS-1;if(nx>=COLS)nx=0;
            ghost_cx[i]=nx;ghost_cy[i]=ny;ghost_dir[i]=best_dir;
            lv_obj_set_pos(ghost_obj[i],cx(nx)+1,cy(ny)+1);
        }
    },300,NULL);
}

static bool can_move(int x, int y, int d){
    int nx=x,ny=y;
    if(d==0)ny--;if(d==1)ny++;if(d==2)nx--;if(d==3)nx++;
    if(nx<0)nx=COLS-1;if(nx>=COLS)nx=0;
    if(ny<0||ny>=ROWS) return false;
    return maze[ny][nx]!=1;
}

static void refresh_cell(int r, int c){
    if(maze_obj[r][c]){lv_obj_del(maze_obj[r][c]);maze_obj[r][c]=NULL;}
}
