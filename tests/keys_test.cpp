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
  initscr();
  raw();
  keypad(stdscr, true);
  noecho();
  nodelay(stdscr, true);

  bool running = true;
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

  while(running) {
    if (kbhit(500) != 0) {
      char seq[4] = { 0,0,0,0 };
      read(STDIN_FILENO, &seq[0], 4);
      if (seq[0] == 'q') {
        running = false;
      }
      for(int i=0; i<4; i++) {
        if (seq[i] == 0) break;
        char tmp[32];
        sprintf(tmp, ">%d %c", seq[i], seq[i]);
        move(i, 0);
        clrtoeol();
        addstr(tmp);
        refresh();
      }
    }
  }

  endwin();
  return 0;
}