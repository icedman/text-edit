#include "render.h"
#include "textmate.h"
#include "utf8.h"
#include "util.h"

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
#define MAX_LINES_HIGHLIGHT_RUN 12

#define RENDER_BACK_PAGES 1
#define RENDER_AHEAD_PAGES (RENDER_BACK_PAGES + 4)

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

void init_renderer() {
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

void shutdown_renderer() { endwin(); }

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

void draw_gutter_line(editor_ptr editor, view_ptr view, int screen_row, int row,
                      const char *text, std::vector<textstyle_t> &styles,
                      int height) {
  screen_row += view->computed.y;
  int screen_col = view->computed.x;
  int l = strlen(text);

  if (screen_row >= view->computed.h) {
    return;
  }

  DocumentPtr doc = editor->doc;
  doc->cursor(); // ensure 1 cursor
  std::vector<Cursor> &cursors = doc->cursors;

  bool is_cursor_row = row == doc->cursor().start.row;

  for (int i = 0; i < height; i++) {
    if (screen_row + i > view->computed.h) {
      break;
    }

    int pair = pair_for_color(is_cursor_row && i == 0 ? fg : cmt,
                              is_cursor_row && i == 0, false);
    _attron(_COLOR_PAIR(pair));
    _move(screen_row + i, screen_col);
    draw_clear(view->computed.w);
    _attroff(_COLOR_PAIR(pair));
  }

  int pair = pair_for_color(is_cursor_row ? fg : cmt, is_cursor_row, false);
  _attron(_COLOR_PAIR(pair));
  _move(screen_row, screen_col);
  draw_clear(view->computed.w);
  _move(screen_row, screen_col + view->computed.w - l - 1);
  _addstr(text);
  _attroff(_COLOR_PAIR(pair));
}

void draw_gutter(editor_ptr editor, view_ptr view) {
  if (!editor->show || !view->show)
    return;

  DocumentPtr doc = editor->doc;
  TextBuffer &text = doc->buffer;

  int scroll_x = editor->scroll.x;
  int scroll_y = editor->scroll.y;
  int vh = editor->computed.h;
  int view_start = scroll_y;
  int view_end = scroll_y + vh;

  int offset_y = 0;

  // highlight
  int idx = 0;
  int start = scroll_y - (vh * 2);
  if (start < 0)
    start = 0;

  for (int i = 0; i < (vh * 3); i++) {
    if (idx + offset_y > editor->computed.h)
      break;

    int line = start + i;
    int computed_line = doc->computed_line(line);

    BlockPtr block = doc->block_at(computed_line);
    if (!block) {
      // ERROR!
      break;
    }

    if (line >= view_start && line < view_end) {
      std::stringstream s;
      s << (computed_line + 1);
      draw_gutter_line(editor, view, (idx++) + offset_y, computed_line,
                       s.str().c_str(), block->styles, block->line_height);

      // todo.. bug
      if (block->line_height > 1) {
        offset_y += (block->line_height - 1);
        for (int j = 0; j < block->line_height - 1; j++) {
          _move(offset_y + idx + j, view->computed.x);
          draw_clear(view->computed.w);
        }
      }
    }
  }
}

void draw_tree_sitter(editor_ptr editor, view_ptr view,
                      TreeSitterPtr treesitter, Cursor cursor) {
  if (!view->show)
    return;
  std::vector<TSNode> nodes =
      treesitter->walk(cursor.start.row, cursor.start.column);

  DocumentPtr doc = editor->doc;

  int def = pair_for_color(cmt, false, false);
  int sel = pair_for_color(fg, false, true);

  int row = 0;
  optional<Cursor> block_cursor = doc->block_cursor(doc->cursor());
  if (block_cursor) {
    std::stringstream ss;
    ss << "[";
    ss << (*block_cursor).start.row;
    ss << ",";
    ss << (*block_cursor).start.column;
    ss << "-";
    ss << (*block_cursor).end.row;
    ss << ",";
    ss << (*block_cursor).end.column;
    ss << "]";
    _attron(_COLOR_PAIR(def));
    _move(view->computed.y + row++, view->computed.x);
    _addstr(ss.str().substr(0, view->computed.w).c_str());
    _attroff(_COLOR_PAIR(def));
  }

  for (auto node : nodes) {
    const char *type = ts_node_type(node);
    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);

    std::stringstream ss;
    ss << start.row;
    ss << ",";
    ss << start.column;
    ss << "-";
    ss << end.row;
    ss << ",";
    ss << end.column;
    ss << " ";
    ss << type;

    int pair = def;
    Cursor cur = cursor.copy();
    cur.start = {start.row, start.column};
    cur.end = {end.row, end.column};
    if (cur.start.row == cursor.start.row &&
        cur.is_within(cursor.start.row, cursor.start.column)) {
      pair = sel;
    }
    _attron(_COLOR_PAIR(pair));
    _move(view->computed.y + row++, view->computed.x);
    _addstr(ss.str().substr(0, view->computed.w).c_str());
    _attroff(_COLOR_PAIR(pair));
  }

  // for (auto f : doc->folds) {
  //

  BlockPtr block = doc->block_at(cursor.start.row);
  if (block) {
    _move(view->computed.y + row++, view->computed.x);
    for (auto s : block->span_infos) {
      if (cursor.start.column >= s.start &&
          cursor.start.column < s.start + s.length) {
        std::stringstream ss;
        ss << s.start;
        ss << ",";
        ss << (s.start + s.length);
        ss << " ";
        ss << s.scope;
        _addstr(ss.str().substr(0, view->computed.w).c_str());
        break;
      }
    }
  }

  std::stringstream ss;
  //   ss << "fold: ";
  //   ss << f.start.row;
  //   ss << ",";
  //   ss << f.start.column;
  //   ss << "-";
  //   ss << f.end.row;
  //   ss << ",";
  //   ss << f.end.column;
  //   int pair = def;
  //   _attron(_COLOR_PAIR(pair));
  //   _move(view->computed.y + row++, view->computed.x);
  //   _addstr(ss.str().substr(0, view->computed.w).c_str());
  //   _attroff(_COLOR_PAIR(pair));
  // }
  if (treesitter->reference_ready) {
    _move(view->computed.y + row++, view->computed.x);
    _addstr("reference ready");
    for (auto i : treesitter->identifiers) {
      int pair = def;
      _attron(_COLOR_PAIR(pair));
      _move(view->computed.y + row++, view->computed.x);
      _addstr(i.substr(0, view->computed.w).c_str());
      _attroff(_COLOR_PAIR(pair));
      if (view->computed.y + row > view->computed.h)
        break;
    }
  }
}

void draw_tabs(menu_ptr view, editors_t &editors) {
  if (!view->show)
    return;
  _move(view->computed.y, view->computed.x); // not accurate
  _clrtoeol();

  int def = pair_for_color(cmt, false, false);
  int sel =
      pair_for_color(view->has_focus() ? fn : cmt, false, view->has_focus());
  int sel_border = pair_for_color(kw, false, view->has_focus());

  char brackets[] = "[]";
  char space[] = "  ";
  char *borders = space;

  std::string t;

  int x1 = 0;
  int x2 = 0;
  int sx1 = 0;
  int sx2 = 0;
  view->items.clear();
  for (auto e : editors.editors) {
    borders = space;
    if (e == editors.current_editor()) {
      borders = brackets;
    }

    if (e->doc->name == "") {
      e->doc->name = "untitled";
    }

    std::string n;
    n = borders[0];
    n += e->doc->name;
    n += borders[1];
    x1 = x2;
    x2 += n.size();

    if (e == editors.current_editor()) {
      view->selected = view->items.size();
      sx1 = x1;
      sx2 = x2;
    }
    view->items.push_back({n, e->doc->name});
    t += n;
  }

  int s = 0;
  if (t.size() > view->computed.w) {
    s = sx1;
    if (t.size() - s < view->computed.w) {
      s = t.size() - view->computed.w;
    }
  }

  t = t.substr(s, view->computed.w);

  int center = 0;
  if (view->computed.w > t.size()) {
    center = (view->computed.w / 2) - (t.size() / 2);
    _move(view->computed.y, view->computed.x + center);
  }

  _attron(_COLOR_PAIR(def));
  _addstr(t.c_str());
  _attroff(_COLOR_PAIR(def));

  // selected
  _move(view->computed.y, view->computed.x + sx1 - s + center);
  _attron(_COLOR_PAIR(sel_border));
  _addch(brackets[0]);
  _attroff(_COLOR_PAIR(sel_border));
  _attron(_COLOR_PAIR(sel));
  _addstr(view->items[view->selected].value.c_str());
  _attroff(_COLOR_PAIR(sel));
  _attron(_COLOR_PAIR(sel_border));
  _addch(brackets[1]);
  _attroff(_COLOR_PAIR(sel_border));

  view->cursor = {view->computed.x + sx1 - s + center, view->computed.y};
}

void draw_text_line(editor_ptr editor, int screen_row, int row,
                    const char *text, BlockPtr block, int *height) {
  std::vector<textstyle_t> &styles = block->styles;

  // for (auto style : styles) {
  //   std::string str(text + style.start, style.length);
  //   block->style_cache[str] = style;
  // }

  int scroll_x = editor->scroll.x;
  int scroll_y = editor->scroll.y;

  DocumentPtr doc = editor->doc;
  optional<Cursor> block_cursor = doc->block_cursor(doc->cursor());
  // optional<Bracket> bracket_cursor = doc->bracket_cursor(doc->cursor());
  SearchPtr search = doc->search();
  TreeSitterPtr treesitter = doc->treesitter();

  screen_row += editor->computed.y;
  int screen_col = editor->computed.x;
  _move(screen_row, screen_col);
  _clrtoeol();

  int l = strlen(text);
  block->line_length = l;
  int indent_size = count_indent_size(text);
  int tab_size = editor->draw_tab_stops ? doc->tab_string.size() : 0;

  doc->cursor(); // ensure 1 cursor
  std::vector<Cursor> &cursors = doc->cursors;

  int edge = pair_for_color(fg, true, false);
  *height = 1;

  int fold_size = 0;
  for (auto f : doc->folds) {
    if (f.start.row == row) {
      fold_size = f.end.row - f.start.row;
      break;
    }
  }

  bool is_cursor_row =
      (editor->has_focus() && (row == doc->cursor().start.row || fold_size));
  int default_pair = pair_for_color(fg, false, is_cursor_row);
  int pair = default_pair;

  for (int i = scroll_x; i < l; i++) {
    pair = default_pair;

    if (i - scroll_x + 1 > (editor->computed.w * (*height))) {
      if (!editor->wrap) {
        break;
      }
      screen_col = editor->computed.x;
      screen_row++;
      *height = (*height) + 1;
      _move(screen_row, screen_col);
      _clrtoeol();
    }

    block->line_height = *height;

    // cursor selections
    bool selected = false;
    for (auto cursor : cursors) {
      if (cursor.is_within(row, i)) {
        selected = cursors.size() > 1 || cursor.has_selection();
        if (cursor.has_selection() && cursor.is_normalized()) {
          if (row == cursor.end.row && i == cursor.end.column) {
            selected = false;
          }
        }
        if (row == cursor.start.row && i == cursor.start.column) {
          editor->cursor.x = screen_col + i - scroll_x -
                             (editor->computed.w * ((*height) - 1));
          editor->cursor.y = screen_row;
          if (cursor.has_selection()) {
            if (!cursor.is_normalized()) {
              selected = false;
            }
            _reverse(true);
          }
        }
        break;
      }
    }

    if (selected) {
      pair = pair_for_color(fg, selected, false);
    }

    char ch = text[i];
    bool underline = false;

    // syntax highlights
    // for (auto s : styles) {
    int idx = 0;
    auto it = styles.rbegin();
    while (it != styles.rend()) {
      auto s = *it++;
      idx++;
      if (s.start <= i && i < s.start + s.length) {
        underline = s.underline;
        int colorIdx = color_index(s.r, s.g, s.b);
        pair = pair_for_color(colorIdx, selected, is_cursor_row);
        pair = pair > 0 ? pair : default_pair;
        break;
      }
    }

    // decorate block edges
    if (block_cursor) {
      if ((*block_cursor).is_edge(row, i)) {
        pair = edge;
      }
    }

    // decorate search result
    if (search) {
      for (auto m : search->matches) {
        m.end.column--;
        if (is_point_within_range({row, i}, m)) {
          underline = true;
          break;
        }
      }
    }

    // decorate brackets
    // if (bracket_cursor) {
    //   Range r = (*bracket_cursor).range;
    //   if (r.start.column == i && r.start.row == row) {
    //     underline = true;
    //   }
    // }

    if (underline) {
      _underline(true);
    }

    // render symbols
    wchar_t *symbol = NULL;

    // tab stop
    if (tab_size > 0 && (i % tab_size == 0) && i < indent_size) {
      pair = pair_for_color(cmt, selected, is_cursor_row);
      symbol = (wchar_t *)symbol_tab;
      ch = '|';
    }

    // render the character
    _attron(_COLOR_PAIR(pair));
    if (symbol != NULL) {
      _addwstr(symbol);
    } else {
      _addch(ch);
    }
    _attroff(_COLOR_PAIR(pair));

    // _attroff(A_BLINK);
    // _attroff(A_STANDOUT);
    _reverse(false);
    _bold(false);
    _underline(false);
  }

  char spacer = ' ';
  if (fold_size) {
    pair = pair_for_color(cmt, false, true);
    std::stringstream ss;
    // ss << "+ ";
    // ss << fold_size;
    // ss << " lines ";
    _attron(_COLOR_PAIR(pair));
    // addwstr(L"\u00b1");
    _addstr(ss.str().c_str());
    _attroff(_COLOR_PAIR(pair));
    l += ss.str().size();
    spacer = '-';
  }

  if (is_cursor_row || fold_size) {
    _move(screen_row, screen_col + l);
    for (int i = 0; i < editor->computed.w - l - scroll_x; i++) {
      _attron(_COLOR_PAIR(pair));
      _addch(spacer);
      _attroff(_COLOR_PAIR(pair));
    }
  }
}

// std::vector<textstyle_t> build_style_from_cache(BlockPtr block,
//                                                 const char *text) {
//   std::vector<textstyle_t> res;

//   std::string t = text;
//   int lastIdx = 0;
//   for (auto m : block->style_cache) {
//     std::string key = m.first;
//     std::string::size_type idx = t.find(key, 0);
//     if (idx != std::string::npos) {
//       lastIdx = idx + 1;
//       textstyle_t s = m.second;
//       s.start = idx;
//       s.length = key.size();
//       // printf("%s\n", key.c_str());
//       res.push_back(s);
//     }
//   }

//   return res;
// }

bool compare_brackets(Bracket a, Bracket b) {
  return compare_range(a.range, b.range);
}

void draw_text_buffer(editor_ptr editor) {
  if (!editor->show)
    return;

  DocumentPtr doc = editor->doc;
  TextBuffer &text = doc->buffer;
  SearchPtr search = doc->search();
  // optional<Bracket> bracket_cursor = doc->bracket_cursor(doc->cursor());
  optional<Cursor> block_cursor = doc->block_cursor(doc->cursor());

  int scroll_x = editor->scroll.x;
  int scroll_y = editor->scroll.y;
  int vh = editor->computed.h;
  int view_start = scroll_y;
  int view_end = scroll_y + (vh * RENDER_AHEAD_PAGES);

  int offset_y = 0;

  // highlight
  int idx = 0;
  int start = scroll_y - RENDER_BACK_PAGES;
  if (start < 0)
    start = 0;

  int dirty_count = 0;
  editor->request_highlight = false;

  bool skip_rendering = false;
  for (int i = 0; i < (vh * RENDER_AHEAD_PAGES); i++) {
    if (idx + offset_y > editor->computed.h) {
      skip_rendering = true;
    }

    int line = start + i;
    int computed_line = doc->computed_line(line);

    BlockPtr block = doc->block_at(computed_line);
    if (!block) {
      // ERROR!
      break;
    }

    optional<std::u16string> row = text.line_for_row(computed_line);

    // expensive?
    std::stringstream s;
    if (row) {
      s << u16string_to_string(*row);
      s << " ";
    }

    if (line >= view_start && line < view_end) {

      if (block->dirty && dirty_count != -1) {
        dirty_count ++;
        // log("%d / %d", dirty_count, vh);
        if (dirty_count > MAX_LINES_HIGHLIGHT_RUN) {
          dirty_count = -1;
          // log("defer highlight");
        }
      }
        
      if (block->dirty && dirty_count != -1) {
        if (doc->language && !doc->language->definition.isNull()) {

          // if (block->styles.size() == 0) {

          // log("hl %d", line);
          block->styles = Textmate::run_highlighter(
              (char *)s.str().c_str(), doc->language, Textmate::theme(),
              block.get(), doc->previous_block(block).get(),
              doc->next_block(block).get(), NULL);

          //   block->style_cache.clear();
          // } else if (block->style_cache.size() > 2) {
          //   block->styles = build_style_from_cache(block, (char
          //   *)s.str().c_str());
          // }

          //&block->span_infos);

          // find brackets
          // block->brackets.clear();
          // for (auto s : block->styles) {
          //   if (s.flags & SCOPE_BRACKET) {
          //     block->brackets.push_back(
          //         Bracket{{{computed_line, s.start},
          //                  {computed_line, s.start + s.length}},
          //                 s.flags});
          //   }
          // }

          // std::sort(block->brackets.begin(), block->brackets.end(),
          // compare_brackets);
        }
        block->dirty = false;
      }

      if (skip_rendering) {
        continue;
      }

      int line_height = 1;

      draw_text_line(editor, (idx++) + offset_y, computed_line, s.str().c_str(),
                     block, &line_height);

      block->line_height = line_height;

      if (line_height > 1) {
        offset_y += (line_height - 1);
      }
    }
  }

  editor->request_highlight = dirty_count == -1;

  for (int i = idx + offset_y; i < editor->computed.h; i++) {
    _move(editor->computed.y + i, editor->computed.x);
    // _addch('~')
    _clrtoeol();
  }
}

void _move(int x, int y) { move(x, y); }

void _attron(int attr) { attron(attr); }

void _attroff(int attr) { attroff(attr); }

void _clear() { clear(); }

void _refresh() { refresh(); }

void _addstr(const char *text) { addstr(text); }

void _addwstr(const wchar_t *text) { addwstr(text); }

void _addch(char ch) { addch(ch); }

void _clrtoeol() { clrtoeol(); }

void _curs_set(int i) { curs_set(i); }

int _COLOR_PAIR(int i) { return COLOR_PAIR(i); }

void _underline(bool on) {
  if (on) {
    attron(A_UNDERLINE);
  } else {
    attroff(A_UNDERLINE);
  }
}

void _reverse(bool on) {
  if (on) {
    attron(A_REVERSE);
  } else {
    attroff(A_REVERSE);
  }
}

void _bold(bool on) {
  if (on) {
    attron(A_BOLD);
  } else {
    attroff(A_BOLD);
  }
}
