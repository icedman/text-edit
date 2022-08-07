#include "render.h"
#include "textmate.h"

#include <curses.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <map>

#define SELECTED_OFFSET 500
#define HIGHLIGHT_OFFSET 1000

static std::map<int, int> colorMap;
const wchar_t *symbol_tab = L"\u2847";

bool use_system_colors = true;
int fg = 0;
int bg = 0;
int hl = 0;
int sel = 0;
int cmt = 0;
int fn = 0;
int kw = 0;
int var = 0;

void init_renderer()
{
  setlocale(LC_ALL, "");

  initscr();
  raw();
  keypad(stdscr, true);
  noecho();
  nodelay(stdscr, true);

  if (use_system_colors) {
    use_default_colors();
  }
  start_color();
}

void shutdown_renderer()
{
  endwin();
}

int color_index(int r, int g, int b) {
  return color_info_t::nearest_color_index(r, g, b);
}

int pair_for_color(int colorIdx, bool selected, bool highlighted) {
  if (selected && colorIdx == color_pair_e::NORMAL) {
    return color_pair_e::SELECTED;
  }
  int offset =
      selected ? SELECTED_OFFSET : (highlighted ? HIGHLIGHT_OFFSET : 0);
  return colorMap[colorIdx + offset];
}

void update_colors() {
  colorMap.clear();

  theme_info_t info = Textmate::theme_info();

  if (use_system_colors) {
    fg = -1; // info.fg_a;
    bg = -1; // info.bg_a;
  } else {
    fg = info.fg_a;
    bg = COLOR_BLACK; // info.bg_a; // color_index(info.bg_r, info.bg_g,
                      // info.bg_b);
  }
  cmt = color_index(info.cmt_r, info.cmt_g, info.cmt_b);
  sel = color_index(info.sel_r, info.sel_g, info.sel_b);
  fn = color_index(info.fn_r, info.fn_g, info.fn_b);
  kw = color_index(info.kw_r, info.kw_g, info.kw_b);
  var = color_index(info.var_r, info.var_g, info.var_b);
  hl = color_index(info.sel_r / 1.5, info.sel_g / 1.5, info.sel_b / 1.5);
  // hl = color_index(info.sel_r / 1.5, info.sel_g / 1.5, info.sel_b / 1.5);

  theme_ptr theme = Textmate::theme();

  //---------------
  // build the color pairs
  //---------------
  init_pair(color_pair_e::NORMAL, fg, bg);
  init_pair(color_pair_e::SELECTED, fg, sel);

  theme->colorIndices[fg] = color_info_t({0, 0, 0, fg});
  theme->colorIndices[cmt] = color_info_t({0, 0, 0, cmt});
  theme->colorIndices[fn] = color_info_t({0, 0, 0, fn});
  theme->colorIndices[kw] = color_info_t({0, 0, 0, kw});

  int idx = 32;

  auto it = theme->colorIndices.begin();
  while (it != theme->colorIndices.end()) {
    colorMap[it->first] = idx;
    init_pair(idx++, it->first, bg);
    it++;
  }

  it = theme->colorIndices.begin();
  while (it != theme->colorIndices.end()) {
    colorMap[it->first + SELECTED_OFFSET] = idx;
    init_pair(idx++, it->first, sel);
    if (it->first == sel) {
      colorMap[it->first + SELECTED_OFFSET] = idx + 1;
    }
    it++;
  }

  it = theme->colorIndices.begin();
  while (it != theme->colorIndices.end()) {
    colorMap[it->first + HIGHLIGHT_OFFSET] = idx;
    init_pair(idx++, it->first, hl);
    if (it->first == sel) {
      colorMap[it->first + HIGHLIGHT_OFFSET] = idx + 1;
    }
    it++;
  }
}

void draw_clear(int w) {
  for (int i = 0; i < w; i++) {
    addch(' ');
  }
}

void draw_text(rect_t rect, const char *text, int align, int margin,
               int color) {
  int off = 0;
  switch (align) {
  case 0:
    // center
    off = rect.w / 2 - strlen(text) / 2;
    break;
  case -1:
    // left
    off = margin;
    break;
  case 1:
    // right
    off = rect.w - strlen(text) - margin;
    break;
  }

  int screen_col = rect.x + off;
  int screen_row = rect.y;

  int pair = color != -1 ? color : pair_for_color(cmt);
  attron(COLOR_PAIR(pair));

  move(screen_row, rect.x);
  for (int i = 0; i < rect.w; i++) {
    addch(' ');
  }
  move(screen_row, screen_col);
  addstr(text);

  attroff(COLOR_PAIR(pair));
}

void draw_text(view_ptr view, const char *text, int align, int margin,
               int color) {
  if (!view->show)
    return;
  draw_text(view->computed, text, align, margin);
}

void draw_menu(menu_ptr menu, int color, int selected_color) {
  if (!menu->show || !menu->items.size())
    return;

  int row = 0;
  int def = color ? color : pair_for_color(cmt, false, true);
  int sel = selected_color ? selected_color : pair_for_color(fg, true, false);

  int margin = 1;
  int w = menu->computed.w;
  int h = menu->computed.h;
  int screen_row = menu->computed.y;
  int screen_col = menu->computed.x;
  int start = menu->scroll.y;
  int offset_row = 0;

  // printf("%d %d %d %d\n", w, h, screen_row, screen_col);

  for (int i = start; i < menu->items.size(); i++) {
    auto m = menu->items[i];
    int pair = menu->selected == i ? sel : def;
    int sc = screen_row + row++;
    if (menu->selected == i) {
      menu->cursor.y = sc + offset_row;
    }

    std::string text = m.name.substr(0, w - 1);
    attron(COLOR_PAIR(pair));
    move(sc + offset_row, screen_col);
    draw_clear(w);
    move(sc + offset_row, screen_col + margin);
    addstr(text.c_str());
    attroff(COLOR_PAIR(pair));
    if (row > h)
      break;
  }

  for (int i = row; i < h; i++) {
    int sc = screen_row + row++;
    move(sc + offset_row, screen_col);
    draw_clear(w);
  }
}

void draw_status(status_line_t *status) {
  if (!status->show)
    return;
  int sel = pair_for_color(fg, true, false);
  for (auto placeholder : status->items) {
    status_item_ptr item = placeholder.second;
    if (!item->show)
      continue;
    int pair = item->color ? item->color : sel;
    draw_text({item->computed.x, item->computed.y, item->computed.w,
               item->computed.h},
              item->text.c_str(), item->align, 0, pair);
  }
}

void draw_styled_text(view_ptr view, const char *text, int row, int col,
                      std::vector<textstyle_t> &styles,
                      std::vector<textstyle_t> *extra_styles, bool wrap,
                      int *height) {

  int scroll_x = view->scroll.x;
  int scroll_y = view->scroll.y;

  int screen_row = view->computed.y + row;
  int screen_col = view->computed.x + col;
  move(screen_row, screen_col);
  clrtoeol();

  int l = strlen(text);
  if (height) {
    *height = 1;
  }

  int default_pair = pair_for_color(fg, false, false);
  for (int i = scroll_x; i < l; i++) {
    int pair = default_pair;

    if (i - scroll_x + 1 > (view->computed.w * (*height))) {
      if (!wrap) {
        break;
      }
      screen_col = view->computed.x;
      screen_row++;
      *height = (*height) + 1;
      move(screen_row, screen_col);
      clrtoeol();
    }

    char ch = text[i];
    bool underline = false;
    bool reverse = false;
    bool strike = false;
    bool selected = false;
    bool highlighted = false;
    int colorIdx = 0;

    // syntax highlights
    for (auto s : styles) {
      if (s.start >= i && i < s.start + s.length) {
        underline = s.underline;
        colorIdx = color_index(s.r, s.g, s.b);
        break;
      }
    }

    if (extra_styles) {
      for (auto s : *extra_styles) {
        if (s.start <= i && i < s.start + s.length) {
          underline = s.underline;
          if (s.bg_a != 0) {
            highlighted = s.bg_a == 2;
            selected = s.bg_a == 1;
          }
          if (s.caret != 0) {
            reverse = true;
            view->cursor.x = screen_col + i;
            view->cursor.y = screen_row;
          }
          // tabstop
          if (s.strike) {
            strike = true;
            colorIdx = cmt;
          }
        }
      }
    }

    if (underline) {
      attron(A_UNDERLINE);
    }
    if (reverse) {
      attron(A_REVERSE);
    }

    // render symbols
    wchar_t *symbol = NULL;

    // tab stop
    if (strike) {
      symbol = (wchar_t *)symbol_tab;
    }

    pair = pair_for_color(colorIdx, selected, highlighted);
    pair = pair > 0 ? pair : default_pair;

    // render the character
    attron(COLOR_PAIR(pair));
    if (symbol != NULL) {
      addwstr(symbol);
    } else {
      addch(ch);
    }
    attroff(COLOR_PAIR(pair));

    // attroff(A_BLINK);
    // attroff(A_STANDOUT);
    attroff(A_REVERSE);
    attroff(A_BOLD);
    attroff(A_UNDERLINE);
  }
}

void _move(int x, int y)
{
  move(x, y);
}

void _attron(int attr)
{
  attron(attr);
}

void _attroff(int attr)
{
  attroff(attr);
}

void _clear()
{
  clear();
}

void _refresh()
{
  refresh();
}

void _addstr(const char *text)
{
  addstr(text);
}

void _addwstr(const wchar_t *text)
{
  addwstr(text);
}

void _addch(char ch)
{
  addch(ch);
}

void _clrtoeol()
{
  clrtoeol();
}

void _curs_set(int i)
{
  curs_set(i);
}

int _COLOR_PAIR(int i)
{
  return COLOR_PAIR(i);
}

void _underline(bool on)
{
  if (on) {
    attron(A_UNDERLINE);
  } else {
    attroff(A_UNDERLINE);
  }
}

void _reverse(bool on)
{
  if (on) {
    attron(A_REVERSE);
  } else {
    attroff(A_REVERSE);
  }
}

void _bold(bool on)
{
  if (on) {
    attron(A_BOLD);
  } else {
    attroff(A_BOLD);
  }
}
