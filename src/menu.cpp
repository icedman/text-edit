#include "menu.h"

menu_t::menu_t() : view_t() {}

bool menu_t::on_idle(int frame) { return false; }

bool menu_t::on_input(int ch, std::string key_sequence) {

  if (key_sequence == "up") {
    if (cursor.y > 0) {
      cursor.y--;
    }
    return true;
  }
  if (key_sequence == "down") {
    if (cursor.y + 1 < items.size()) {
      cursor.y++;
    }
    return true;
  }
  if (key_sequence == "enter" || key_sequence == "tab") {
    if (on_submit) {
      return on_submit(cursor.y);
    }
    return true;
  }
  if (key_sequence == "left" || key_sequence == "right") {
    if (on_cancel) {
      return on_cancel();
    }
    return true;
  }

  return false;
}

void menu_t::update_scroll() {
  if (cursor.y - scroll.y >= frame.h) {
    scroll.y = cursor.y - frame.h;
  }
  if (cursor.y - scroll.y < 0) {
    scroll.y = cursor.y;
  }
}
