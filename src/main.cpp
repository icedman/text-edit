#include <core/regex.h>
#include <core/text-buffer.h>
#include <core/text.h>
#include <stdio.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "cursor.h"
#include "document.h"
#include "files.h"
#include "keybindings.h"
#include "menu.h"
#include "textmate.h"
#include "theme.h"
#include "utf8.h"

#include "editor.h"
#include "input.h"
#include "ui.h"
#include "view.h"
#include "util.h"

#include "render.h"

extern "C" {
  #include "libvim.h"
}

// symbols
extern const wchar_t *symbol_tab;

// theme
extern int fg;
extern int bg;
extern int hl;
extern int sel;
extern int cmt;
extern int fn;
extern int kw;
extern int var;

int width = 0;
int height = 0;
int last_layout_hash = -1;

#define TIMER_BEGIN                                                            \
  clock_t start, end;                                                          \
  double cpu_time_used;                                                        \
  start = clock();

#define TIMER_RESET start = clock();

#define TIMER_END                                                              \
  end = clock();                                                               \
  cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

void delay(int ms) {
  struct timespec waittime;
  waittime.tv_sec = (ms / 1000);
  ms = ms % 1000;
  waittime.tv_nsec = ms * 1000 * 1000;
  nanosleep(&waittime, NULL);
}

void draw_gutter_line(editor_ptr editor, view_ptr view, int screen_row, int row,
                      const char *text, std::vector<textstyle_t> &styles, int height) {
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
  
  for(int i=0; i<height; i++) {
    if (screen_row + i > view->computed.h) {
      break;
    }

    int pair = pair_for_color(is_cursor_row && i == 0 ? fg : cmt, is_cursor_row && i == 0, false);
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
  int start = scroll_y - vh / 2;
  if (start < 0)
    start = 0;

  int docSize = doc->size();

  for (int i = 0; i < vh * 2; i++) {
    if (idx + offset_y > editor->computed.h)
      break;

    int line = start + i;
    int computed_line = doc->computed_line(line);

    if (computed_line >= docSize) {
      break;
    }

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

std::vector<textstyle_t> build_style_from_cache(BlockPtr block, const char *text) {
  std::vector<textstyle_t> res;

  std::string t = text;
  int lastIdx = 0;
  for(auto m : block->style_cache) {
    std::string key = m.first;
    std::string::size_type idx = t.find(key, 0);
    if (idx != std::string::npos) {
      lastIdx = idx + 1;
      textstyle_t s = m.second;
      s.start = idx;
      s.length = key.size();
      // printf("%s\n", key.c_str());
      res.push_back(s);
    }
  }

  return res;
}

void draw_text_line(editor_ptr editor, int screen_row, int row,
                    const char *text, BlockPtr block, int *height = 0) {
  std::vector<textstyle_t> &styles = block->styles;

  // for(auto style : styles) {
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

    /**  too slow
    if (treesitter) {
      for(auto s : block->span_infos) {
        if (s.start <= i && i < s.start + s.length) {
          if (s.scope.starts_with("source.")) {
             std::vector<TSNode> nodes = treesitter->walk(row, i);
            if (nodes.size() > 0) {
              TSNode n = nodes.back();
              const char *type = ts_node_type(n);
              std::string stype = type;
              if (stype == "identifier") {
                pair = pair_for_color(var, selected, is_cursor_row);
                underline = true;
              }
              if (stype == "type_identifier") {
                pair = pair_for_color(fn, selected, is_cursor_row);
                underline = true;
              }
              break;
            }
          }
        }
      }
    }
    */

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
  int view_end = scroll_y + vh;

  int offset_y = 0;

  // highlight
  int idx = 0;
  int start = scroll_y - vh / 2;
  if (start < 0)
    start = 0;

  int docSize = doc->size();

  for (int i = 0; i < vh * 2; i++) {
    if (idx + offset_y > editor->computed.h)
      break;

    int line = start + i;
    int computed_line = doc->computed_line(line);

    if (computed_line >= docSize) {
      break;
    }

    BlockPtr block = doc->block_at(computed_line);
    if (!block) {
      // ERROR!
      break;
    }

    // expensive?
    std::stringstream s;
#ifdef VIM_MODE
      char *_buf = (char*)vimBufferGetLine((buf_T*)(doc->buf), computed_line+1);
      s << _buf;
#else
      optional<std::u16string> row = text.line_for_row(computed_line);
      if (row) {
        s << u16string_to_string(*row);
      }
#endif
      s << " ";

    if (line >= view_start && line < view_end) {
      if (block->dirty) {
        if (doc->language && !doc->language->definition.isNull()) {

            block->styles = Textmate::run_highlighter(
                (char *)s.str().c_str(), doc->language, Textmate::theme(),
                block.get(), doc->previous_block(block).get(),
                doc->next_block(block).get(), NULL);
            // block->style_cache.clear();

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

      int line_height = 1;

      draw_text_line(editor, (idx++) + offset_y, computed_line, s.str().c_str(),
                     block, &line_height);

      block->line_height = line_height;

      if (line_height > 1) {
        offset_y += (line_height - 1);
      }
    }
  }

  for (int i = idx + offset_y; i < editor->computed.h; i++) {
    _move(editor->computed.y + i, editor->computed.x);
    // _addch('~')
    _clrtoeol();
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
  if (!view->show) return;
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

bool get_dimensions(int *width, int *height) {
  static struct winsize ws;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  int _width = ws.ws_col;
  int _height = ws.ws_row;
  if (_width != *width || _height != *height) {
    *width = _width;
    *height = _height;
    return true;
  }
  return false;
}

void layout(view_ptr root) {
  _clear();
  int margin = 0;
  root->layout(
      rect_t{margin, margin, width - (margin * 2), height - (margin * 2)});
  root->finalize();
}

void relayout() { last_layout_hash = -1; }

editor_ptr open_document(std::string path, editors_t &editors, view_ptr view) {
  editor_ptr e = editors.add_editor(path);
  e->flex = 1;
  int idx = 0;
  for (auto c : view->children) {
    if (c == e) {
      editors.selected = idx;
      view_t::input_focus = editors.current_editor();
      return e;
    }
    idx++;
  }
  view->add_child(e);
  view_t::input_focus = editors.current_editor();
  return e;
}

void close_current_editor(editors_t &editors, view_ptr view) {
  auto it = std::find(view->children.begin(), view->children.end(),
                      editors.current_editor());
  if (it != view->children.end()) {
    view->children.erase(it);
  }
  editors.close_current_editor();
  view_t::input_focus = editors.current_editor();
}

int main(int argc, char **argv) {
  int last_arg = 0;
  std::string file_path;
  if (argc > 1) {
    last_arg = argc - 1;
  }

  initLog();

  vimInit(argc, argv);
  win_setwidth(5);
  win_setheight(100);

  const char *defaultTheme = "Monokai";
  const char *argTheme = defaultTheme;
  for (int i = 0; i < argc - 1; i++) {
    if (strcmp(argv[i], "-t") == 0) {
      if (last_arg == i + 1) {
        last_arg = 0;
      }
      argTheme = argv[i + 1];
    }
  }

  if (last_arg != 0) {
    file_path = argv[last_arg];
  }

  Textmate::initialize("/home/iceman/.editor/extensions/");
  Textmate::load_theme(argTheme);
  theme_info_t info = Textmate::theme_info();

  FilesPtr files = std::make_shared<Files>();
  // files->load("./libs/tree-sitter-grammars");
  // files->load("./libs");

  menu_ptr menu = std::make_shared<menu_t>();

  view_ptr root = std::make_shared<column_t>();
  view_ptr main = std::make_shared<row_t>();
  status_bar_ptr status = std::make_shared<status_bar_t>();
  root->add_child(main);
  root->add_child(status);

  menu_ptr explorer = std::make_shared<menu_t>();
  explorer->frame.w = 22;
  explorer->focusable = true;
  main->add_child(explorer);

  view_ptr gutter = std::make_shared<view_t>();

  view_ptr editor_views = std::make_shared<view_t>();
  editor_views->flex = 1;

  // the editors
  editors_t editors;
  editor_ptr editor = open_document(file_path, editors, editor_views);
  files->set_root_path(
      directory_path(editor->doc->file_path, editor->doc->name));

  // for(int i=0; i<8; i++)
  // open_document("./src/autocomplete.cpp", editors, editor_views);

  tabs_ptr tabs = std::make_shared<tabs_t>();
  tabs->on_change = [&editors](int selected) {
    // printf(">>%d\n", selected);
    editors.selected = selected;
    return true;
  };

  tabs->frame.h = 1;
  view_ptr tabs_and_content = std::make_shared<column_t>();
  tabs_and_content->flex = 3;
  main->add_child(tabs_and_content);

  view_ptr gutter_and_content = std::make_shared<row_t>();
  gutter_and_content->flex = 1;
  gutter_and_content->add_child(gutter);
  gutter_and_content->add_child(editor_views);
  editor_views->flex = 1;

  tabs_and_content->add_child(tabs);
  tabs_and_content->add_child(gutter_and_content);

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

  // callbacks
  explorer->on_submit = [&files, &explorer, &editors, &editor_views](int idx) {
    FileList &tree = files->tree();
    FileItemPtr item = tree[idx];
    if (item->is_directory) {
      item->expanded = !item->expanded;
      if (item->expanded) {
        files->load(item->full_path);
      } else {
        files->rebuild_tree();
        explorer->items.clear();
      }
    } else {
      open_document(item->full_path, editors, editor_views);
    }
    return true;
  };

  explorer->show = false; // files->root->name.size() > 0; 

  init_renderer();

  // printf("\x1b[?2004h");
  update_colors();

  _curs_set(0);
  _clear();

  get_dimensions(&width, &height);

  std::stringstream message;
  message << "Welcome to text-edit";

  int warm_start = 3;
  std::string last_key_sequence;

  editor_ptr prev;
  bool running = true;
  while (running) {
    if (!editors.editors.size()) {
      break;
    }

    tabs->show = editors.editors.size() > 1;

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
        relayout();
      }
      has_input = has_input | i->has_focus();
    }
    // status->show = !has_input;

    if (last_layout_hash != size) {
      std::stringstream s;
      s << "  ";
      s << size;
      gutter->frame.w = s.str().size();
      layout(root);
      last_layout_hash = size;
    }

    // explorer
    draw_menu(explorer, pair_for_color(cmt, false, false),
              explorer->has_focus() ? pair_for_color(fn, false, true)
                                    : pair_for_color(cmt, false, false));

#ifdef VIM_MODE
    int cl = vimCursorGetLine();
    int cc = vimCursorGetColumn();
    Cursor& _cur = doc->cursor();
    _cur.start.row = cl-1;
    _cur.start.column = cc;
    _cur.end = _cur.start;
#endif

    draw_tabs(tabs, editors);
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

        if (search->selected < search->matches.size()) {
          Range range = search->matches[search->selected];
          Cursor &cursor = doc->cursor();
          if (range != cursor) {
            cursor.start = range.start;
            cursor.end = range.end;
            editor->on_input(-1, ""); // ping
            continue;
          }
        }
      }
    }

    if (find->show) {
      find->items["title"]->color = find->input->has_focus()
                                        ? pair_for_color(fn, false, false)
                                        : pair_for_color(cmt, false, false);
      find->items["replace"]->color = find->replace->has_focus()
                                          ? pair_for_color(fn, false, false)
                                          : pair_for_color(cmt, false, false);
    }

    if (gotoline->show) {
      gotoline->items["title"]->color = gotoline->input->has_focus()
                                            ? pair_for_color(fn, false, false)
                                            : pair_for_color(cmt, false, false);
    }

    // other inputs
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
        } else if (cursor.start.row + 1 >= size) {
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

    // tree sitter
    TreeSitterPtr treesitter = doc->treesitter();
    if (treesitter) {
      draw_tree_sitter(editor, sitter, treesitter, doc->cursor());
    }

    // auto complete menu
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
      menu->computed.w = w;
      menu->computed.h = menu->items.size();
      if (menu->computed.h > 10) {
        menu->computed.h = 10;
      }
      int h = menu->computed.h;

      int offset_row = 1;
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
      menu->computed.x = screen_col;
      menu->computed.y = screen_row + offset_row;
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
    draw_menu(menu);

    // blit
    _move(view_t::input_focus->cursor.y, view_t::input_focus->cursor.x);

    _refresh();
    _curs_set(1);

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
      if (files->update() || !explorer->items.size()) {
        explorer->items.clear();
        FileList &tree = files->tree();
        for (auto f : tree) {
          std::string n;
          int d = f->depth;
          for (int i = 0; i < d * 1; i++) {
            n += " ";
          }

          if (f->is_directory) {
            if (f->expanded) {
              n += "- ";
            } else {
              n += "+ ";
            }
          } else {
            n += " ";
          }

          n += f->name;
          explorer->items.push_back(menu_item_t{n, f->full_path});
        }
        break;
      }

      if (editor->on_idle(frames)) {
        break;
      }

      // check dimensions
      if (frames == 1800 && get_dimensions(&width, &height)) {
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
          delay(250);
          _curs_set(0);
        }
      }
    }

#ifdef VIM_MODE
    size_t start_len = doc->size();
    cl = vimCursorGetLine();

    bool didVimInput = false;

    if (key_sequence == "" && ch != -1) {
      char seq[] = { (char)ch, 0, 0, 0, 0 };
      vimKey((unsigned char*)seq);
      // log("%c %d\n", ch, ch);
      didVimInput = true;
    } else  {
      std::string remapped = remapKey(key_sequence);
      if (remapped != "") {
        vimKey((unsigned char*)remapped.c_str());
        didVimInput = true;
      }
    }

    if (didVimInput) {
      size_t end_len = doc->size();
      int ncl = vimCursorGetLine();
      BlockPtr block = doc->block_at(ncl-1);
      if (block) {
        block->make_dirty();
      }

      int diff = end_len - start_len;
      // log("%d %d >>%d\n", cl, ncl, diff);
      if (diff > 0) {
        for(int i=0; i<diff; i++) {
          doc->add_block_at(ncl);
        }
      } else if (diff < 0) {
        for(int i=0; i<-diff; i++) {
          doc->erase_block_at(ncl);
        }
        // for(int i=0; i<-diff; i++) {
        //   BlockPtr block = doc->block_at(ncl+i-1);
        //   if (block) {
        //     block->make_dirty();
        //   }
        // }
      }
      continue;
    }
#endif

    _curs_set(0);
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
        search->selected--;
      }
      if (key_sequence == "down") {
        search->selected++;
      }
      if (search->selected < 0) {
        search->selected = 0;
      }
      if (search->selected >= search->matches.size()) {
        search->selected = search->matches.size() - 1;
      }
    }

    if (view_t::input_focus == editor && menu->show) {
      if (menu->on_input(ch, key_sequence)) {
        continue;
      }
    }
    if (view_t::input_focus->on_input(ch, key_sequence)) {
      if (!view_t::input_focus) {
        view_t::input_focus = editors.current_editor();
      }
      view_t::input_focus->update_scroll();
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
      }
      message << "] ";

      message << key_sequence;
      message << " ";
      message << doc->buffer.is_modified();
    }

    if (cmd.command == "toggle_explorer") {
      if (!explorer->show) {
        explorer->show = (explorer->show = files->root->name.size() > 0);
        if (explorer->show) {
          view_t::input_focus = explorer;
          relayout();
        }
        continue;
      } else if (explorer->show) {
        explorer->show = false;
        view_t::input_focus = editor;
        relayout();
        continue;
      }
    }
    if (cmd.command == "toggle_tabs") {
      if (!tabs->show) {
        tabs->show = true;
        view_t::input_focus = tabs;
        relayout();
        continue;
      } else if (tabs->show) {
        tabs->show = false;
        view_t::input_focus = editor;
        relayout();
        continue;
      }
    }

    if (cmd.command == "switch_tab") {
      int idx = std::stoi(cmd.params);
      if (idx < editors.editors.size()) {
        editors.selected = idx;
        view_t::input_focus = editors.current_editor();
      }
    }

    if (cmd.command == "focus_left") {
      view_ptr next = view_t::find_next_focus(root, view_t::input_focus, -1, 0);
      if (next) {
        view_t::input_focus = next;
      }
    }
    if (cmd.command == "focus_right") {
      view_ptr next = view_t::find_next_focus(root, view_t::input_focus, 1, 0);
      if (next) {
        view_t::input_focus = next;
      }
    }
    if (cmd.command == "focus_up") {
      view_ptr next = view_t::find_next_focus(root, view_t::input_focus, 0, -1);
      if (next) {
        view_t::input_focus = next;
      }
    }
    if (cmd.command == "focus_down") {
      view_ptr next = view_t::find_next_focus(root, view_t::input_focus, 0, 1);
      if (next) {
        view_t::input_focus = next;
      }
    }

    // todo move to view
    if (cmd.command == "search_text" || cmd.command == "search_and_replace") {
      view_t::input_focus = find->input;
      find->enable_replace = cmd.command == "search_and_replace";
      find->input->on_input(-1, "!select_all"); // << todo
      find->input->cursor = {0, 0};
      find->input->on_submit = [&editor, &find](std::u16string value) {
        editor->doc->run_search(value, editor->doc->cursor().start);
        editor->doc->clear_cursors();

        if (find->input->has_focus() && editor->doc->search()) {
          editor->doc->search()->selected++;
          if (editor->doc->search()->selected >=
              editor->doc->search()->matches.size()) {
            editor->doc->search()->selected = 0;
          }
        }
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
        } else {
          search->selected = 0;
        }
        return true;
      };
      find->replace_title->show = find->enable_replace;
      find->replace->show = find->enable_replace;
      if (find->enable_replace && search && search->matches.size()) {
        view_t::input_focus = find->replace;
      }
      relayout();
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
      relayout();
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
      close_current_editor(editors, editor_views);
    }
  }

  shutdown_renderer();

  // graceful exit... shutting down...
  int idx = 20;
  while ((Textmate::has_running_threads() || files->has_running_threads()) &&
         idx-- > 0) {
    delay(50);
  }

  Textmate::shutdown();
  return 0;
}
