#include "ui.h"
#include "game.h"

/* The stage (2507x1220) is scaled uniformly to the screen width and centred:
 *   top    : 400 x 195 px  (black bars above / below)
 *   bottom : 320 x 156 px  mini view inside the green grid pad.
 * Touching the mini view = touching the stage at that spot. */
static Tex *grid;

static float kt(void) { return (float)TOP_W / g_stage_w; }
static float kb(void) { return (float)BOT_W / g_stage_w; }

void ui_init(void) { grid = plat_tex_load_file("grid.tex"); }
void ui_exit(void) { if (grid) plat_tex_free(grid); grid = 0; }

static void cursor(float x, float y)
{
    /* crosshair with black outline so it is visible on every background */
    plat_rect(x - 8, y - 1.5f, 16, 3, 0, 0, 0, 1);
    plat_rect(x - 1.5f, y - 8, 3, 16, 0, 0, 0, 1);
    plat_rect(x - 7, y - 0.5f, 14, 1, 1, 1, 0.2f, 1);
    plat_rect(x - 0.5f, y - 7, 1, 14, 1, 1, 0.2f, 1);
}

void ui_draw_top(bool show_cursor, float cx, float cy)
{
    float k = kt();
    plat_rect(0, 0, TOP_W, SCR_H, 0, 0, 0, 1);
    game_draw(TOP_W / 2.0f, SCR_H / 2.0f, k);
    float bar = (SCR_H - g_stage_h * k) / 2.0f;       /* letterbox: hides sprites that leave the stage */
    plat_rect(0, 0, TOP_W, bar, 0, 0, 0, 1);
    plat_rect(0, SCR_H - bar, TOP_W, bar + 1, 0, 0, 0, 1);
    if (show_cursor) cursor(TOP_W / 2.0f + cx * k, SCR_H / 2.0f - cy * k);
}

void ui_draw_bottom(bool show_cursor, float cx, float cy)
{
    float k = kb();
    float top = (SCR_H - g_stage_h * k) / 2.0f;       /* mini view y range */
    float bot = top + g_stage_h * k;
    plat_rect(0, 0, BOT_W, SCR_H, 0, 0, 0, 1);
    game_draw(BOT_W / 2.0f, SCR_H / 2.0f, k);
    /* green grid pad above and below the mini view (also hides overflow) */
    int t = (int)top, b = (int)(bot + 0.999f);
    if (grid) {
        plat_draw_part(grid, 0, 0, BOT_W, t, 0, 0);
        plat_draw_part(grid, 0, b, BOT_W, SCR_H - b, 0, (float)b);
    }
    plat_rect(0, (float)t - 1, BOT_W, 1, 0.45f, 1.0f, 0.4f, 1);
    plat_rect(0, (float)b, BOT_W, 1, 0.45f, 1.0f, 0.4f, 1);
    plat_text("TOUCH THE PAD", 8, 6, 0.55f, 0.6f, 1, 0.5f);
    plat_text("A/B/X/Y: tap   D-Pad / Circle Pad: move   R: fast  L: slow", 4, (float)b + 6, 0.38f, 0.8f, 1, 0.7f);
    plat_text("SELECT: restart   START: exit", 4, (float)b + 20, 0.38f, 0.8f, 1, 0.7f);
    if (show_cursor) cursor(BOT_W / 2.0f + cx * k, SCR_H / 2.0f - cy * k);
}

void ui_touch_to_stage(int px, int py, float *sx, float *sy)
{
    float k = kb();
    float x = (px - BOT_W / 2.0f) / k, y = (SCR_H / 2.0f - py) / k;
    float hx = g_stage_w / 2.0f, hy = g_stage_h / 2.0f;
    if (x < -hx) x = -hx;
    if (x > hx) x = hx;
    if (y < -hy) y = -hy;
    if (y > hy) y = hy;
    *sx = x; *sy = y;
}
