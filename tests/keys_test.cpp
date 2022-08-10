#include "input.h"
#include "keybindings.h"

#include <curses.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <string>

int main(int argc, char **argv) {
  bool running = true;
  
  initscr();
  raw();
  // cbreak();
  keypad(stdscr, true);
  noecho();
  nodelay(stdscr, true);

  // std::string keySequence;
  //  
  // int idx = 0;
  // char tmp[32];
  // while(running) {
  //   int c = getch();
  //   if (c != -1) {
  //     clrtoeol();
  //     sprintf(tmp, "%c %x %d %c %s\n", c, c, (c & 0x1f) == c, (c & 0x1f), keyname(c));
  //     addstr(tmp);
  //     move(idx++,0);
  //     if (idx > 10) idx = 0;
  //     if (c == 'q') running = false;
  //   }
  // }

  // int ch = -1;
  // std::string keySequence;
  // while (running) {
  //   ch = readKey(keySequence);
  //   if (ch == -1) continue;
  //   if (ch == 27) break;
  //   move(0,0);
  //   clrtoeol();

  //   char tmp[32];
  //   sprintf(tmp, "%d %c %s", ch, ch, keySequence.c_str());
  //   addstr(tmp);
  //   refresh();
  // }
  
  while (running) {
    if (kbhit(500) != 0) {
      char seq[4] = {0, 0, 0, 0};
      read(STDIN_FILENO, &seq[0], 4);
      if (seq[0] == 'q') {
        running = false;
      }
      for (int i = 0; i < 4; i++) {
        if (seq[i] == 0)
          break;
        char tmp[32];
        sprintf(tmp, ">%d %c %d", seq[i], seq[i], (seq[i] & 0x1f) == seq[i]);
        move(i, 0);
        clrtoeol();
        addstr(tmp);
        move(i+1,0);
        clrtoeol();
        refresh();
      }
    }
  }

  endwin();
  return 0;
}