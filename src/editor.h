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
  void on_draw();
  void update_scroll();

  DocumentPtr doc;

  bool request_treesitter;
  bool request_autocomplete;
  bool wrap;
  bool draw_tab_stops;

  std::string last_key_sequence;
};

typedef std::shared_ptr<editor_t> EditorPtr;

struct textfield_t : editor_t {
  textfield_t();
  bool on_input(int ch, std::string key_sequence);

  std::function<bool(std::u16string)> on_submit;
};

typedef std::shared_ptr<textfield_t> TextFieldPtr;

#endif // TE_EDITOR_H