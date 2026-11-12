/*
 * Mini Catrobat (Pocket Code) brick interpreter.
 *
 * Semantics follow Pocket Code:
 *  - stage origin in the centre, y up, units = project pixels (2507 x 1220)
 *  - direction 90 = right, 0 = up, clockwise; looks rotate with the direction
 *  - Forever loops run one iteration per frame, Wait always lasts >= 1 frame
 *  - "When tapped" scripts start on touch-down, broadcast scripts restart
 *    when their message is sent again, scene change happens at end of frame
 *  - Clone copies the sprite as it is right now (clones run no scripts)
 */
#include "game.h"
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define PI 3.14159265358979f
#define TAP_SLOP 45.0f          /* extra touch radius in stage units (finger friendly) */

typedef struct { bool running; int pc; float wait; int spr; const Script *sc; } Thread;

static int   cur_scene = -1, next_scene = 0;
static Inst  inst[MAX_INST];
static int   ninst, nspr, nclones;
static Tex  *looktex[MAX_SPR][MAX_LOOKS];
static Thread thr[MAX_THREADS];
static int   nthr;
static float touch_x, touch_y;
static uint32_t rng = 0x2545F491u;

static float frand(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (rng & 0xFFFFFF) / 16777216.0f;
}

static void unload_scene(void)
{
    for (int s = 0; s < MAX_SPR; s++)
        for (int l = 0; l < MAX_LOOKS; l++)
            if (looktex[s][l]) { plat_tex_free(looktex[s][l]); looktex[s][l] = NULL; }
    plat_snd_stop_all();
    ninst = nspr = nclones = nthr = 0;
}

static void load_scene(int si)
{
    unload_scene();
    cur_scene = si;
    const SceneDef *sc = &g_scenes[si];
    nspr = sc->nsprites > MAX_SPR ? MAX_SPR : sc->nsprites;
    for (int i = 0; i < nspr; i++) {
        const SpriteDef *d = &sc->sprites[i];
        Inst *s = &inst[i];
        s->x = s->y = 0; s->size = 100; s->dir = 90;
        s->visible = true; s->clone = false; s->look = 0; s->def = i;
        for (int l = 0; l < d->nlooks && l < MAX_LOOKS; l++)
        {
            char path[32];
            snprintf(path, sizeof path, "tex/%03d.tex", d->looks[l].tex);
            looktex[i][l] = plat_tex_load_file(path);
        }
        for (int k = 0; k < d->nscripts && nthr < MAX_THREADS; k++) {
            Thread *t = &thr[nthr++];
            t->sc = &d->scripts[k]; t->spr = i; t->pc = 0; t->wait = 0;
            t->running = (t->sc->type == SC_START);
        }
    }
    ninst = nspr;
}

void game_init(void)    { cur_scene = -1; next_scene = 0; }
void game_restart(void) { next_scene = 0; }

static void start_thread(Thread *t) { t->running = true; t->pc = 0; t->wait = 0; }

static void run_thread(Thread *t, float dt)
{
    Inst *s = &inst[t->spr];
    const Brick *b = t->sc->bricks;
    const int n = t->sc->n;

    if (t->wait > 0) {                       /* still waiting from an earlier frame */
        t->wait -= dt;
        if (t->wait > 0) return;
        t->wait = 0;
    }
    while (t->pc < n) {
        const Brick *k = &b[t->pc++];
        switch (k->op) {
        case OP_PLACE_AT:  s->x = k->a; s->y = k->b; break;
        case OP_HIDE:      s->visible = false; break;
        case OP_SHOW:      s->visible = true;  break;
        case OP_SET_SIZE:  s->size = k->a; break;
        case OP_WAIT:
            t->wait = k->a - dt;             /* the frame we are in counts */
            if (t->wait > 0) return;
            t->wait = 0; break;
        case OP_SCENE_START:
            next_scene = k->i; t->running = false; return;
        case OP_FOREVER:   break;
        case OP_LOOP_END:  t->pc = k->i; return;   /* one iteration per frame */
        case OP_NEXT_LOOK: {
            int nl = g_scenes[cur_scene].sprites[s->def].nlooks;
            if (nl > 0) s->look = (s->look + 1) % nl;
        } break;
        case OP_PLAY_SOUND: plat_snd_play(k->i); break;
        case OP_PLAY_SOUND_WAIT:
            plat_snd_play(k->i);
            t->wait = (float)g_sounds[k->i].samples / SND_RATE - dt;
            if (t->wait > 0) return;
            t->wait = 0; break;
        case OP_MOVE_STEPS: {
            float r = s->dir * PI / 180.0f;
            s->x += k->a * sinf(r); s->y += k->a * cosf(r);
        } break;
        case OP_TURN_LEFT:  s->dir -= k->a; break;
        case OP_TURN_RIGHT: s->dir += k->a; break;
        case OP_POINT_TO: {
            if (k->i >= 0 && k->i < nspr) {
                float dx = inst[k->i].x - s->x, dy = inst[k->i].y - s->y;
                if (dx != 0 || dy != 0) s->dir = atan2f(dx, dy) * 180.0f / PI;
            }
        } break;
        case OP_BROADCAST:
            for (int i = 0; i < nthr; i++)
                if (thr[i].sc->type == SC_BROADCAST && thr[i].sc->msg == k->i)
                    start_thread(&thr[i]);
            break;
        case OP_GOTO_TOUCH:  s->x = touch_x; s->y = touch_y; break;
        case OP_GOTO_RANDOM:
            s->x = (frand() - 0.5f) * g_stage_w;
            s->y = (frand() - 0.5f) * g_stage_h; break;
        case OP_CLONE:
            if (nclones < MAX_CLONES && ninst < MAX_INST) {
                inst[ninst] = *s; inst[ninst].clone = true; ninst++; nclones++;
            }
            break;
        }
    }
    t->running = false;
}

void game_update(float dt)
{
    if (next_scene >= 0) { int s = next_scene; next_scene = -1; load_scene(s); }
    for (int i = 0; i < nthr; i++)
        if (thr[i].running) run_thread(&thr[i], dt);
}

void game_set_touch(float sx, float sy) { touch_x = sx; touch_y = sy; }

static bool hit(const Inst *s, float tx, float ty)
{
    const Look *l = &g_scenes[cur_scene].sprites[s->def].looks[s->look];
    float phi = (s->dir - 90.0f) * PI / 180.0f;          /* visible clockwise angle */
    float dx = tx - s->x, dy = ty - s->y;
    float lx = dx * cosf(phi) - dy * sinf(phi);
    float ly = dx * sinf(phi) + dy * cosf(phi);
    float hw = l->w * s->size / 200.0f + TAP_SLOP;
    float hh = l->h * s->size / 200.0f + TAP_SLOP;
    return fabsf(lx) <= hw && fabsf(ly) <= hh;
}

void game_tap(float sx, float sy)
{
    if (cur_scene < 0) return;
    touch_x = sx; touch_y = sy;
    /* topmost visible sprite that actually reacts to taps gets the touch */
    for (int i = nspr - 1; i >= 0; i--) {
        const SpriteDef *d = &g_scenes[cur_scene].sprites[i];
        if (!inst[i].visible || d->nlooks == 0) continue;
        bool has_tap = false;
        for (int k = 0; k < d->nscripts; k++) if (d->scripts[k].type == SC_TAP) has_tap = true;
        if (!has_tap || !hit(&inst[i], sx, sy)) continue;
        for (int t = 0; t < nthr; t++)
            if (thr[t].spr == i && thr[t].sc->type == SC_TAP) start_thread(&thr[t]);
        return;
    }
}

void game_draw(float ox, float oy, float k)
{
    if (cur_scene < 0) return;
    for (int i = 0; i < ninst; i++) {
        const Inst *s = &inst[i];
        if (!s->visible) continue;
        const SpriteDef *d = &g_scenes[cur_scene].sprites[s->def];
        if (d->nlooks == 0 || !looktex[s->def][s->look]) continue;
        const Look *l = &d->looks[s->look];
        float w = l->w * s->size / 100.0f * k, h = l->h * s->size / 100.0f * k;
        plat_draw(looktex[s->def][s->look], ox + s->x * k, oy - s->y * k, w, h,
                  (s->dir - 90.0f) * PI / 180.0f, 1.0f);
    }
}

int game_scene(void)              { return cur_scene; }
const char *game_scene_name(void) { return cur_scene >= 0 ? g_scenes[cur_scene].name : ""; }
int game_ninst(void)              { return ninst; }
const Inst *game_inst(int i)      { return &inst[i]; }
const SpriteDef *game_def(int i)  { return &g_scenes[cur_scene].sprites[i]; }
