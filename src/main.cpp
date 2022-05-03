#include <stdio.h>
#include <core/text-buffer.h>
#include <core/text.h>
#include <core/encoding-conversion.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <cstring>

#include <time.h>
#include <curses.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "input.h"
#include "utf8.h"

int width = 0;
int height = 0;

#define TIMER_BEGIN                                                            \
  clock_t start, end;                                                          \
  double cpu_time_used;                                                        \
  start = clock();

#define TIMER_RESET                                                            \
  start = clock();
  
#define TIMER_END                                                              \
  end = clock();                                                               \
  cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

int scroll_y = 0;
int cur_x = 0;
int cur_y = 0;

void drawLine(int r, const char* text) {
    move(r, 0);
    clrtoeol();
    
    int l = strlen(text);
    for(int i=0; i<l; i++) {
      bool u = r == (cur_y - scroll_y) && i == cur_x;
      if (u) {
        attron(A_REVERSE);
      }
      addch(text[i]);
      if (u) {
        attroff(A_REVERSE);
      }
    }
}

void drawText(TextBuffer &text, int scroll_y)
{
    int lines = text.size();
    for(int i=0;i<lines && i<height;i++) {
        optional<uint32_t> l = text.line_length_for_row(scroll_y + i);
        int line = l ? *l : 0;
        optional<std::u16string> row = text.line_for_row(scroll_y + i);
        if (row) {
            std::string s = u16string_to_utf8string(*row) + " ";
            // printf("index:%d\n", doc.getIndexForLine(i));
            // printf("Line %d, %s\n", i, s.c_str());
            drawLine(i, s.c_str());
            // drawLine(i, (wchar_t*)(*row->c_str()));
        } else {
            drawLine(i, "");
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
    while(running) {
        int size = text.extent().row;
        get_dimensions();
        drawText(text, scroll_y);
        
        std::stringstream status;
        status << "line ";
        status << cur_y;
        status << " col ";
        status << cur_x;
        status << " layers:";
        status << text.layer_count();
        drawLine(height - 1, status.str().c_str());
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
            cur_x = c.old_start.column;
            cur_y = c.old_start.row;
          }
        }

        // if (keySequence == "up") start --;
        // if (keySequence == "down") start ++;
        if (keySequence == "pageup") cur_y -= height;
        if (keySequence == "pagedown") cur_y += height;
        
        if (keySequence == "up") {
          cur_y--;
        }
        if (keySequence == "down") {
          cur_y++;
        }
        if (keySequence == "left") {
          cur_x--;
          if (cur_x < 0) {
            cur_y --;
            cur_x = *text.line_length_for_row(cur_y);
          }
        }
        if (keySequence == "right") {
          cur_x++;
          if (cur_x > *text.line_length_for_row(cur_y)) {
            cur_y++;
            cur_x = 0;
          }
        }
        
        if (cur_y < 0) cur_y = 0;
        if (cur_y >= size) cur_y = size-1;
        if (cur_y > scroll_y + height - 4) {
          scroll_y = cur_y - height + 4;
        }
        if (cur_y - scroll_y - 4 < 0) {
          scroll_y = cur_y - 4;
        }
        if (scroll_y + height/2 > size) scroll_y = size - height/2;
        if (scroll_y < 0) scroll_y = 0;

        if (keySequence == "enter") {
          keySequence = "";
          ch = '\n';
        }
                
        if (keySequence == "" && ch != -1) {
            Range range = Range({Point(cur_y, cur_x), Point(cur_y, cur_x)});
            std::u16string k = u"x";
            k[0] = ch;
            text.set_text_in_range(range, k.data());
            cur_x++;
            if (ch == '\n') {
              cur_x = 0;
              cur_y++;
            }
        }
        
        if (keySequence == "backspace") {
          if (cur_x == 0 && cur_y > 0) {
            cur_y--;
            cur_x = *text.line_length_for_row(cur_y);
            Range range= Range({Point(cur_y, cur_x), Point(cur_y+1, 0)});
            text.set_text_in_range(range, u"");
          } else if (cur_x > 0) {
            cur_x--;
            keySequence = "delete";
          }
        }        
        if (keySequence == "delete") {
            Range range = Range({Point(cur_y, cur_x), Point(cur_y, cur_x+1)});
            if (cur_x == *text.line_length_for_row(cur_y)) {
                range= Range({Point(cur_y, cur_x), Point(cur_y+1, 0)});
            }
            text.set_text_in_range(range, u"");
        }
    }

    endwin();

    for(auto k : outputs) {
        printf(">%s\n", k.c_str());
    }
    
    delete snapshot1;
    return 0;
}
