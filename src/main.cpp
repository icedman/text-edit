#include <core/encoding-conversion.h>
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
#include "textmate.h"
#include "theme.h"
#include "utf8.h"

std::vector<std::string> outputs;

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

Document doc;

#define SELECTED_OFFSET 500
enum color_pair_e { NORMAL = 0, SELECTED };
static std::map<int, int> colorMap;

int color_index(int r, int g, int b) {
  return color_info_t::nearest_color_index(r, g, b);
}

int pair_for_color(int colorIdx, bool selected) {
  if (selected && colorIdx == color_pair_e::NORMAL) {
    return color_pair_e::SELECTED;
  }
  return colorMap[colorIdx + (selected ? SELECTED_OFFSET : 0)];
}

int fg = 0;
int bg = 0;
int sel = 0;

void update_colors() {
  colorMap.clear();

  theme_info_t info = Textmate::theme_info();

  fg = info.fg_a; // color_index(info.fg_r, info.fg_g, info.fg_b);
  bg = COLOR_BLACK; // info.bg_a; // color_index(info.bg_r, info.bg_g, info.bg_b);
  sel = color_index(info.sel_r, info.sel_g, info.sel_b);

  theme_ptr theme = Textmate::theme();

  //---------------
  // build the color pairs
  //---------------
  init_pair(color_pair_e::NORMAL, fg, bg);
  init_pair(color_pair_e::SELECTED, fg, sel);

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
}

void draw_text(int screen_row, const char *text) {
  move(screen_row, 0);
  clrtoeol();
  addstr(text);
}

void draw_text_line(int screen_row, int row, const char *text,
                    std::vector<textstyle_t> &styles) {
  move(screen_row, 0);
  clrtoeol();

  int l = strlen(text);

  doc.cursor(); // ensure 1 cursor
  std::vector<Cursor> &cursors = doc.cursors;

  int default_pair = pair_for_color(fg, false);
  for (int i = 0; i < l; i++) {
    int pair = default_pair;
    bool selected = false;

    // build...
    for (auto cursor : cursors) {
      if (cursor.is_within(row, i)) {
        selected = true;
        if (cursor.has_selection() && cursor.is_normalized()) {
          if (row == cursor.end.row &&
            i == cursor.end.column) {
            selected = false;
          }
        }
        if (cursor.has_selection() && row == cursor.start.row &&
            i == cursor.start.column) {
          if (!cursor.is_normalized()) {
            selected = false;
          }
          attron(A_REVERSE);
          break;
        }
        break;
      }
    }

    char ch = text[i];

    for (auto s : styles) {
      if (s.start >= i && i < s.start + s.length) {
        int colorIdx = color_index(s.r, s.g, s.b);
        pair = pair_for_color(colorIdx, selected);
        // std::stringstream ss;
        // ss << pair;
        // outputs.push_back(ss.str());
        pair = pair > 0 ? pair : default_pair;
        break;
      }
    }

    attron(COLOR_PAIR(pair));
    addch(ch);
    attroff(COLOR_PAIR(pair));
    attroff(A_REVERSE);
    attroff(A_UNDERLINE);
  }
}

void draw_text_buffer(TextBuffer &text, int scroll_y) {
  int view_start = scroll_y;
  int view_end = scroll_y + height;

  // highlight
  int idx = 0;
  int start = scroll_y - height / 2;
  if (start < 0)
    start = 0;
  for (int i = 0; i < (height * 1.5); i++) {
    int line = start + i;
    BlockPtr block = doc.block_at(line);
    if (!block)
      continue;

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
      draw_text_line(idx++, line, s.str().c_str(), block->styles);
    }
  }
}

void get_dimensions() {
  static struct winsize ws;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  width = ws.ws_col;
  height = ws.ws_row;
}

int main(int argc, char **argv) {
  std::string file_path = "./tests/main.cpp";
  if (argc > 1) {
    file_path = argv[argc - 1];
  }

  Textmate::initialize("/home/iceman/.editor/extensions/");
  // theme_id = Textmate::load_theme("default");
  theme_id = Textmate::load_theme("/home/iceman/.editor/extensions/dracula-theme.theme-dracula-2.24.2/theme/dracula.json");
  lang_id = Textmate::load_language(file_path);

  theme_info_t info = Textmate::theme_info();

  std::ifstream t(file_path);
  std::stringstream buffer;
  buffer << t.rdbuf();

  std::u16string str;

  // TIMER_BEGIN
  optional<EncodingConversion> enc = transcoding_from("UTF-8");
  (*enc).decode(str, buffer.str().c_str(), buffer.str().size());
  // str = string_to_u16string(buffer.str());
  // TIMER_END
  // printf("time: %f\n------\n", cpu_time_used);

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

  // curs_set(0);
  clear();

  // std::vector<Patch::change> patches;

  bool running = true;

  while (running) {
    int size = text.extent().row;
    get_dimensions();
    draw_text_buffer(text, scroll_y);

    Cursor cursor = doc.cursor();

    std::stringstream status;
    // status << "theme ";
    // status << theme_id;
    // status << " lang ";
    // status << lang_id;
    status << " line ";
    status << (cursor.start.row + 1);
    status << " col ";
    status << (cursor.start.column + 1);
    // status << " scroll ";
    // status << scroll_y;
    // status << " -> line ";
    // status << cursor.end.row;
    // status << " col ";
    // status << cursor.end.column;
    // status << " layers:";
    // status << text.layer_count();
    draw_text(height - 1, status.str().c_str());
    move(cursor.start.row, cursor.start.column);
    refresh();

    int ch = -1;
    std::string keySequence;
    while (running) {
      ch = readKey(keySequence);
      if (ch != -1) {
        break;
      }
    }
    if (keySequence == "ctrl+q") {
      running = false;
    }

    if (ch == 27) {
      doc.clear_cursors();
      ch = -1;
    }

    // if (keySequence != "") {
    //   outputs.push_back(keySequence);
    // }

    if (keySequence == "ctrl+z") {
      doc.undo();
    }

    if (keySequence == "ctrl+up") {
      doc.add_cursor(doc.cursor());
      doc.cursor().move_up();
    }
    if (keySequence == "ctrl+down") {
      doc.add_cursor(doc.cursor());
      doc.cursor().move_down();
    }

    bool anchor = false;
    if (keySequence.starts_with("shift+")) {
      anchor = true;
      keySequence = keySequence.substr(6);
    }

    if (keySequence == "up") {
      doc.move_up(anchor);
    }
    if (keySequence == "down") {
      doc.move_down(anchor);
    }
    if (keySequence == "left") {
      doc.move_left(anchor);
    }
    if (keySequence == "right") {
      doc.move_right(anchor);
    }

    cursor = doc.cursor();

    int lead = 0;
    if (cursor.start.row >= scroll_y + (height - 2) - lead) {
      scroll_y = -(height - 2) + cursor.start.row + lead;
    }
    if (scroll_y + lead > cursor.start.row) {
      scroll_y = -lead + cursor.start.row;
    }
    if (scroll_y + height / 2 > size) {
      scroll_y = size - height / 2;
    }
    if (scroll_y < 0) {
      scroll_y = 0;
    }

    if (keySequence == "enter") {
      keySequence = "";
      ch = '\n';
    }

    // input
    if (keySequence == "" && ch != -1) {
      std::u16string text = u"x";
      text[0] = ch;
      doc.insert_text(text);
    }

    // delete
    if (keySequence == "backspace") {
      if (!doc.has_selection()) {
        doc.move_left();
      }
      doc.delete_text();
    }
    if (keySequence == "delete") {
      doc.delete_text();
    }
  }

  endwin();

  for (auto k : outputs) {
    printf(">%s\n", k.c_str());
  }
  for (auto k : doc.outputs) {
    printf(">%s\n", k.c_str());
  }

  printf("colorMap: %ld\n", colorMap.size());
  printf("colorIndices: %ld\n", Textmate::theme()->colorIndices.size());
  printf("color: %d\n", pair_for_color(68, false));

  // printf("%s\n", doc.buffer.get_dot_graph().c_str());
  return 0;
}
