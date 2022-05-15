#ifndef TE_MENU_H
#define TE_MENU_H

#include "view.h"
#include <functional>
#include <string>

struct menu_item_t {
  std::string name;
};

struct menu_t : view_t {
  menu_t();

  bool on_idle(int frame);
  bool on_input(int ch, std::string key_sequence);
  void update_scroll();

  std::vector<menu_item_t> items;

  std::function<bool(int)> on_submit;
  std::function<bool()> on_cancel;
};

typedef std::shared_ptr<menu_t> MenuPtr;

#endif // TE_MENU_H