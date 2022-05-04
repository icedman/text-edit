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

Document doc;

void drawLine(int screen_row, int row, const char *text) {
  move(screen_row, 0);
  clrtoeol();

  int l = strlen(text);

  std::vector<Cursor> &cursors = doc.cursors;

  for (int i = 0; i < l; i++) {
    bool rev = false;
    for (auto cursor : cursors) {
      if (cursor.normalized().is_within(row, i)) {
        rev = true;
        if (cursor.has_selection() && row == cursor.start.row &&
            i == cursor.start.column) {
          rev = false;
          attron(A_UNDERLINE);
          break;
        }
        break;
      }
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

  TextBuffer &text = doc.buffer;
  text.set_text(str);
  doc.buffer.flush_changes();
  doc.snap();

  std::vector<Cursor> &cursors = doc.cursors;

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

  bool running = true;
  while (running) {
    int size = text.extent().row;
    get_dimensions();
    drawText(text, scroll_y);

    Cursor cursor = doc.cursor();

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
      doc.undo();
      continue;
    }

    if (keySequence == "ctrl+up") {
      doc.add_cursor(doc.cursor());
      doc.cursor().move_up();
      continue;
    }
    if (keySequence == "ctrl+down") {
      doc.add_cursor(doc.cursor());
      doc.cursor().move_down();
      continue;
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

  // printf("%s\n", doc.buffer.get_dot_graph().c_str());
  return 0;
}
