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
#include "editor_view.h"
#include "input.h"
#include "keybindings.h"
#include "textmate.h"
#include "theme.h"
#include "utf8.h"
#include "view.h"

std::vector<std::string> outputs;

// theme
bool use_system_colors = true;
int fg = 0;
int bg = 0;
int hl = 0;
int sel = 0;
int cmt = 0;
int width = 0;
int height = 0;
int theme_id = -1;
int lang_id = -1;

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

void draw_text(view_ptr view, const char *text, int align = 1, int margin = 0) {
  if (!view->show)
    return;
  int off = 0;
  switch (align) {
  case 0:
    // center
    off = view->computed.w / 2 - strlen(text) / 2;
    break;
  case -1:
    // left
    off = margin;
    break;
  case 1:
    // right
    off = view->computed.w - strlen(text) - margin;
    break;
  }

  int screen_col = view->computed.x + off;
  int screen_row = view->computed.y;

  int pair = pair_for_color(cmt);
  attron(COLOR_PAIR(pair));

  move(screen_row, view->computed.x);
  for (int i = 0; i < view->computed.w; i++) {
    addch(' ');
  }
  move(screen_row, screen_col);
  addstr(text);

  attroff(COLOR_PAIR(pair));
}

void draw_gutter_line(EditorPtr editor, view_ptr view, int screen_row, int row,
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
  for (int i = 0; i < view->computed.w; i++) {
    addch(' ');
  }
  move(screen_row, screen_col + view->computed.w - l - 1);
  addstr(text);
  attroff(COLOR_PAIR(pair));
}

void draw_gutter(EditorPtr editor, view_ptr view) {
  if (!view->show)
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
  for (int i = 0; i < (vh * 1.5); i++) {
    int line = start + i;
    BlockPtr block = doc->block_at(line);
    if (!block) {
      move(line + view->computed.y, view->computed.x);
      draw_clear(view->computed.w);
      continue;
    }

    if (line >= view_start && line < view_end) {
      std::stringstream s;
      s << (line + 1);
      draw_gutter_line(editor, view, idx++, line, s.str().c_str(),
                       block->styles);
    }
  }
}

void draw_text_line(EditorPtr editor, int screen_row, int row, const char *text,
                    std::vector<textstyle_t> &styles) {
  int scroll_x = editor->scroll.x;
  int scroll_y = editor->scroll.y;

  DocumentPtr doc = editor->doc;
  optional<Cursor> block_cursor = doc->block_cursor(doc->cursor());
  optional<Cursor> span_cursor; // = doc->span_cursor(doc->cursor());

  screen_row += editor->computed.y;
  int screen_col = editor->computed.x;
  move(screen_row, screen_col);
  clrtoeol();

  int l = strlen(text);

  doc->cursor(); // ensure 1 cursor
  std::vector<Cursor> &cursors = doc->cursors;

  int edge = pair_for_color(fg, true, false);

  bool is_cursor_row = editor->focused && row == doc->cursor().start.row;
  int default_pair = pair_for_color(fg, false, is_cursor_row);
  for (int i = scroll_x; i < l; i++) {
    int pair = default_pair;
    bool selected = false;

    if (i - scroll_x + 1 > editor->computed.w)
      break;

    // build...
    for (auto cursor : cursors) {
      if (cursor.is_within(row, i)) {
        selected = cursors.size() > 1 || cursor.has_selection();
        if (cursor.has_selection() && cursor.is_normalized()) {
          if (row == cursor.end.row && i == cursor.end.column) {
            selected = false;
          }
        }
        if (row == cursor.start.row && i == cursor.start.column) {
          editor->cursor.x = screen_col + i - scroll_x;
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

    char ch = text[i];

    for (auto s : styles) {
      if (s.start >= i && i < s.start + s.length) {
        int colorIdx = color_index(s.r, s.g, s.b);
        pair = pair_for_color(colorIdx, selected, is_cursor_row);
        pair = pair > 0 ? pair : default_pair;
        break;
      }
    }

    if (block_cursor) {
      if ((*block_cursor).is_edge(row, i)) {
        pair = edge;
      }
    }

    if (span_cursor) {
      if ((*span_cursor).is_edge(row, i)) {
        attron(A_UNDERLINE);
      }
    }

    attron(COLOR_PAIR(pair));
    addch(ch);
    attroff(COLOR_PAIR(pair));
    // attroff(A_BLINK);
    // attroff(A_STANDOUT);
    attroff(A_REVERSE);
    attroff(A_BOLD);
    attroff(A_UNDERLINE);
  }

  if (is_cursor_row) {
    move(screen_row, screen_col + l);
    for (int i = 0; i < editor->computed.w - l - scroll_x; i++) {
      attron(COLOR_PAIR(default_pair));
      addch(' ');
      attroff(COLOR_PAIR(default_pair));
    }
  }
}

void draw_text_buffer(EditorPtr editor) {
  if (!editor->show)
    return;

  DocumentPtr doc = editor->doc;
  TextBuffer &text = doc->buffer;

  int scroll_x = editor->scroll.x;
  int scroll_y = editor->scroll.y;
  int vh = editor->computed.h;
  int view_start = scroll_y;
  int view_end = scroll_y + vh;

  // highlight
  int idx = 0;
  int start = scroll_y - vh / 2;
  if (start < 0)
    start = 0;
  for (int i = 0; i < (vh * 1.5); i++) {
    int line = start + i;
    BlockPtr block = doc->block_at(line);
    if (!block) {
      move(line + editor->computed.y, editor->computed.x);
      clrtoeol();
      continue;
    }

    optional<std::u16string> row = text.line_for_row(line);
    std::stringstream s;
    if (row) {
      s << u16string_to_string(*row);
      s << " ";
    }
    if (block->dirty) {
      block->styles = Textmate::run_highlighter(
          (char *)s.str().c_str(), lang_id, theme_id, block.get(),
          doc->previous_block(block).get(), doc->next_block(block).get());
      block->dirty = false;
    }

    if (line >= view_start && line < view_end) {
      draw_text_line(editor, idx++, line, s.str().c_str(), block->styles);
    }
  }
}

void draw_search(EditorPtr editor, SearchPtr search) {
  DocumentPtr doc = editor->doc;

  int row = 0;
  int def = pair_for_color(cmt, false, true);
  int sel = pair_for_color(fg, true, false);

  int pair = def;
  int screen_col = width - 20; // editor->computed.x;
  int screen_row = editor->computed.y + row++;

  std::stringstream ss;
  ss << search->matches.size();
  ss << " found";
  attron(COLOR_PAIR(pair));
  move(screen_row, screen_col);
  draw_clear(20);
  move(screen_row, screen_col + 1);
  addstr(ss.str().c_str());
  attroff(COLOR_PAIR(pair));

  /*
  for (auto m : search->matches) {
    int pair = def;
    int screen_row = editor->computed.y + row++;
    // std::string text = u16string_to_string(m.string);
    std::stringstream ss;
    ss << m.start;
    ss << "-";
    ss << m.end;
    attron(COLOR_PAIR(pair));
    move(screen_row, screen_col);
    draw_clear(20);
    move(screen_row, screen_col);
    addstr(ss.str().c_str());
    attroff(COLOR_PAIR(pair));
    if (row > editor->computed.h)
      break;
  }*/
}

void draw_autocomplete(EditorPtr editor, AutoCompletePtr autocomplete) {
  DocumentPtr doc = editor->doc;

  int row = 0;
  int def = pair_for_color(cmt, false, true);
  int sel = pair_for_color(fg, true, false);

  int w = 0;
  int h = 0;

  // get dimensions
  for (auto m : autocomplete->matches) {
    if (w < m.string.size()) {
      w = m.string.size();
    }
    h++;
    if (h > 10)
      break;
  }

  int margin = 1;
  w += (margin * 2);

  optional<Range> sub = doc->subsequence_range();
  int offset_row = 0;
  int screen_col = editor->cursor.x;
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

  for (auto m : autocomplete->matches) {
    int pair = autocomplete->selected == row ? sel : def;
    int screen_row = editor->cursor.y + 1 + row++;
    std::string text = u16string_to_string(m.string);
    attron(COLOR_PAIR(pair));
    move(screen_row + offset_row, screen_col);
    draw_clear(w);
    move(screen_row + offset_row, screen_col + margin);
    addstr(text.c_str());
    attroff(COLOR_PAIR(pair));
    if (row > 10)
      break;
  }
}

void draw_tree_sitter(EditorPtr editor, view_ptr view, TreeSitterPtr treesitter,
                      Cursor cursor) {
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
  TextFieldPtr input;
  input = std::make_shared<textfield_t>();

  EditorPtr editor;
  editor = std::make_shared<editor_t>();
  DocumentPtr doc = editor->doc;

  view_ptr root = std::make_shared<column_t>();
  view_ptr main = std::make_shared<row_t>();
  view_ptr status = std::make_shared<row_t>();
  view_ptr status_message = std::make_shared<row_t>();
  view_ptr status_line_col = std::make_shared<row_t>();
  status->children.push_back(status_message);
  status->children.push_back(status_line_col);
  status_message->flex = 3;
  status_line_col->flex = 2;
  root->children.push_back(main);
  root->children.push_back(status);
  root->children.push_back(input);
  view_ptr gutter = std::make_shared<view_t>();
  main->children.push_back(gutter);
  main->children.push_back(editor);

  view_ptr sitter = std::make_shared<view_t>();
  main->children.push_back(sitter);

  main->flex = 1;
  status->frame.h = 1;
  gutter->frame.w = 8;
  editor->flex = 3;
  sitter->flex = 1;
  sitter->show = false;
  input->frame.h = 1;

  const char *defaultTheme = "Monokai";
  const char *argTheme = defaultTheme;

  for (int i = 0; i < argc - 1; i++) {
    if (strcmp(argv[i], "-t") == 0) {
      argTheme = argv[i + 1];
    }
  }

  std::string file_path = "./tests/main.cpp";
  if (argc > 1) {
    file_path = argv[argc - 1];
  }

  Textmate::initialize("/home/iceman/.editor/extensions/");
  theme_id = Textmate::load_theme(argTheme);
  lang_id = Textmate::load_language(file_path);
  doc->language = Textmate::language_info(lang_id);

  theme_info_t info = Textmate::theme_info();

  doc->load(file_path);

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

  EditorPtr focused = editor;

  int last_layout_size = -1;
  std::string last_key_sequence;
  bool running = true;
  while (running) {
    DocumentPtr doc = editor->doc;
    TextBuffer &text = doc->buffer;
    int size = doc->size();

    editor->focused = focused == editor;
    input->focused = focused == input;
    if (input->focused != input->show) {
      input->show = input->focused;
      last_layout_size = -1;

      if (!input->show) {
        editor->doc->clear_search();
      }
    }

    if (last_layout_size != size) {
      std::stringstream s;
      s << "  ";
      s << size;
      gutter->frame.w = s.str().size();
      layout(root);
      last_layout_size = size;
    }

    draw_gutter(editor, gutter);
    draw_text_buffer(editor);
    draw_text_buffer(input);

    Cursor cursor = doc->cursor();

    // status
    std::stringstream ss;
    ss << doc->language->id;
    ss << "  ";
    ss << " line ";
    ss << (cursor.start.row + 1);
    ss << " col ";
    ss << (cursor.start.column + 1);
    ss << "  ";
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

    if (status->show) {
      draw_text(status_message, message.str().c_str(), -1);
      draw_text(status_line_col, ss.str().c_str(), 1);
    }

    SearchPtr search = doc->search();
    if (search) {
      draw_search(editor, search);
    }

    AutoCompletePtr autocomplete = doc->autocomplete();
    if (autocomplete) {
      draw_autocomplete(editor, autocomplete);
    }

    TreeSitterPtr treesitter = doc->treesitter();
    if (treesitter) {
      draw_tree_sitter(editor, sitter, treesitter, doc->cursor());
    }

    move(focused->cursor.y, focused->cursor.x);
    curs_set(1);
    refresh();

    int idle = 0;
    int ch = -1;
    std::string key_sequence;
    int frames = 0;

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
        break;
      }

      // loop
      if (frames > 2000) {
        frames = 0;
        idle++;
      }

      if (idle > 8)
        delay(50);
    }

    curs_set(0);

    if (ch == 27) {
      key_sequence = "escape";
      ch = -1;
    }

    // popups
    if (autocomplete) {
      if (key_sequence == "up") {
        if (autocomplete->selected > 0) {
          autocomplete->selected--;
        }
        continue;
      }
      if (key_sequence == "down") {
        if (autocomplete->selected + 1 < autocomplete->matches.size()) {
          autocomplete->selected++;
        }
        continue;
      }
      if (key_sequence == "enter" || key_sequence == "tab") {
        optional<Range> range = doc->subsequence_range();
        if (range) {
          std::u16string selected =
              autocomplete->matches[autocomplete->selected].string;
          doc->clear_cursors();
          doc->cursor().copy_from(Cursor{(*range).start, (*range).end});
          doc->insert_text(selected);
        }
        doc->clear_autocomplete(true);
        continue;
      }
      if (key_sequence == "left" || key_sequence == "right") {
        doc->clear_autocomplete(true);
        continue;
      }
    }

    if (search) {
      bool scroll_to_selected = false;
      if (key_sequence == "up") {
        if (search->selected > 0) {
          search->selected--;
        }
        scroll_to_selected = true;
      }
      if (key_sequence == "down") {
        if (search->selected + 1 < search->matches.size()) {
          search->selected++;
        }
        scroll_to_selected = true;
      }
      if (scroll_to_selected) {
        Range range = search->matches[search->selected];
        Cursor &cursor = doc->cursor();
        cursor.start = range.start;
        cursor.end = range.end;
        editor->on_input(-1, ""); // ping
        continue;
      }
      // if (key_sequence == "enter" || key_sequence == "tab") {
      //   continue;
      // }
      // if (key_sequence == "left" || key_sequence == "right") {
      //   continue;
      // }
    }

    if (focused->on_input(ch, key_sequence)) {
      continue;
    }

    Command &cmd = command_from_keys(key_sequence, last_key_sequence);
    if (cmd.command == "await") {
      last_key_sequence = key_sequence;
      continue;
    }
    last_key_sequence = "";

    if (key_sequence != "") {
      message.str(key_sequence);
      message << " ";
      message << cmd.command;
    }

    if (cmd.command == "search_text") {
      focused = input;
      input->doc->initialize(Document::empty());
      input->cursor = {0, 0};
      input->on_submit = [&editor, &message](std::u16string value) {
        editor->doc->run_search(value);
        message.str(u16string_to_string(value));
        return true;
      };
    }
    if (cmd.command == "jump_to_line") {
      focused = input;
      input->doc->initialize(Document::empty());
      input->cursor = {0, 0};
      input->on_submit = [&focused, &editor, &message](std::u16string value) {
        focused = editor;
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
      focused = editor;
    }
    if (cmd.command == "toggle_tree_sitter") {
      sitter->show = !sitter->show;
      layout(root);
    }
    if (cmd.command == "toggle_gutter") {
      gutter->show = !gutter->show;
      layout(root);
    }
    if (cmd.command == "toggle_statusbar") {
      status->show = !status->show;
      layout(root);
    }
    if (cmd.command == "quit") {
      running = false;
    }
    if (cmd.command == "close") {
      running = false;
    }
  }

  endwin();

  for (auto k : outputs) {
    printf(">%s\n", k.c_str());
  }

  // printf("%s\n", doc->buffer.get_dot_graph().c_str());
  printf(">%s\n", doc->language->id.c_str());
  return 0;
}
