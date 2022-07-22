#include "menu.h"
#include <map>

menu_t::menu_t() : view_t(), selected(0) {}

bool menu_t::on_idle(int frame) { return false; }

bool menu_t::on_input(int ch, std::string key_sequence) {

  if (key_sequence == "up") {
    if (selected > 0) {
      selected--;
      if (on_change) {
        return on_change(selected);
      }
    }
    return true;
  }
  if (key_sequence == "down") {
    if (selected + 1 < items.size()) {
      selected++;
      if (on_change) {
        return on_change(selected);
      }
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

bool tabs_t::on_input(int ch, std::string key_sequence) {
  static std::map<std::string, std::string> remap = {
      {"up", "left"},
      {"down", "right"},
      {"left", "up"},
      {"right", "down"},
  };
  if (key_sequence == "down" || key_sequence == "up" ||
      key_sequence == "enter" || key_sequence == "tab") {
    view_t::input_focus = nullptr;
  }
  if (remap.find(key_sequence) != remap.end()) {
    key_sequence = remap[key_sequence];
  }
  return menu_t::on_input(ch, key_sequence);
}