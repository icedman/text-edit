#include "menu.h"

menu_t::menu_t() : view_t(), selected(0) {}

bool menu_t::on_idle(int frame) { return false; }

bool menu_t::on_input(int ch, std::string key_sequence) {

  if (key_sequence == "up") {
    if (selected > 0) {
      selected--;
    }
    return true;
  }
  if (key_sequence == "down") {
    if (selected + 1 < items.size()) {
      selected++;
    }
    return true;
  }
  if (key_sequence == "enter" || key_sequence == "tab") {
    if (on_submit) {
      return on_submit(selected);
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
  int h = frame.h;
  if (h == 0) {
    h = computed.h;
  }
  if (selected - scroll.y >= h) {
    scroll.y = selected - h;
  }
  if (selected - scroll.y < 0) {
    scroll.y = selected;
  }
}
