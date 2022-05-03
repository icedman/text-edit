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

#include "input.h"
#include "utf8.h"

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

int scroll_y = 0;

class Cursor : public Range {
public:
  TextBuffer *buffer;

  void move_up(bool anchor = false) {
    if (start.row > 0) {
      start.row--;
    }
    if (!anchor) {
      end = start;
    }
  }

  void move_down(bool anchor = false) {
    start.row++;
    int size = buffer->extent().row;
    if (start.row >= size) {
      start.row = size - 1;
    }
    if (!anchor) {
      end = start;
    }
  }

  void move_left(bool anchor = false) {
    if (start.column == 0) {
      if (start.row > 0) {
        start.row--;
        start.column = *(*buffer).line_length_for_row(start.row);
      }
    } else {
      start.column--;
    }

    if (!anchor) {
      end = start;
    }
  }

  void move_right(bool anchor = false) {
    start.column++;
    if (start.column > *(*buffer).line_length_for_row(start.row)) {
      start.row++;
      start.column = 0;
    }
    if (!anchor) {
      end = start;
    }
  }

  Cursor copy() { return Cursor{start, end}; }

  Cursor normalized() {
    Cursor c = copy();
    bool flip = false;
    if (start.row > end.row ||
        (start.row == end.row && start.column > end.column)) {
      flip = true;
    }
    if (flip) {
      c.end = start;
      c.start = end;
    }
    return c;
  }

  bool has_selection() { return start != end; }

  void clear_selection() {
    Cursor n = normalized();
    start = n.start;
    end = start;
  }

  bool is_within(int row, int column) {
    Cursor c = normalized();
    if (row < c.start.row || (row == c.start.row && column < c.start.column))
      return false;
    if (row > c.end.row || (row == c.end.row && column > c.end.column))
      return false;
    return true;
  }
};

Cursor cursor;

void drawLine(int screen_row, int row, const char *text) {
  move(screen_row, 0);
  clrtoeol();

  int cx = cursor.start.column;
  int cy = cursor.start.row - scroll_y;
  int l = strlen(text);
  if (cx > l)
    cx = l;

  for (int i = 0; i < l; i++) {
    bool rev = cursor.normalized().is_within(row, i);
    if (cursor.has_selection() && row == cursor.start.row &&
        i == cursor.start.column) {
      rev = false;
      attron(A_UNDERLINE);
    }
    if (rev) {
      attron(A_REVERSE);
    }
    addch(text[i]);
    attroff(A_REVERSE);
    attroff(A_UNDERLINE);
  }
}

void drawText(TextBuffer &text, int scroll_y) {
  int lines = text.size();
  for (int i = 0; i < lines && i < height; i++) {
    optional<uint32_t> l = text.line_length_for_row(scroll_y + i);
    int line = l ? *l : 0;
    optional<std::u16string> row = text.line_for_row(scroll_y + i);
    if (row) {
      std::stringstream s;
      // s << (scroll_y + i);
      // s << " ";
      s << u16string_to_utf8string(*row);
      s << " ";
      // printf("index:%d\n", doc.getIndexForLine(i));
      // printf("Line %d, %s\n", i, s.c_str());
      drawLine(i, scroll_y + i, s.str().c_str());
      // drawLine(i, (wchar_t*)(*row->c_str()));
    } else {
      drawLine(i, 0, "");
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
  std::ifstream t("./tests/tinywl.c");
  // std::ifstream t("./tests/sqlite3.c");
  std::stringstream buffer;
  buffer << t.rdbuf();

  std::u16string str;

  // TIMER_BEGIN
  optional<EncodingConversion> enc = transcoding_from("UTF-8");
  (*enc).decode(str, buffer.str().c_str(), buffer.str().size());
  // str = utf8string_to_u16string(buffer.str());
  // TIMER_END
  // printf("time: %f\n------\n", cpu_time_used);

  TextBuffer text(str);
  cursor.buffer = &text;

  setlocale(LC_ALL, "");

  initscr();
  raw();
  keypad(stdscr, true);
  noecho();
  nodelay(stdscr, true);

  use_default_colors();
  start_color();

  curs_set(0);
  clear();

  std::vector<std::string> outputs;
  // std::vector<Patch::change> patches;

  auto snapshot1 = text.create_snapshot();

  bool running = true;
  while (running) {
    int size = text.extent().row;
    get_dimensions();
    drawText(text, scroll_y);

    std::stringstream status;
    status << "line ";
    status << cursor.start.row;
    status << " col ";
    status << cursor.start.column;
    status << " scroll ";
    status << scroll_y;
    // status << " -> line ";
    // status << cursor.end.row;
    // status << " col ";
    // status << cursor.end.column;
    // status << " layers:";
    // status << text.layer_count();
    drawLine(height - 1, 0, status.str().c_str());
    refresh();

    int ch = -1;
    std::string keySequence;
    while (running) {
      ch = readKey(keySequence);
      if (ch != -1) {
        break;
      }
    }
    if (ch == 27 || keySequence == "ctrl+q") {
      running = false;
    }

    if (keySequence != "") {
      outputs.push_back(keySequence);
    }

    if (keySequence == "ctrl+z") {
      // text.flush_changes();
      auto patch = text.get_inverted_changes(snapshot1);
      std::vector<Patch::Change> changes = patch.get_changes();
      if (changes.size() > 0) {
        Patch::Change c = changes.back();
        Range range = Range({c.old_start, c.old_end});
        text.set_text_in_range(range, c.new_text->content.data());
        cursor.start = c.old_start;
        cursor.end = c.old_start;
      }
    }

    if (keySequence == "ctrl+e") {
      auto patch = text.get_inverted_changes(snapshot1);
      std::vector<Patch::Change> changes = patch.get_changes();
      for (auto c : changes) {
        std::string s = u16string_to_utf8string(c.old_text->content);
        outputs.push_back(s);
      }
    }

    if (keySequence == "ctrl+up") {
      int r = -height + cursor.start.row;
      if (r < 0) {
        r = 0;
      }
      cursor.start.row = r;
    }
    if (keySequence == "pagedown") {
      cursor.start.row += height;
      cursor.move_down();
    }

    bool anchor = false;
    if (keySequence.starts_with("shift+")) {
      anchor = true;
      keySequence = keySequence.substr(6);
    }

    if (keySequence == "up") {
      cursor.move_up(anchor);
    }
    if (keySequence == "down") {
      cursor.move_down(anchor);
    }
    if (keySequence == "left") {
      cursor.move_left(anchor);
    }
    if (keySequence == "right") {
      cursor.move_right(anchor);
    }

    if (cursor.start.row >= scroll_y + height - 4) {
      scroll_y = -height + cursor.start.row + 4;
    }
    if (scroll_y + 4 > cursor.start.row) {
      scroll_y = -4 + cursor.start.row;
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
      Range range = cursor.normalized();
      std::u16string k = u"x";
      k[0] = ch;
      text.set_text_in_range(range, k.data());
      cursor.clear_selection();
      cursor.move_right();
      if (ch == '\n') {
        cursor.start.column = 0;
      }
    }

    // delete
    if (keySequence == "backspace") {
      if (cursor.has_selection()) {
        keySequence = "delete";
      } else {
        if (cursor.start.column == 0 && cursor.start.row > 0) {
          cursor.move_up();
          cursor.start.column = *text.line_length_for_row(cursor.start.row);
          Range range = Range({Point(cursor.start.row, cursor.start.column),
                               Point(cursor.start.row + 1, 0)});
          text.set_text_in_range(range, u"");
          cursor.end = cursor.start;
        } else if (cursor.start.column > 0) {
          cursor.move_left();
          keySequence = "delete";
        }
      }
    }
    if (keySequence == "delete") {
      Cursor cur = cursor.normalized();
      if (!cursor.has_selection()) {
        cur.end.column++;
        if (cur.end.column > *text.line_length_for_row(cur.start.row)) {
          cur.end.column = 0;
          cur.end.row++;
        }
      }
      text.set_text_in_range(cur, u"");
      cursor.clear_selection();
    }
  }

  endwin();

  for (auto k : outputs) {
    printf(">%s\n", k.c_str());
  }

  delete snapshot1;
  return 0;
}
