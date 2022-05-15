#include <core/regex.h>
#include <core/text-buffer.h>
#include <core/text.h>
#include <stdio.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <curses.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "cursor.h"
#include "document.h"
#include "keybindings.h"
#include "menu.h"
#include "textmate.h"
#include "theme.h"
#include "utf8.h"

#include "editor.h"
#include "input.h"
#include "ui.h"
#include "view.h"

// symbols
const wchar_t *symbol_tab = L"\u2847";

// theme
bool use_system_colors = true;
int fg = 0;
int bg = 0;
int hl = 0;
int sel = 0;
int cmt = 0;
int width = 0;
int height = 0;

#define TIMER_BEGIN                                                            \
  clock_t start, end;                                                          \
  double cpu_time_used;                                                        \
  start = clock();

#define TIMER_RESET start = clock();

#define TIMER_END                                                              \
  end = clock();                                                               \
  cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

#define SELECTED_OFFSET 500
#define HIGHLIGHT_OFFSET 1000

enum color_pair_e { NORMAL = 0, SELECTED, COMMENT };
static std::map<int, int> colorMap;

void delay(int ms) {
  struct timespec waittime;
  waittime.tv_sec = (ms / 1000);
  ms = ms % 1000;
  waittime.tv_nsec = ms * 1000 * 1000;
  nanosleep(&waittime, NULL);
}

int color_index(int r, int g, int b) {
  return color_info_t::nearest_color_index(r, g, b);
}

int pair_for_color(int colorIdx, bool selected = false,
                   bool highlighted = false) {
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
  hl = color_index(info.sel_r / 1.5, info.sel_g / 1.5, info.sel_b / 1.5);

  theme_ptr theme = Textmate::theme();

  //---------------
  // build the color pairs
  //---------------
  init_pair(color_pair_e::NORMAL, fg, bg);
  init_pair(color_pair_e::SELECTED, fg, sel);

  theme->colorIndices[fg] = color_info_t({0, 0, 0, fg});
  theme->colorIndices[cmt] = color_info_t({0, 0, 0, cmt});

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

void draw_text(rect_t rect, const char *text, int align = 1, int margin = 0,
               int color = -1) {
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

void draw_text(view_ptr view, const char *text, int align = 1, int margin = 0,
               int color = -1) {
  if (!view->show)
    return;
  draw_text(view->computed, text, align, margin);
}

void draw_gutter_line(editor_ptr editor, view_ptr view, int screen_row, int row,
                      const char *text, std::vector<textstyle_t> &styles) {
  screen_row += view->computed.y;
  int screen_col = view->computed.x;
  int l = strlen(text);

  DocumentPtr doc = editor->doc;
  doc->cursor(); // ensure 1 cursor
  std::vector<Cursor> &cursors = doc->cursors;

  bool is_cursor_row = row == doc->cursor().start.row;
  int pair = pair_for_color(is_cursor_row ? fg : cmt, is_cursor_row, false);
  attron(COLOR_PAIR(pair));
  move(screen_row, screen_col);
  draw_clear(view->computed.w);
  move(screen_row, screen_col + view->computed.w - l - 1);
  addstr(text);
  attroff(COLOR_PAIR(pair));
}

void draw_gutter(editor_ptr editor, view_ptr view) {
  if (!view->show || !editor->show)
    return;

  int scroll_y = editor->scroll.y;
  DocumentPtr doc = editor->doc;

  int vh = view->computed.h;
  int view_start = scroll_y;
  int view_end = scroll_y + vh;

  // highlight
  int idx = 0;
  int start = scroll_y - vh / 2;
  if (start < 0)
    start = 0;

  // inefficient clear
  idx = 0;
  while (idx < editor->computed.h) {
    move(idx++, view->computed.x);
    draw_clear(view->computed.w);
  }

  idx = 0;
  for (int i = 0; i < vh * 2; i++) {
    if (idx > editor->computed.h)
      break;

    int line = start + i;
    int computed_line = doc->computed_line(line);
    BlockPtr block = doc->block_at(computed_line);
    if (!block) {
      move(idx++, view->computed.x);
      draw_clear(view->computed.w);
      continue;
    }

    if (line >= view_start && line < view_end) {
      std::stringstream s;
      s << (computed_line + 1);
      draw_gutter_line(editor, view, idx, computed_line, s.str().c_str(),
                       block->styles);
      idx++;
      for (int j = 1; j < block->line_height; j++) {
        idx++;
        //   move(idx++, view->computed.x);
        //   draw_clear(view->computed.w);
      }
    }
  }
}

void draw_text_line(editor_ptr editor, int screen_row, int row,
                    const char *text, BlockPtr block, int *height = 0) {
  std::vector<textstyle_t> &styles = block->styles;

  int scroll_x = editor->scroll.x;
  int scroll_y = editor->scroll.y;

  DocumentPtr doc = editor->doc;
  optional<Cursor> block_cursor = doc->block_cursor(doc->cursor());
  optional<Bracket> bracket_cursor = doc->bracket_cursor(doc->cursor());
  SearchPtr search = doc->search();

  screen_row += editor->computed.y;
  int screen_col = editor->computed.x;
  move(screen_row, screen_col);
  clrtoeol();

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
  for (int i = scroll_x; i < l; i++) {
    int pair = default_pair;
    bool selected = false;

    if (i - scroll_x + 1 > (editor->computed.w * (*height))) {
      if (!editor->wrap) {
        break;
      }
      screen_col = editor->computed.x;
      screen_row++;
      *height = (*height) + 1;
      move(screen_row, screen_col);
      clrtoeol();
    }

    block->line_height = *height;

    // cursor selections
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
            attron(A_REVERSE);
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
    for (auto s : styles) {
      if (s.start >= i && i < s.start + s.length) {
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
    if (bracket_cursor) {
      Range r = (*bracket_cursor).range;
      if (r.start.column == i && r.start.row == row) {
        underline = true;
      }
    }

    if (underline) {
      attron(A_UNDERLINE);
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

  char spacer = ' ';
  int pair = default_pair;
  if (fold_size) {
    pair = pair_for_color(cmt, false, true);
    std::stringstream ss;
    // ss << "+ ";
    // ss << fold_size;
    // ss << " lines ";
    attron(COLOR_PAIR(pair));
    // addwstr(L"\u00b1");
    addstr(ss.str().c_str());
    attroff(COLOR_PAIR(pair));
    l += ss.str().size();
    spacer = '-';
  }

  if (is_cursor_row || fold_size) {
    move(screen_row, screen_col + l);
    for (int i = 0; i < editor->computed.w - l - scroll_x; i++) {
      attron(COLOR_PAIR(pair));
      addch(spacer);
      attroff(COLOR_PAIR(pair));
    }
  }
}

bool compare_brackets(Bracket a, Bracket b) {
  return compare_range(a.range, b.range);
}

void draw_text_buffer(editor_ptr editor) {
  if (!editor->show)
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
  int start = scroll_y - vh / 2;
  if (start < 0)
    start = 0;

  for (int i = 0; i < vh * 2; i++) {
    if (idx + offset_y > editor->computed.h)
      break;

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
      if (block->dirty) {
        if (doc->language && !doc->language->definition.isNull()) {
          block->styles = Textmate::run_highlighter(
              (char *)s.str().c_str(), doc->language, Textmate::theme(),
              block.get(), doc->previous_block(block).get(),
              doc->next_block(block).get());

          // find brackets
          block->brackets.clear();
          for (auto s : block->styles) {
            if (s.flags & SCOPE_BRACKET) {
              block->brackets.push_back(
                  Bracket{{{computed_line, s.start},
                           {computed_line, s.start + s.length}},
                          s.flags});
            }
          }

          // std::sort(block->brackets.begin(), block->brackets.end(),
          // compare_brackets);
        }
        block->dirty = false;
      }

      int line_height = 1;
      draw_text_line(editor, (idx++) + offset_y, computed_line, s.str().c_str(),
                     block, &line_height);
      if (line_height > 1) {
        offset_y += (line_height - 1);
      }
    }
  }

  for (int i = idx + offset_y; i < editor->computed.h; i++) {
    move(editor->computed.y + i, editor->computed.x);
    // addch('~')
    clrtoeol();
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

void draw_menu(MenuPtr menu, editor_ptr editor) {
  if (!menu->show || !menu->items.size())
    return;

  int row = 0;
  int def = pair_for_color(cmt, false, true);
  int sel = pair_for_color(fg, true, false);

  int margin = 1;
  int w = menu->frame.w;
  int h = menu->frame.h;
  int start = menu->scroll.y;
  int screen_row = menu->frame.y;
  int screen_col = menu->frame.x;
  int offset_row = 0;

  for (int i = start; i < menu->items.size(); i++) {
    auto m = menu->items[i];
    int pair = menu->cursor.y == i ? sel : def;
    int screen_row = editor->cursor.y + 1 + row++;
    std::string text = m.name.substr(0, w - 1);
    attron(COLOR_PAIR(pair));
    move(screen_row + offset_row, screen_col);
    draw_clear(w);
    move(screen_row + offset_row, screen_col + margin);
    addstr(text.c_str());
    attroff(COLOR_PAIR(pair));
    if (row > h)
      break;
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
    attron(COLOR_PAIR(def));
    move(view->computed.y + row++, view->computed.x);
    addstr(ss.str().substr(0, view->computed.w).c_str());
    attroff(COLOR_PAIR(def));
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
    attron(COLOR_PAIR(pair));
    move(view->computed.y + row++, view->computed.x);
    addstr(ss.str().substr(0, view->computed.w).c_str());
    attroff(COLOR_PAIR(pair));
  }

  for (auto f : doc->folds) {
    std::stringstream ss;
    ss << "fold: ";
    ss << f.start.row;
    ss << ",";
    ss << f.start.column;
    ss << "-";
    ss << f.end.row;
    ss << ",";
    ss << f.end.column;
    int pair = def;
    attron(COLOR_PAIR(pair));
    move(view->computed.y + row++, view->computed.x);
    addstr(ss.str().substr(0, view->computed.w).c_str());
    attroff(COLOR_PAIR(pair));
  }
}

bool get_dimensions() {
  static struct winsize ws;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  int _width = ws.ws_col;
  int _height = ws.ws_row;
  if (_width != width || _height != height) {
    width = _width;
    height = _height;
    return true;
  }
  return false;
}

void layout(view_ptr root) {
  clear();
  int margin = 0;
  root->layout(
      rect_t{margin, margin, width - (margin * 2), height - (margin * 2)});
  root->finalize();
}

int main(int argc, char **argv) {
  std::string file_path = "./tests/main.cpp";
  if (argc > 1) {
    file_path = argv[argc - 1];
  }

  const char *defaultTheme = "Monokai";
  const char *argTheme = defaultTheme;
  for (int i = 0; i < argc - 1; i++) {
    if (strcmp(argv[i], "-t") == 0) {
      argTheme = argv[i + 1];
    }
  }
  Textmate::initialize("/home/iceman/.editor/extensions/");
  Textmate::load_theme(argTheme);
  theme_info_t info = Textmate::theme_info();

  MenuPtr menu = std::make_shared<menu_t>();

  editors_t editors;
  editor_ptr editor = editors.add_editor(file_path);
  // editors.add_editor("./src/document.cpp");

  view_ptr root = std::make_shared<column_t>();
  view_ptr main = std::make_shared<row_t>();
  status_bar_ptr status = std::make_shared<status_bar_t>();
  // view_ptr status = std::make_shared<row_t>();
  // view_ptr status_message = std::make_shared<row_t>();
  // view_ptr status_line_col = std::make_shared<row_t>();
  // status->add_child(status_message);
  // status->add_child(status_line_col);
  // status_message->flex = 3;
  // status_line_col->flex = 2;
  root->add_child(main);
  root->add_child(status);
  view_ptr gutter = std::make_shared<view_t>();
  main->add_child(gutter);

  view_ptr editor_views = std::make_shared<view_t>();
  editor_views->flex = 3;
  for (auto e : editors.editors) {
    e->flex = 1;
    editor_views->add_child(e);
  }
  main->add_child(editor_views);

  view_ptr sitter = std::make_shared<view_t>();
  main->add_child(sitter);

  main->flex = 1;
  gutter->frame.w = 8;
  sitter->flex = 1;
  sitter->show = false;

  goto_ptr gotoline = std::make_shared<goto_t>();
  find_ptr find = std::make_shared<find_t>();
  root->add_child(gotoline);
  root->add_child(find);

  std::vector<editor_ptr> input_popups = {gotoline->input, find->input,
                                          find->replace};

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
  update_colors();

  curs_set(0);
  clear();

  get_dimensions();

  std::stringstream message;
  message << "Welcome to text-edit";

  int warm_start = 3;
  int last_layout_size = -1;
  std::string last_key_sequence;

  view_t::input_focus = editor;

  editor_ptr prev;
  bool running = true;
  while (running) {
    if (prev != editors.current_editor()) {
      editor = editors.current_editor();
      layout(root);
    }
    prev = editor;
    DocumentPtr doc = editor->doc;
    TextBuffer &text = doc->buffer;
    int size = doc->size();

    bool has_input = false;
    for (auto i : input_popups) {
      bool has_children_focus = i->parent->has_children_focus();
      if (has_children_focus != i->parent->show) {
        i->parent->show = has_children_focus;
        last_layout_size = -1;
      }
      has_input = has_input | i->has_focus();
    }
    // status->show = !has_input;

    if (last_layout_size != size) {
      std::stringstream s;
      s << "  ";
      s << size;
      gutter->frame.w = s.str().size();
      layout(root);
      last_layout_size = size;
    }

    for (auto e : editors.editors) {
      draw_text_buffer(e);
      draw_gutter(e, gutter);
    }

    SearchPtr search = doc->search();
    if (find->show && search) {
      if (search) {
        message.str("");
        message << " ";
        if (search->matches.size()) {
          message << (search->selected + 1);
          message << " of ";
        }
        message << search->matches.size();
        message << " found";
      }
    }

    for (auto input : input_popups) {
      if (!input->parent->show) {
        continue;
      }
      draw_text_buffer(input);
      draw_status((status_line_t *)(input->parent));
    }

    Cursor cursor = doc->cursor();

    // status

    if (status->show) {

      std::stringstream ss;
      if (doc->insert_mode) {
        ss << "INS   ";
      } else {
        ss << "OVR   ";
      }
      ss << doc->language->id;
      ss << "  ";
      // ss << " line ";
      ss << (cursor.start.row + 1);
      // ss << " col ";
      ss << ",";
      ss << (cursor.start.column + 1);
      ss << "  ";
      // ss << doc->buffer.extent().column;

      if (size > 0) {
        int p = (100 * cursor.start.row) / size;
        if (p == 0) {
          ss << "top";
        } else if (p == 100) {
          ss << "end";
        } else {
          ss << p;
          ss << "%";
        }
      }

      status->items["message"]->text = message.str();
      status->items["info"]->text = ss.str();
      status->items["message"]->color = pair_for_color(cmt, false, false);
      status->items["info"]->color = pair_for_color(cmt, false, false);
      draw_status(status.get());
    }

    menu->show = false;
    menu->items.clear();

    AutoCompletePtr autocomplete = doc->autocomplete();
    if (!menu->show && autocomplete) {
      int margin = 1;
      int w = 0;
      for (auto m : autocomplete->matches) {
        if (w < m.string.size()) {
          w = m.string.size();
        }
        menu->items.push_back(menu_item_t{u16string_to_string(m.string)});
      }
      w += margin * 2;
      menu->frame.w = w;
      menu->frame.h = menu->items.size();
      if (menu->frame.h > 10) {
        menu->frame.h = 10;
      }
      int h = menu->frame.h;

      int offset_row = 0;
      int screen_row = editor->cursor.y;
      int screen_col = editor->cursor.x;

      optional<Range> sub = doc->subsequence_range();
      if (sub) {
        screen_col -= ((*sub).end.column - (*sub).start.column);
      }
      if (editor->cursor.x + w > width) {
        screen_col = editor->cursor.x - w;
      }
      if (editor->cursor.y + 2 + h > height) {
        offset_row = -(h + 1);
      }

      screen_col -= margin;
      menu->frame.x = screen_col;
      menu->frame.y = screen_row;
      menu->scroll.x = 0;
      menu->update_scroll();

      menu->on_submit = [&doc](int selected) {
        AutoCompletePtr autocomplete = doc->autocomplete();
        optional<Range> range = doc->subsequence_range();
        if (range) {
          std::u16string value = autocomplete->matches[selected].string;
          doc->clear_cursors();
          doc->cursor().copy_from(Cursor{(*range).start, (*range).end});
          doc->insert_text(value);
        }
        doc->clear_autocomplete(true);
        return true;
      };

      menu->on_cancel = [&doc]() {
        doc->clear_autocomplete(true);
        return true;
      };

      menu->show = (menu->items.size() > 0);
    }

    draw_menu(menu, editor);

    TreeSitterPtr treesitter = doc->treesitter();
    if (treesitter) {
      draw_tree_sitter(editor, sitter, treesitter, doc->cursor());
    }

    // blit
    move(view_t::input_focus->cursor.y, view_t::input_focus->cursor.x);
    curs_set(1);
    refresh();

    // input
    int ch = -1;
    std::string key_sequence;
    int frames = 0;
    int idle = 0;
    while (running) {
      ch = readKey(key_sequence);
      if (ch != -1) {
        break;
      }
      frames++;

      // background tasks
      if (editor->on_idle(frames)) {
        break;
      }

      // check dimensions
      if (frames == 1800 && get_dimensions()) {
        layout(root);
        editor->doc->make_dirty();
        break;
      }

      // loop
      if (frames > 2000) {
        frames = 0;
        idle++;
        if (Textmate::has_running_threads() || (warm_start-- > 0)) {
          editor->doc->make_dirty();
          break;
        }
      }

      if (idle > 8) {
        delay(50);
        if (idle > 32) {
          delay(150);
          curs_set(0);
        }
      }
    }

    curs_set(0);
    if (ch == 27) {
      key_sequence = "escape";
      ch = -1;
    }

    // todo .. move somewhere?
    if (find->show) {
      if (key_sequence == "tab" && find->enable_replace) {
        if (view_t::input_focus != find->replace) {
          view_t::input_focus = find->replace;
        } else {
          view_t::input_focus = find->input;
        }
        continue;
      }
    }
    if (search) {
      if (key_sequence == "up") {
        if (search->selected > 0) {
          search->selected--;
        }
      }
      if (key_sequence == "down") {
        if (search->selected + 1 < search->matches.size()) {
          search->selected++;
        }
      }
      Range range = search->matches[search->selected];
      Cursor &cursor = doc->cursor();
      if (range != cursor) {
        cursor.start = range.start;
        cursor.end = range.end;
        editor->on_input(-1, ""); // ping
        continue;
      }
    }

    if (menu->show) {
      if (menu->on_input(ch, key_sequence)) {
        continue;
      }
    }
    if (view_t::input_focus->on_input(ch, key_sequence)) {
      continue;
    }

    // globl commands
    Command &cmd = command_from_keys(key_sequence, last_key_sequence);
    if (cmd.command == "await") {
      last_key_sequence = key_sequence;
      continue;
    }
    last_key_sequence = "";

    if (key_sequence != "") {
      message.str("");
      message << "[";
      if (cmd.command != "") {
        message << cmd.command;
        if (cmd.params != "") {
          message << ":";
          message << cmd.params;
        }
      } else {
        message << key_sequence;
      }
      message << "]";
      // message << " ";
      // message << doc->folds.size();
    }

    if (cmd.command == "switch_tab") {
      int idx = std::stoi(cmd.params);
      if (idx < editors.editors.size()) {
        editors.selected = idx;
        view_t::input_focus = editors.current_editor();
      }
    }

    // todo move to view
    if (cmd.command == "search_text" || cmd.command == "search_and_replace") {
      view_t::input_focus = find->input;
      find->enable_replace = cmd.command == "search_and_replace";
      find->input->cursor = {0, 0};
      find->input->on_submit = [&editor, &find](std::u16string value) {
        editor->doc->run_search(value, editor->doc->cursor().start);
        return true;
      };
      find->replace->cursor = {0, 0};
      find->replace->on_submit = [&editor, &search,
                                  &find](std::u16string value) {
        if (!search || search->matches.size() <= 0) {
          find->input->on_input(-1, "enter");
          return true;
        }
        if (search->selected >= 0) {
          editor->doc->insert_text(value);
          search->matches.erase(search->matches.begin() + search->selected);
        }
        if (search->selected >= search->matches.size()) {
          search->selected = search->matches.size() - 1;
        }
        return true;
      };
      find->replace_title->show = find->enable_replace;
      find->replace->show = find->enable_replace;
      if (find->enable_replace && search && search->matches.size()) {
        view_t::input_focus = find->replace;
      }
    }
    if (cmd.command == "jump_to_line") {
      view_t::input_focus = gotoline->input;
      gotoline->input->cursor = {0, 0};
      gotoline->input->on_submit = [&editor, &message](std::u16string value) {
        view_t::input_focus = editor;
        try {
          int line = std::stoi(u16string_to_string(value)) - 1;
          editor->doc->go_to_line(line);
          editor->on_input(-1, ""); // ping
        } catch (std::exception &e) {
          message.str(e.what());
        }
        return true;
      };
    }

    if (cmd.command == "cancel") {
      if (view_t::input_focus != editor) {
        view_t::input_focus = editor;
        editor->doc->clear_search(true);
      }
    }
    if (cmd.command == "toggle_tree_sitter") {
      sitter->show = !sitter->show;
      layout(root);
      editor->doc->make_dirty();
    }
    if (cmd.command == "toggle_gutter") {
      gutter->show = !gutter->show;
      layout(root);
      editor->doc->make_dirty();
    }
    if (cmd.command == "toggle_statusbar") {
      status->show = !status->show;
      layout(root);
      editor->doc->make_dirty();
    }
    if (cmd.command == "quit") {
      running = false;
    }
    if (cmd.command == "close") {
      running = false;
    }
  }

  endwin();

  // shutting down...
  int idx = 20;
  while (Textmate::has_running_threads() && idx-- > 0) {
    delay(50);
  }
  return 0;
}
