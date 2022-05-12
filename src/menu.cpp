#include "menu.h"

menu_t::menu_t() : view_t() {}

bool menu_t::on_idle(int frame) { return false; }

bool menu_t::on_input(int ch, std::string key_sequence) { return false; }

void menu_t::on_draw() {}

void menu_t::update_scroll() {}
