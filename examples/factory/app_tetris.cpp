/**
 * @file    app_tetris.cpp
 * @brief   Tetris for T-Display-S3-Pro (222x480 LVGL)
 *
 * All objects children of game screen. D-pad uses LV_EVENT_PRESSING.
 */
#include "app_tetris.h"
#include "lvgl.h"
#include <stdlib.h>
#include <string.h>

#define SCR_W 222
#define SCR_H 480
#define CS    16
#define COLS  10
#define ROWS  20
#define BX    ((SCR_W - COLS*CS)/2)
#define BY    28

static const int PCS[7][4][4][2] = {
    {{{0,0},{1,0},{2,0},{3,0}},{{0,0},{0,1},{0,2},{0,3}},{{0,0},{1,0},{2,0},{3,0}},{{0,0},{0,1},{0,2},{0,3}}},
    {{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}}},
    {{{0,0},{1,0},{2,0},{1,1}},{{0,0},{0,1},{0,2},{1,1}},{{1,0},{0,1},{1,1},{2,1}},{{0,0},{0,1},{0,2},{-1,1}}},
    {{{1,0},{2,0},{0,1},{1,1}},{{0,0},{0,1},{1,1},{1,2}},{{1,0},{2,0},{0,1},{1,1}},{{0,0},{0,1},{1,1},{1,2}}},
    {{{0,0},{1,0},{1,1},{2,1}},{{1,0},{0,1},{1,1},{0,2}},{{0,0},{1,0},{1,1},{2,1}},{{1,0},{0,1},{1,1},{0,2}}},
    {{{0,0},{0,1},{1,1},{2,1}},{{0,0},{1,0},{0,1},{0,2}},{{0,0},{1,0},{2,0},{2,1}},{{0,0},{0,1},{0,2},{-1,2}}},
    {{{2,0},{0,1},{1,1},{2,1}},{{0,0},{0,1},{0,2},{1,2}},{{0,0},{1,0},{2,0},{0,1}},{{0,0},{1,0},{1,1},{1,2}}},
};
static const lv_color_t PC[7] = {
    LV_COLOR_MAKE(0x00,0xFF,0xFF),LV_COLOR_MAKE(0xFF,0xFF,0x00),LV_COLOR_MAKE(0xAA,0x00,0xFF),
    LV_COLOR_MAKE(0x00,0xFF,0x00),LV_COLOR_MAKE(0xFF,0x00,0x00),LV_COLOR_MAKE(0x00,0x00,0xFF),
    LV_COLOR_MAKE(0xFF,0x88,0x00),
};

static uint8_t    bd[ROWS][COLS];
static lv_color_t bc[ROWS][COLS];
static int cp, cr, px, py, np;
static int score, level, lines;
static bool running;
static lv_obj_t *game_scr = NULL, *cells[ROWS][COLS];
static lv_obj_t *sl, *ll, *vl, *pv[4][4];
static lv_timer_t *ft = NULL;

static bool col(int tx, int ty, int pc, int rot);
static void draw_cell(int r, int c, lv_color_t cl);
static void clear_cell(int r, int c);
static void spawn(void);
static void lock(void);
static void clr_lines(void);
static void md(void), ml(void), mr(void), rot(void);
static void go(void);
static void restart(void);

lv_obj_t *tetris_game_create(void)
{
    game_scr = lv_obj_create(NULL);
    lv_obj_set_size(game_scr, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(game_scr, lv_color_black(), 0);
    lv_obj_clear_flag(game_scr, LV_OBJ_FLAG_SCROLLABLE);

    sl = lv_label_create(game_scr); lv_obj_set_style_text_color(sl, lv_color_white(), 0);
    lv_obj_align(sl, LV_ALIGN_TOP_LEFT, 4, 2);
    ll = lv_label_create(game_scr); lv_obj_set_style_text_color(ll, lv_color_white(), 0);
    lv_obj_align(ll, LV_ALIGN_TOP_LEFT, 4, 16);
    vl = lv_label_create(game_scr); lv_obj_set_style_text_color(vl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(vl, LV_ALIGN_TOP_RIGHT, -4, 2);

    lv_obj_t *pb = lv_obj_create(game_scr);
    lv_obj_set_size(pb, 4*CS, 4*CS); lv_obj_align(pb, LV_ALIGN_TOP_RIGHT, -4, 16);
    lv_obj_set_style_border_color(pb, lv_color_hex(0x505050), 0);
    lv_obj_set_style_border_width(pb, 1, 0);
    lv_obj_set_style_bg_opa(pb, LV_OPA_TRANSP, 0);
    lv_obj_t *pl = lv_label_create(game_scr);
    lv_label_set_text(pl, "Next"); lv_obj_set_style_text_color(pl, lv_color_hex(0x707070), 0);
    lv_obj_align_to(pl, pb, LV_ALIGN_OUT_TOP_MID, 0, 0);

    lv_obj_t *bb = lv_btn_create(game_scr);
    lv_obj_set_size(bb, 44, 20); lv_obj_align(bb, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *bl = lv_label_create(bb);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back"); lv_obj_center(bl);
    lv_obj_add_event_cb(bb, [](lv_event_t*) {
        if(ft){lv_timer_del(ft);ft=NULL;}
        lv_obj_del(game_scr);
    }, LV_EVENT_CLICKED, NULL);

    memset(pv, 0, sizeof(pv));
    for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++){
        lv_obj_t *o = lv_obj_create(game_scr);
        lv_obj_set_size(o, CS-1, CS-1); lv_obj_set_pos(o, BX+c*CS, BY+r*CS);
        lv_obj_set_style_bg_color(o, lv_color_hex(0x202020), 0);
        lv_obj_set_style_border_width(o, 1, 0);
        lv_obj_set_style_border_color(o, lv_color_hex(0x404040), 0);
        lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
        cells[r][c] = o;
    }
    restart();
     
    int base = BY + ROWS*CS + 12;  /* direction buttons row */
    struct { int x,y; const char *t; void (*cb)(void); } btns[] = {
        {12,base,LV_SYMBOL_LEFT,ml},{60,base,LV_SYMBOL_DOWN,md},
        {108,base,LV_SYMBOL_RIGHT,mr},{60,base+42,LV_SYMBOL_REFRESH,rot},
    };
    for(auto &b: btns){
        lv_obj_t *btn = lv_btn_create(game_scr);
        lv_obj_set_size(btn, 44, 36); lv_obj_set_pos(btn, b.x, b.y);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, b.t); lv_obj_center(l);
        lv_obj_add_event_cb(btn, [](lv_event_t *e) {
            void (*f)(void) = (void(*)(void))(intptr_t)lv_event_get_user_data(e);
            if(f && running) f();
        }, LV_EVENT_PRESSING, (void*)(intptr_t)b.cb);
    }
    lv_scr_load(game_scr);
    return game_scr;
}

static void spawn(void)
{
    cp=np; cr=0; px=COLS/2-2; py=0; np=rand()%7;
    if(col(px,py,cp,cr)){go();return;}
    for(int r=0;r<4;r++) for(int c=0;c<4;c++)
        if(pv[r][c]){lv_obj_del(pv[r][c]);pv[r][c]=NULL;}
    int pvx=SCR_W-8-4*CS, pvy=17;
    for(int i=0;i<4;i++){
        int cx=PCS[np][0][i][0], cy=PCS[np][0][i][1];
        lv_obj_t *o = lv_obj_create(game_scr);
        lv_obj_set_size(o, CS-2, CS-2); lv_obj_set_pos(o, pvx+cx*CS, pvy+cy*CS);
        lv_obj_set_style_bg_color(o, PC[np], 0); lv_obj_set_style_border_width(o, 0, 0);
        lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
        pv[cy][cx]=o;
    }
    for(int i=0;i<4;i++) draw_cell(py+PCS[cp][cr][i][1], px+PCS[cp][cr][i][0], PC[cp]);
}

static bool col(int tx, int ty, int pc, int rot){
    for(int i=0;i<4;i++){
        int x=tx+PCS[pc][rot][i][0], y=ty+PCS[pc][rot][i][1];
        if(x<0||x>=COLS||y>=ROWS) return true;
        if(y>=0&&bd[y][x]) return true;
    } return false;
}

static void lock(void){
    for(int i=0;i<4;i++){
        int x=px+PCS[cp][cr][i][0], y=py+PCS[cp][cr][i][1];
        if(y<0){go();return;}
        bd[y][x]=cp+1; bc[y][x]=PC[cp];
    } clr_lines(); spawn();
}

static void clr_lines(void){
    int clr=0;
    for(int r=ROWS-1;r>=0;r--){
        bool full=true;
        for(int c=0;c<COLS;c++) if(!bd[r][c]){full=false;break;}
        if(full){ clr++;
            for(int rr=r;rr>0;rr--) for(int c=0;c<COLS;c++){bd[rr][c]=bd[rr-1][c];bc[rr][c]=bc[rr-1][c];}
            for(int c=0;c<COLS;c++){bd[0][c]=0;bc[0][c]=lv_color_black();} r++;
        }
    }
    for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++) if(bd[r][c]) draw_cell(r,c,bc[r][c]); else clear_cell(r,c);
    if(clr>0){
        static const int ls[]={0,100,300,500,800};
        score+=ls[clr>4?4:clr]*(level+1); lines+=clr; level=lines/10;
        lv_label_set_text_fmt(sl,"Score: %d",score); lv_label_set_text_fmt(ll,"Lines: %d",lines);
        lv_label_set_text_fmt(vl,"Lvl %d",level);
        if(ft){int p=500-level*40;if(p<80)p=80;lv_timer_set_period(ft,p);}
    }
}

static void er(int r,int c){if(r>=0&&r<ROWS&&c>=0&&c<COLS)clear_cell(r,c);}
static void dr(int r,int c, lv_color_t cl){if(r>=0&&r<ROWS&&c>=0&&c<COLS)draw_cell(r,c,cl);}

#define ERASE for(int i=0;i<4;i++) er(py+PCS[cp][cr][i][1],px+PCS[cp][cr][i][0])
#define REDRAW for(int i=0;i<4;i++) dr(py+PCS[cp][cr][i][1],px+PCS[cp][cr][i][0],PC[cp])

static void md(void){ERASE; if(!col(px,py+1,cp,cr))py++;else{REDRAW;lock();return;}REDRAW;}
static void ml(void){ERASE; if(!col(px-1,py,cp,cr))px--; REDRAW;}
static void mr(void){ERASE; if(!col(px+1,py,cp,cr))px++; REDRAW;}
static void rot(void){
    ERASE; int nr=(cr+1)%4;
    if(!col(px,py,cp,nr))cr=nr;
    else if(!col(px-1,py,cp,nr)){px--;cr=nr;}
    else if(!col(px+1,py,cp,nr)){px++;cr=nr;}
    REDRAW;
}

static void go(void){
    running=false; if(ft){lv_timer_del(ft);ft=NULL;}
    lv_obj_t *m=lv_msgbox_create(NULL,"Game Over","Tap to restart",NULL,true);
    lv_obj_center(m);
    lv_obj_add_event_cb(m,[](lv_event_t*e){lv_obj_del(lv_event_get_current_target(e));restart();},LV_EVENT_CLICKED,NULL);
}

static void restart(void){
    memset(bd,0,sizeof(bd));
    for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++) clear_cell(r,c);
    score=level=lines=0; lv_label_set_text(sl,"Score: 0"); lv_label_set_text(ll,"Lines: 0"); lv_label_set_text(vl,"Lvl 0");
    running=true; np=rand()%7; spawn();
    if(ft)lv_timer_del(ft); ft=lv_timer_create([](lv_timer_t*){if(running)md();},500,NULL);
}

static void draw_cell(int r,int c,lv_color_t cl){lv_obj_set_style_bg_color(cells[r][c],cl,0);lv_obj_set_style_border_width(cells[r][c],0,0);}
static void clear_cell(int r,int c){bd[r][c]=0;bc[r][c]=lv_color_black();lv_obj_set_style_bg_color(cells[r][c],lv_color_hex(0x202020),0);lv_obj_set_style_border_width(cells[r][c],1,0);lv_obj_set_style_border_color(cells[r][c],lv_color_hex(0x404040),0);}