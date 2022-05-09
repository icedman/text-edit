#include <core/encoding-conversion.h>
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
#include "input.h"
#include "keybindings.h"
#include "textmate.h"
#include "theme.h"
#include "utf8.h"
#include "view.h"

std::vector<std::string> outputs;

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

int scroll_y = 0;
int scroll_x = 0;
int blink_y = 0;
int blink_x = 0;

Document doc;

#define SELECTED_OFFSET 500
#define HIGHLIGHT_OFFSET 1000

enum color_pair_e { NORMAL = 0, SELECTED, COMMENT };
static std::map<int, int> colorMap;

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

  fg = info.fg_a;
  bg = COLOR_BLACK;
  // bg = color_index(info.bg_r, info.bg_g, info.bg_b);
  // bg = color_index(255, 0, 0);
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

void draw_text(view_ptr view, const char *text, int align = 1) {
  int margin = 2;
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

void draw_text_line(int screen_row, int row, const char *text,
                    std::vector<textstyle_t> &styles, view_ptr view) {
  screen_row += view->computed.y;
  int screen_col = view->computed.x;
  move(screen_row, screen_col);
  clrtoeol();

  int l = strlen(text);

  doc.cursor(); // ensure 1 cursor
  std::vector<Cursor> &cursors = doc.cursors;

  bool is_cursor_row = row == doc.cursor().start.row;
  int default_pair = pair_for_color(fg, false, is_cursor_row);
  for (int i = scroll_x; i < l; i++) {
    int pair = default_pair;
    bool selected = false;

    if (i - scroll_x + 1 > view->computed.w)
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
          blink_x = screen_col + i - scroll_x;
          blink_y = screen_row;
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
        pair =
            pair_for_color(colorIdx, selected, row == doc.cursor().start.row);
        pair = pair > 0 ? pair : default_pair;
        break;
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
    for (int i = 0; i < view->computed.w - l - scroll_x; i++) {
      attron(COLOR_PAIR(default_pair));
      addch(' ');
      attroff(COLOR_PAIR(default_pair));
    }
  }
}

void draw_gutter_line(int screen_row, int row, const char *text,
                      std::vector<textstyle_t> &styles, view_ptr view) {
  screen_row += view->computed.y;
  int screen_col = view->computed.x;
  int l = strlen(text);

  doc.cursor(); // ensure 1 cursor
  std::vector<Cursor> &cursors = doc.cursors;

  bool is_cursor_row = row == doc.cursor().start.row;
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

void draw_gutter(TextBuffer &text, int scroll_y, view_ptr view) {
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
    BlockPtr block = doc.block_at(line);
    if (!block) {
      move(line + view->computed.y, view->computed.x);
      clrtoeol();
      continue;
    }

    if (line >= view_start && line < view_end) {
      std::stringstream s;
      s << (line + 1);
      draw_gutter_line(idx++, line, s.str().c_str(), block->styles, view);
    }
  }
}

void draw_text_buffer(TextBuffer &text, int scroll_y, view_ptr view) {
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
    BlockPtr block = doc.block_at(line);
    if (!block) {
      move(line + view->computed.y, view->computed.x);
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
          doc.previous_block(block).get(), doc.next_block(block).get());
      block->dirty = false;
    }

    if (line >= view_start && line < view_end) {
      draw_text_line(idx++, line, s.str().c_str(), block->styles, view);
    }
  }
}

void draw_autocomplete(AutoCompletePtr autocomplete) {
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
  w += (margin*2);

  optional<Range> sub = doc.subsequence_range();
  int offset_row = 0;
  int screen_col = blink_x;
  if (sub) {
    screen_col -= ((*sub).end.column - (*sub).start.column);
  }

  if (blink_x + w > width) {
    screen_col = blink_x - w;
  }
  if (blink_y + 2 + h > height) {
    offset_row = -(h + 1);
  }

  screen_col -= margin;

  for (auto m : autocomplete->matches) {
    int pair = autocomplete->selected == row ? sel : def;
    int screen_row = blink_y + 1 + row++;
    std::string text = u16string_to_string(m.string);
    attron(COLOR_PAIR(pair));
    move(screen_row + offset_row, screen_col);
    for (int i = 0; i < w; i++) {
      addch(' ');
    }
    move(screen_row + offset_row, screen_col + margin);
    addstr(text.c_str());
    attroff(COLOR_PAIR(pair));
    if (row > 10)
      break;
  }
}

void draw_tree_sitter(view_ptr sitter, TreeSitterPtr treesitter, Cursor cursor) {
  std::vector<TSNode> nodes = treesitter->walk(cursor.start.row, cursor.start.column);
  
  int def = pair_for_color(cmt, false, false);
  int sel = pair_for_color(fg, false, true);

  int row = 0;
  for(auto node : nodes) {
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
    move(sitter->computed.y + row++, sitter->computed.x);
    addstr(ss.str().c_str());
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
  root->render();
}

int main(int argc, char **argv) {

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
  view_ptr gutter = std::make_shared<view_t>();
  view_ptr editor = std::make_shared<view_t>();
  main->children.push_back(gutter);
  main->children.push_back(editor);

  view_ptr sitter = std::make_shared<view_t>();
  main->children.push_back(sitter);
  
  main->flex = 1;
  status->frame.h = 1;
  gutter->frame.w = 8;
  editor->flex = 3;
  sitter->flex = 1;

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

  theme_info_t info = Textmate::theme_info();

  std::ifstream t(file_path);
  std::stringstream buffer;
  buffer << t.rdbuf();

  std::u16string str;

  // TIMER_BEGIN
  optional<EncodingConversion> enc = transcoding_from("UTF-8");
  (*enc).decode(str, buffer.str().c_str(), buffer.str().size());
  // TIMER_END

  TextBuffer &text = doc.buffer;
  doc.initialize(str);

  std::vector<Cursor> &cursors = doc.cursors;

  setlocale(LC_ALL, "");

  initscr();
  raw();
  keypad(stdscr, true);
  noecho();
  nodelay(stdscr, true);

  start_color();
  // use_default_colors();
  update_colors();

  curs_set(0);
  clear();

  get_dimensions();

  std::string message = "Welcome to text-edit";

  int last_layout_size = -1;
  std::string last_key_sequence;
  bool request_autocomplete = false;
  bool request_treesitter = false;
  bool running = true;
  while (running) {
    int size = text.extent().row;

    if (last_layout_size != size) {
      std::stringstream s;
      s << "  ";
      s << size;
      gutter->frame.w = s.str().size();
      layout(root);
      last_layout_size = size;
    }

    draw_gutter(text, scroll_y, gutter);
    draw_text_buffer(text, scroll_y, editor);

    Cursor cursor = doc.cursor();

    // status
    std::stringstream status;
    status << " line ";
    status << (cursor.start.row + 1);
    status << " col ";
    status << (cursor.start.column + 1);
    status << " ";
    int p = (100 * cursor.start.row) / size;
    if (p == 0) {
      status << "top";
    } else if (p == 100) {
      status << "end";
    } else {
      status << p;
      status << "%";
    }

    draw_text(status_message, message.c_str(), -1);
    draw_text(status_line_col, status.str().c_str());

    AutoCompletePtr autocomplete = doc.autocomplete();
    if (autocomplete) {
      draw_autocomplete(autocomplete);
    }

    TreeSitterPtr treesitter = doc.treesitter();
    if (treesitter) {
      draw_tree_sitter(sitter, treesitter, doc.cursor());
    }

    move(blink_y, blink_x);
    curs_set(1);
    refresh();

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

      // check autocomplete
      if (frames == 500 && request_treesitter) {
        doc.run_treesitter();
        request_treesitter = false;
        break;
      }

      if (frames == 750 && request_autocomplete) {
        doc.clear_autocomplete(true);
        doc.run_autocomplete();
        request_autocomplete = false;
        break;
      }
      if (frames == 1500) {
        autocomplete = doc.autocomplete();
        if (autocomplete &&
            autocomplete->state != AutoComplete::State::Consumed) {
          autocomplete->set_consumed();
          break;
        }
      }

      // check dimensions
      if (frames == 1800 && get_dimensions()) {
        layout(root);
        break;
      }

      // loop
      if (frames > 2000) {
        frames = 0;
      }
    }

    curs_set(0);

    if (ch == 27) {
      key_sequence = "escape";
      ch = -1;
    }

    if (autocomplete) {
      if (key_sequence == "up") {
        if (autocomplete->selected > 0) {
          autocomplete->selected--;
        }
        key_sequence = "";
        ch = -1;
      }
      if (key_sequence == "down") {
        if (autocomplete->selected + 1 < autocomplete->matches.size()) {
          autocomplete->selected++;
        }
        key_sequence = "";
        ch = -1;
      }
      if (key_sequence == "enter") {
        key_sequence = "";
        ch = -1;
        optional<Range> range = doc.subsequence_range();
        if (range) {
          std::u16string selected =
              autocomplete->matches[autocomplete->selected].string;
          doc.clear_cursors();
          doc.cursor().copy_from(Cursor{(*range).start, (*range).end});
          doc.insert_text(selected);
        }
      }
      if (key_sequence == "left" ||
          key_sequence == "right") {
        doc.clear_autocomplete(true);
        key_sequence = "";
        ch = -1;
      }
    }

    Command &cmd = command_from_keys(key_sequence, last_key_sequence);

    if (key_sequence != "") {
      message = cmd.command;
      // message = key_sequence;
      //   outputs.push_back(key_sequence);
      //   outputs.push_back(cmd.command);
    }

    if (cmd.command == "quit") {
      running = false;
    }

    if (cmd.command == "await") {
      last_key_sequence = key_sequence;
      continue;
    }
    last_key_sequence = "";

    if (cmd.command == "save") {
      message = "save - unimplemented";
    }
    if (cmd.command == "close") {
      running = false;
    }

    if (cmd.command == "cancel") {
      doc.clear_cursors();
      doc.clear_autocomplete(true);
    }

    if (cmd.command == "undo") {
      doc.undo();
    }

    if (cmd.command == "copy") {
      doc.copy();
    }
    if (cmd.command == "cut") {
      doc.copy();
      doc.delete_text();
    }
    if (cmd.command == "paste") {
      doc.paste();
    }
    if (cmd.command == "select_word") {
      doc.add_cursor_from_selected_word();
    }
    if (cmd.command == "select_all") {
      doc.clear_cursors();
      doc.move_to_start_of_document();
      doc.move_to_end_of_document(true);
    }
    if (cmd.command == "select_line") {
      doc.move_to_start_of_line();
      doc.move_to_end_of_line(true);
    }
    if (cmd.command == "selection_to_uppercase") {
      doc.selection_to_uppercase();
    }
    if (cmd.command == "selection_to_lowercase") {
      doc.selection_to_lowercase();
    }
    if (cmd.command == "indent") {
      doc.indent();
    }
    if (cmd.command == "unindent") {
      doc.unindent();
    }

    if (cmd.command == "move_up") {
      doc.move_up(cmd.params == "anchored");
    }
    if (cmd.command == "move_down") {
      doc.move_down(cmd.params == "anchored");
    }
    if (cmd.command == "move_left") {
      doc.move_left(cmd.params == "anchored");
    }
    if (cmd.command == "move_right") {
      doc.move_right(cmd.params == "anchored");
    }

    if (cmd.command == "pageup") {
      for (int i = 0; i < height; i++)
        doc.move_up();
    }
    if (cmd.command == "pagedown") {
      for (int i = 0; i < height; i++)
        doc.move_down();
    }
    if (cmd.command == "move_to_start_of_line") {
      doc.move_to_start_of_line();
    }
    if (cmd.command == "move_to_end_of_line") {
      doc.move_to_end_of_line();
    }

    if (cmd.command == "add_cursor_and_move_up") {
      doc.add_cursor(doc.cursor());
      doc.cursor().move_up();
    }
    if (cmd.command == "add_cursor_and_move_down") {
      doc.add_cursor(doc.cursor());
      doc.cursor().move_down();
    }
    if (cmd.command == "move_to_previous_word") {
      doc.move_to_previous_word();
    }
    if (cmd.command == "move_to_next_word") {
      doc.move_to_next_word();
    }

    cursor = doc.cursor();

    if (key_sequence == "enter") {
      key_sequence = "";
      ch = '\n';
    }

    // input
    if (key_sequence == "" && ch != -1) {
      std::u16string text = u"x";
      text[0] = ch;
      doc.insert_text(text);
      request_autocomplete = true;
      request_treesitter = true;
    }

    if (!request_autocomplete && autocomplete) {
      doc.clear_autocomplete();
    }

    // delete
    if (cmd.command == "backspace") {
      if (!doc.has_selection()) {
        doc.move_left();
      }
      doc.delete_text();
      request_treesitter = true;
    }
    if (cmd.command == "delete") {
      doc.delete_text();
      request_treesitter = true;
    }

    size = text.extent().row;
    int lead = 0;
    int hh = editor->computed.h;
    if (cursor.start.row >= scroll_y + (hh - 1) - lead) {
      scroll_y = -(hh - 1) + cursor.start.row + lead;
    }
    if (scroll_y + lead > cursor.start.row) {
      scroll_y = -lead + cursor.start.row;
    }
    if (scroll_y + hh / 2 > size) {
      scroll_y = size - hh / 2;
    }
    if (scroll_y < 0) {
      scroll_y = 0;
    }
    int ww = editor->computed.w;
    if (cursor.start.column >= scroll_x + (ww - 1) - lead) {
      scroll_x = -(ww - 1) + cursor.start.column + lead;
    }
    if (scroll_x + lead > cursor.start.column) {
      scroll_x = -lead + cursor.start.column;
    }
    if (scroll_x + ww / 2 > size) {
      scroll_x = size - ww / 2;
    }
    if (scroll_x < 0) {
      scroll_x = 0;
    }
  }

  endwin();

  for (auto k : outputs) {
    printf(">%s\n", k.c_str());
  }
  for (auto k : doc.outputs) {
    printf(">%s\n", k.c_str());
  }

  // printf("%s\n", doc.buffer.get_dot_graph().c_str());
  return 0;
}
