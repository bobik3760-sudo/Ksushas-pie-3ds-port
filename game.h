/*
 * Ksusha cooks a pie 3 -- 3DS port of a Pocket Code (Catrobat) project.
 *
 * game.c   : platform independent engine (mini Catrobat brick interpreter)
 * gamedata.c: GENERATED from the .catrobat file by tools/convert.py
 * main.c   : Nintendo 3DS front end (citro2d, ndsp, input)
 * sim/     : Linux software renderer used to test game.c without a 3DS
 */
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

/* ------------------------------------------------------------ data types */
enum {
    OP_PLACE_AT, OP_HIDE, OP_SHOW, OP_WAIT, OP_SET_SIZE, OP_SCENE_START,
    OP_FOREVER, OP_LOOP_END, OP_NEXT_LOOK, OP_PLAY_SOUND, OP_PLAY_SOUND_WAIT,
    OP_MOVE_STEPS, OP_POINT_TO, OP_BROADCAST, OP_GOTO_TOUCH, OP_GOTO_RANDOM,
    OP_CLONE, OP_TURN_LEFT, OP_TURN_RIGHT
};
enum { SC_START, SC_TAP, SC_BROADCAST };

typedef struct { int op; float a, b; int i; } Brick;
typedef struct { int type; int msg; int n; const Brick *bricks; } Script;
typedef struct { int tex, w, h; } Look;            /* w,h = size in stage units  */
typedef struct { const char *name; int nlooks; const Look *looks;
                 int nscripts; const Script *scripts; } SpriteDef;
typedef struct { const char *name; int nsprites; const SpriteDef *sprites; } SceneDef;
typedef struct { int samples; } SoundDef;           /* 32 kHz stereo PCM16        */
typedef struct { int w, h, tw, th; } TexDef;        /* image size / padded size   */

extern const SceneDef g_scenes[];  extern const int g_nscenes;
extern const SoundDef g_sounds[];  extern const int g_nsounds;
extern const TexDef   g_texdefs[]; extern const int g_ntex;
extern const int g_stage_w, g_stage_h;

#define SND_RATE 32000

/* ------------------------------------------------------- platform layer */
typedef struct Tex Tex;                      /* opaque, owned by the platform */
Tex  *plat_tex_load_file(const char *relpath);   /* romfs:/<relpath> (.tex)   */
void  plat_tex_free(Tex *t);
/* draw centred at (cx,cy) screen px, size (w,h) px, clockwise angle in radians */
void  plat_draw(Tex *t, float cx, float cy, float w, float h, float angle, float alpha);
/* draw part (sx,sy,sw,sh in image px) of a texture 1:1 with its top-left at (dx,dy) */
void  plat_draw_part(Tex *t, int sx, int sy, int sw, int sh, float dx, float dy);
void  plat_rect(float x, float y, float w, float h, float r, float g, float b, float a);
void  plat_text(const char *s, float x, float y, float scale, float r, float g, float b);
void  plat_snd_play(int snd_id);
void  plat_snd_stop_all(void);

/* ----------------------------------------------------------- engine API */
#define MAX_INST 700           /* sprites + clones                         */
#define MAX_CLONES 500
#define MAX_THREADS 48
#define MAX_SPR 16
#define MAX_LOOKS 8

typedef struct {
    float x, y, size, dir;     /* Catrobat: stage units, percent, degrees  */
    bool  visible, clone;
    int   look, def;           /* def = sprite index inside the scene      */
} Inst;

void game_init(void);          /* starts the first scene                   */
void game_restart(void);
void game_update(float dt);    /* one logic step (call at 60 Hz)           */
void game_tap(float sx, float sy);          /* touch-down at stage coords  */
void game_set_touch(float sx, float sy);    /* "touch position" for GoTo   */
/* draw whole stage: stage centre is at screen (ox,oy), scale k px/unit    */
void game_draw(float ox, float oy, float k);
int  game_scene(void);
const char *game_scene_name(void);
int  game_ninst(void);
const Inst *game_inst(int i);
const SpriteDef *game_def(int i);
#endif
