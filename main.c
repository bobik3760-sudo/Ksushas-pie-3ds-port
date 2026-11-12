/*
 * Nintendo 3DS front end: citro2d rendering, ndsp audio, touch + button input.
 * Platform independent game logic lives in game.c / ui.c.
 */
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "game.h"
#include "ui.h"

/* If sprites rotate the wrong way on real hardware, change this to -1.0f */
#define ROT_SIGN 1.0f

/* ------------------------------------------------------------- textures */
struct Tex {
    C3D_Tex tex;
    Tex3DS_SubTexture sub;
    C2D_Image img;
};

static float cur_w = TOP_W;            /* width of the screen being drawn (culling) */
static C2D_TextBuf tbuf;

Tex *plat_tex_load_file(const char *rel)
{
    char path[96];
    snprintf(path, sizeof path, "romfs:/%s", rel);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    uint16_t hd[4];
    if (fread(hd, 2, 4, f) != 4) { fclose(f); return NULL; }
    int w = hd[0], h = hd[1], tw = hd[2], th = hd[3];
    Tex *t = (Tex *)calloc(1, sizeof(Tex));
    if (!t) { fclose(f); return NULL; }
    if (!C3D_TexInit(&t->tex, (u16)tw, (u16)th, GPU_RGBA8)) { free(t); fclose(f); return NULL; }
    size_t bytes = (size_t)tw * th * 4;
    size_t got = fread(t->tex.data, 1, bytes, f);     /* already swizzled by tools/convert.py */
    fclose(f);
    if (got != bytes) { C3D_TexDelete(&t->tex); free(t); return NULL; }
    GSPGPU_FlushDataCache(t->tex.data, bytes);
    C3D_TexSetFilter(&t->tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&t->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    t->sub.width = (u16)w;  t->sub.height = (u16)h;
    t->sub.left = 0.0f;     t->sub.right = (float)w / tw;
    t->sub.top = 1.0f;      t->sub.bottom = 1.0f - (float)h / th;
    t->img.tex = &t->tex;   t->img.subtex = &t->sub;
    return t;
}

/* The GPU may still be using last frame's textures: free a few frames later. */
#define DEFER 8
static struct { Tex *t; int age; } dead[DEFER * 4];
static int ndead;

void plat_tex_free(Tex *t)
{
    if (!t) return;
    if (ndead < (int)(sizeof dead / sizeof dead[0])) { dead[ndead].t = t; dead[ndead].age = 0; ndead++; }
    else { C3D_TexDelete(&t->tex); free(t); }
}

static void reap_textures(void)
{
    int j = 0;
    for (int i = 0; i < ndead; i++) {
        if (++dead[i].age > 4) { C3D_TexDelete(&dead[i].t->tex); free(dead[i].t); }
        else dead[j++] = dead[i];
    }
    ndead = j;
}

/* ------------------------------------------------------------- drawing */
void plat_draw(Tex *t, float cx, float cy, float w, float h, float angle, float alpha)
{
    if (!t) return;
    float r = 0.5f * sqrtf(w * w + h * h);
    if (cx + r < 0 || cx - r > cur_w || cy + r < 0 || cy - r > SCR_H) return;
    C2D_DrawParams p;
    p.pos.x = cx; p.pos.y = cy; p.pos.w = w; p.pos.h = h;
    p.center.x = w * 0.5f; p.center.y = h * 0.5f;
    p.depth = 0.0f;
    p.angle = angle * ROT_SIGN;
    if (alpha < 0.999f) {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32f(1, 1, 1, alpha), 0.0f);
        C2D_DrawImage(t->img, &p, &tint);
    } else {
        C2D_DrawImage(t->img, &p, NULL);
    }
}

void plat_draw_part(Tex *t, int sx, int sy, int sw, int sh, float dx, float dy)
{
    if (!t || sw <= 0 || sh <= 0) return;
    float tw = (float)t->tex.width, th = (float)t->tex.height;
    Tex3DS_SubTexture st;
    st.width = (u16)sw; st.height = (u16)sh;
    st.left = sx / tw;  st.right = (sx + sw) / tw;
    st.top = 1.0f - sy / th; st.bottom = 1.0f - (sy + sh) / th;
    C2D_Image im = { &t->tex, &st };
    C2D_DrawImageAt(im, dx, dy, 0.0f, NULL, 1.0f, 1.0f);
}

void plat_rect(float x, float y, float w, float h, float r, float g, float b, float a)
{
    C2D_DrawRectSolid(x, y, 0.0f, w, h, C2D_Color32f(r, g, b, a));
}

void plat_text(const char *s, float x, float y, float scale, float r, float g, float b)
{
    C2D_Text txt;
    C2D_TextParse(&txt, tbuf, s);
    C2D_TextOptimize(&txt);
    C2D_DrawText(&txt, C2D_WithColor, x, y, 0.0f, scale, scale, C2D_Color32f(r, g, b, 1.0f));
}

/* --------------------------------------------------------------- audio */
#define NCH 4
static bool audio_ok;
static void *snd_data[16];
static ndspWaveBuf wbuf[NCH];
static int next_sfx = 1;

static void *load_snd(int id)
{
    if (snd_data[id]) return snd_data[id];
    size_t bytes = (size_t)g_sounds[id].samples * 4;
    char path[48];
    snprintf(path, sizeof path, "romfs:/snd/%02d.pcm", id);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    void *buf = linearAlloc(bytes);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, bytes, f);
    fclose(f);
    if (got != bytes) { linearFree(buf); return NULL; }
    DSP_FlushDataCache(buf, bytes);
    snd_data[id] = buf;
    return buf;
}

void plat_snd_play(int id)
{
    if (!audio_ok || id < 0 || id >= g_nsounds || id >= 16) return;
    void *buf = load_snd(id);
    if (!buf) return;
    /* channel 0 is kept for long sounds (music), 1..3 for short effects */
    int ch;
    if (g_sounds[id].samples > SND_RATE * 10) ch = 0;
    else { ch = next_sfx; next_sfx = next_sfx % (NCH - 1) + 1; }
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, (float)SND_RATE);
    ndspChnSetFormat(ch, NDSP_FORMAT_STEREO_PCM16);
    float mix[12] = { 0 };
    mix[0] = mix[1] = 1.0f;
    ndspChnSetMix(ch, mix);
    memset(&wbuf[ch], 0, sizeof wbuf[ch]);
    wbuf[ch].data_vaddr = buf;
    wbuf[ch].nsamples = g_sounds[id].samples;
    ndspChnWaveBufAdd(ch, &wbuf[ch]);
}

void plat_snd_stop_all(void)
{
    if (!audio_ok) return;
    for (int i = 0; i < NCH; i++) ndspChnReset(i);
}

/* ---------------------------------------------------------------- main */
#define STEP (1.0f / 60.0f)

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    gfxInitDefault();
    romfsInit();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    tbuf = C2D_TextBufNew(2048);

    audio_ok = R_SUCCEEDED(ndspInit());       /* needs sdmc:/3ds/dspfirm.cdc; game runs silent without it */
    if (audio_ok) ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    ui_init();
    game_init();

    float cx = 0, cy = 0;                     /* cursor = "finger" for button play */
    bool cursor_visible = true;
    u64 last = svcGetSystemTick();
    float acc = 0;

    while (aptMainLoop()) {
        u64 now = svcGetSystemTick();
        acc += (float)((double)(now - last) / (double)SYSCLOCK_ARM11);
        last = now;
        if (acc > 0.1f) acc = 0.1f;

        hidScanInput();
        u32 down = hidKeysDown(), held = hidKeysHeld();
        if (down & KEY_START) break;
        if (down & KEY_SELECT) game_restart();

        /* ---- touch screen */
        touchPosition tp;
        hidTouchRead(&tp);
        bool touching = (held & KEY_TOUCH) != 0;
        if (touching) {
            ui_touch_to_stage(tp.px, tp.py, &cx, &cy);
            cursor_visible = false;
            if (down & KEY_TOUCH) game_tap(cx, cy);
        }

        /* ---- buttons: Circle Pad / D-Pad move the cursor, A/B/X/Y tap */
        circlePosition cp;
        hidCircleRead(&cp);
        float vx = 0, vy = 0;
        if (abs(cp.dx) > 18) vx += cp.dx / 156.0f;
        if (abs(cp.dy) > 18) vy += cp.dy / 156.0f;
        if (held & KEY_DLEFT)  vx -= 1.0f;
        if (held & KEY_DRIGHT) vx += 1.0f;
        if (held & KEY_DUP)    vy += 1.0f;
        if (held & KEY_DDOWN)  vy -= 1.0f;
        float speed = 1100.0f;                       /* stage units / second */
        if (held & KEY_R) speed *= 2.0f;
        if (held & KEY_L) speed *= 0.3f;
        if (vx != 0 || vy != 0) cursor_visible = true;
        if (down & (KEY_A | KEY_B | KEY_X | KEY_Y)) { cursor_visible = true; game_tap(cx, cy); }

        /* ---- fixed 60 Hz logic like Pocket Code */
        int steps = 0;
        while (acc >= STEP && steps < 4) {
            if (!touching) {
                cx += vx * speed * STEP; cy += vy * speed * STEP;
                float hx = g_stage_w / 2.0f, hy = g_stage_h / 2.0f;
                if (cx < -hx) cx = -hx;
                if (cx > hx) cx = hx;
                if (cy < -hy) cy = -hy;
                if (cy > hy) cy = hy;
            }
            game_set_touch(cx, cy);
            game_update(STEP);
            acc -= STEP; steps++;
        }
        if (steps == 4) acc = 0;

        /* ---- render */
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(tbuf);
        C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(top);
        cur_w = TOP_W;
        ui_draw_top(cursor_visible, cx, cy);
        C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(bot);
        cur_w = BOT_W;
        ui_draw_bottom(cursor_visible, cx, cy);
        C3D_FrameEnd(0);
        reap_textures();
    }

    plat_snd_stop_all();
    ui_exit();
    for (int i = 0; i < ndead; i++) { C3D_TexDelete(&dead[i].t->tex); free(dead[i].t); }
    for (int i = 0; i < 16; i++) if (snd_data[i]) linearFree(snd_data[i]);
    if (audio_ok) ndspExit();
    C2D_TextBufDelete(tbuf);
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();
    return 0;
}
