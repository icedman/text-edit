#include "ui.h"

status_bar_t::status_bar_t() {
  frame.h = 1;
  status_item_ptr message = std::make_shared<status_item_t>();
  message->align = -1;
  message->flex = 3;
  add_child(message);
  items["message"] = message;

  status_item_ptr info = std::make_shared<status_item_t>();
  info->align = 1;
  info->flex = 2;
  add_child(info);
  items["info"] = info;
}

goto_t::goto_t() : status_line_t() {
  frame.h = 1;

  status_item_ptr title = std::make_shared<status_item_t>();
  add_child(title);
  items["title"] = title;
  title->text = "go to:";
  title->frame.w = title->text.size() + 2;

  input = std::make_shared<textfield_t>();
  input->flex = 1;
  add_child(input);
}

find_t::find_t() : status_line_t(), enable_replace(false) {
  frame.h = 1;

  title = std::make_shared<status_item_t>();
  add_child(title);
  items["title"] = title;
  title->text = "search:";
  title->frame.w = title->text.size() + 2;
  input = std::make_shared<textfield_t>();
  input->flex = 1;
  add_child(input);

  replace_title = std::make_shared<status_item_t>();
  add_child(replace_title);
  items["replace"] = replace_title;
  replace_title->text = "replace:";
  replace_title->frame.w = replace_title->text.size() + 2;
  replace = std::make_shared<textfield_t>();
  replace->flex = 1;
  add_child(replace);

  // status_item_ptr result = std::make_shared<status_item_t>();
  // result->frame.w = 20;
  // add_child(result);
  // items["result"] = result;
}