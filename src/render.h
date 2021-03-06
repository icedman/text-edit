#ifndef TE_RENDER_H
#define TE_RENDER_H

#include "menu.h"
#include "ui.h"
#include "view.h"

enum color_pair_e { NORMAL = 0, SELECTED, COMMENT };

int color_index(int r, int g, int b);
int pair_for_color(int colorIdx, bool selected = false,
                   bool highlighted = false);
void update_colors();

void draw_clear(int w);
void draw_text(rect_t rect, const char *text, int align = 1, int margin = 0,
               int color = -1);
void draw_text(view_ptr view, const char *text, int align = 1, int margin = 0,
               int color = -1);
void draw_menu(menu_ptr menu, int color = 0, int selected_color = 0);
void draw_status(status_line_t *status);
void draw_styled_text(view_ptr view, const char *text, int row, int col,
                      std::vector<textstyle_t> &styles,
                      std::vector<textstyle_t> *extra_styles = 0,
                      bool wrap = false, int *height = 0);

#endif //  TE_RENDER_H