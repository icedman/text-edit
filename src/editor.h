#ifndef TE_EDITOR_H
#define TE_EDITOR_H

#include "document.h"
#include "view.h"

#include <functional>
#include <memory>
#include <string>

struct editor_t : view_t {
  editor_t();

  bool on_idle(int frame);
  bool on_input(int ch, std::string key_sequence);
  void update_scroll();

  DocumentPtr doc;

  bool request_treesitter;
  bool request_autocomplete;
  bool wrap;
  bool draw_tab_stops;

  std::string last_key_sequence;
};

typedef std::shared_ptr<editor_t> editor_ptr;

struct textfield_t : editor_t {
  textfield_t();
  bool on_input(int ch, std::string key_sequence);

  std::function<bool(std::u16string)> on_submit;
};

typedef std::shared_ptr<textfield_t> textfield_ptr;

struct editors_t {
  std::vector<editor_ptr> editors;
  int selected;

  editors_t();
  editor_ptr add_editor(std::string path);
  editor_ptr current_editor();
  void close_current_editor();
};

#endif // TE_EDITOR_H