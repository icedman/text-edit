#ifndef TE_MENU_H
#define TE_MENU_H

#include "view.h"
#include <string>

struct menu_t : view_t {
  menu_t();

  bool on_idle(int frame);
  bool on_input(int ch, std::string key_sequence);
  void on_draw();
  void update_scroll();
};

#endif // TE_MENU_H