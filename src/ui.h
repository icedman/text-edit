#ifndef TE_UI_H
#define TE_UI_H

#include "editor.h"
#include "view.h"

#include <map>
#include <memory>
#include <string>

struct status_item_t : view_t {
  status_item_t() : align(0), color(0) {}
  std::string text;
  int align;
  int color;
};
typedef std::shared_ptr<status_item_t> status_item_ptr;

struct status_line_t : row_t {
  status_line_t() : row_t() {}
  std::map<std::string, status_item_ptr> items;
};

struct status_bar_t : status_line_t {
  status_bar_t();
};

typedef std::shared_ptr<status_bar_t> status_bar_ptr;
struct goto_t : status_line_t {
  goto_t();
  textfield_ptr input;
};

typedef std::shared_ptr<goto_t> goto_ptr;

struct find_t : status_line_t {
  find_t();
  textfield_ptr input;
  textfield_ptr replace;
  status_item_ptr title;
  status_item_ptr replace_title;
  bool enable_replace;
};

typedef std::shared_ptr<find_t> find_ptr;

struct cmd_line_t : status_line_t {
  cmd_line_t();
  textfield_ptr input;

  std::vector<std::u16string> history;
  int selected;

  void select_history();
};

typedef std::shared_ptr<cmd_line_t> cmd_line_ptr;

#endif // TE_UI_H