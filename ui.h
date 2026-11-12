/* Screen layout + drawing shared by the 3DS build and the Linux simulator. */
#ifndef UI_H
#define UI_H
#include <stdbool.h>

#define TOP_W 400
#define BOT_W 320
#define SCR_H 240

void ui_init(void);
void ui_exit(void);
/* cursor (stage coords) is drawn when show_cursor is set */
void ui_draw_top(bool show_cursor, float cur_x, float cur_y);
void ui_draw_bottom(bool show_cursor, float cur_x, float cur_y);
/* bottom-screen touch pixel -> stage coordinates (clamped to the stage) */
void ui_touch_to_stage(int px, int py, float *sx, float *sy);
#endif
