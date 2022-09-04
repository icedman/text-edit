#include "editor.h"
#include "autocomplete.h"
#include "files.h"
#include "input.h"
#include "keybindings.h"
#include "textmate.h"
#include "treesitter.h"
#include "utf8.h"

#include <algorithm>

editor_t::editor_t()
    : view_t(), request_treesitter(false), request_autocomplete(false),
      wrap(true), draw_tab_stops(false) {
  focusable = true;
  doc = std::make_shared<Document>();
  doc->initialize(Document::empty());
}

bool editor_t::on_input(int ch, std::string key_sequence) {
  AutoCompletePtr autocomplete = doc->autocomplete();
  SearchPtr search = doc->search();

  Command &cmd = command_from_keys(key_sequence, last_key_sequence);

  if (cmd.command == "await") {
    last_key_sequence = key_sequence;
    return false;
  }
  last_key_sequence = "";

  if (cmd.command == "save") {
    if (doc->file_path != "") {
      if (doc->save(doc->file_path)) {
        // message = "saved";
      } else {
        // message = "error saving";
      }
    }
  }

  if (cmd.command == "cancel") {
    doc->clear_cursors();
    doc->clear_autocomplete(true);
    doc->clear_search(true);
  }
  if (cmd.command == "undo") {
    doc->undo();
    request_treesitter = true;
  }
  if (cmd.command == "redo") {
    doc->redo();
    request_treesitter = true;
  }
  if (cmd.command == "copy") {
    doc->copy();
  }
  if (cmd.command == "insert") {
    doc->insert_mode = !doc->insert_mode;
  }
  if (cmd.command == "cut") {
    doc->copy();
    doc->delete_text();
  }
  if (cmd.command == "paste") {
    doc->paste();
  }
  if (cmd.command == "select_word") {
    doc->add_cursor_from_selected_word();
  }
  if (cmd.command == "select_all") {
    doc->clear_cursors();
    doc->move_to_start_of_document();
    doc->move_to_end_of_document(true);
  }
  if (cmd.command == "duplicate_selection") {
    doc->duplicate_selection();
  }
  if (cmd.command == "duplicate_line") {
    doc->duplicate_line();
  }
  if (cmd.command == "select_line") {
    doc->move_to_start_of_line();
    doc->move_to_end_of_line(true);
  }
  if (cmd.command == "toggle_wrap") {
    wrap = !wrap;
    doc->make_dirty();
  }
  if (cmd.command == "expand_to_block") {
    // move to doc
    Cursor cur = doc->cursor().normalized();
    cur.move_left();
    cur.move_left();
    optional<Cursor> block_cursor = doc->block_cursor(cur);
    if (block_cursor) {
      doc->clear_cursors();
      doc->cursor().copy_from(*block_cursor);
    }
  }
  if (cmd.command == "toggle_block_fold") {
    doc->toggle_fold(doc->cursor());
  }
  if (cmd.command == "selection_to_uppercase") {
    doc->selection_to_uppercase();
  }
  if (cmd.command == "selection_to_lowercase") {
    doc->selection_to_lowercase();
  }
  if (cmd.command == "indent") {
    doc->indent();
    request_treesitter = true;
  }
  if (cmd.command == "unindent") {
    doc->unindent();
    request_treesitter = true;
  }
  if (cmd.command == "toggle_comment") {
    doc->toggle_comment();
    request_treesitter = true;
  }

  if (cmd.command == "move_up") {
    doc->move_up(cmd.params == "anchored");
  }
  if (cmd.command == "move_down") {
    doc->move_down(cmd.params == "anchored");
  }
  if (cmd.command == "move_left") {
    doc->move_left(cmd.params == "anchored");
  }
  if (cmd.command == "move_right") {
    doc->move_right(cmd.params == "anchored");
  }

  int hh = computed.h;
  if (cmd.command == "pageup") {
    for (int i = 0; i < hh; i++)
      doc->move_up();
  }
  if (cmd.command == "pagedown") {
    for (int i = 0; i < hh; i++)
      doc->move_down();
  }

  if (cmd.command == "move_to_start_of_line") {
    doc->move_to_start_of_line();
  }
  if (cmd.command == "move_to_end_of_line") {
    doc->move_to_end_of_line();
  }

  if (cmd.command == "add_cursor_and_move_up") {
    doc->add_cursor(doc->cursor());
    doc->cursor().move_up();
  }
  if (cmd.command == "add_cursor_and_move_down") {
    doc->add_cursor(doc->cursor());
    doc->cursor().move_down();
  }
  if (cmd.command == "move_to_previous_word") {
    doc->move_to_previous_word();
  }
  if (cmd.command == "move_to_next_word") {
    doc->move_to_next_word();
  }

  // delete
  if (cmd.command == "backspace") {
    if (!doc->has_selection()) {
      doc->move_left();
    }
    doc->delete_text();
    request_treesitter = true;
  }
  if (cmd.command == "delete") {
    doc->delete_text();
    request_treesitter = true;
  }

  // input
  if (key_sequence == "enter") {
    key_sequence = "";
    ch = '\n';
  }
  if (key_sequence == "tab") {
    key_sequence = "";
    ch = '\t';
  }

  if (key_sequence == "" && ch != -1) {
    std::u16string text = u"x";
    text[0] = ch;
    if (ch == '\t') {
      text = doc->tab_string;
    }
    doc->insert_text(text);
    request_autocomplete = true;
    request_treesitter = true;
    doc->on_input(ch);
  }

  if (!request_autocomplete && autocomplete) {
    doc->clear_autocomplete();
  }

  update_scroll();
  return false;
}

bool editor_t::on_idle(int frame) {
  if (frame == 500 && request_treesitter) {
    doc->run_treesitter();
    request_treesitter = false;

    // save undo
    // doc->snapshots.push_back(doc->buffer.create_snapshot());
    return true;
  }
  if (frame == 750 && request_autocomplete) {
    doc->clear_autocomplete(true);
    doc->run_autocomplete();
    request_autocomplete = false;
    return true;
  }
  if (frame == 1500) {
    AutoCompletePtr autocomplete = doc->autocomplete();
    if (autocomplete && autocomplete->state != AutoComplete::State::Consumed) {
      autocomplete->set_consumed();
      return true;
    }
  }
  if (frame == 850) {
    SearchPtr search = doc->search();
    if (search && search->state != Search::State::Consumed) {
      search->set_consumed();
      return true;
    }
  }

  if (frame == 1000) {
    doc->commit_undo();
  }
  return false;
}

void editor_t::update_scroll() {
  int hh = computed.h;

  Cursor cur = doc->cursor();
  point_t cursor = {cur.start.column, cur.start.row};

  // compute wrap
  int offset_y = 0;
  if (wrap) {
    int s = cursor.y - computed.h;
    if (s < 0)
      s = 0;
    for (int i = s; i < cursor.y; i++) {
      BlockPtr block = doc->block_at(i);
      for (auto f : doc->folds) {
        if (is_point_within_range({i, 0}, f)) {
          block = nullptr;
          break;
        }
      }
      if (!block) {
        continue;
      }
      offset_y += (block->line_height - 1);
    }
  }

  // compute fold
  int offset_folds = 0;
  int prev_end = 0;
  for (auto f : doc->folds) {
    if (f.start.row < prev_end)
      continue;
    if (cursor.y > f.start.row) {
      offset_folds += (f.end.row - f.start.row);
    }
    prev_end = f.end.row;
  }

  // compute the scroll
  int size = doc->size();
  int lead = 0;
  if (cursor.y >= scroll.y + (hh - 1) - lead - offset_y + offset_folds) {
    scroll.y = -(hh - 1) + cursor.y + lead + offset_y - offset_folds;
  }
  if (scroll.y + lead + offset_folds > cursor.y) {
    scroll.y = -lead + cursor.y - offset_folds;
  }
  if (scroll.y + hh / 2 > size) {
    scroll.y = size - hh / 2;
  }
  if (scroll.y < 0) {
    scroll.y = 0;
  }
  int ww = computed.w;
  if (cursor.x >= scroll.x + (ww - 1) - lead) {
    scroll.x = -(ww - 1) + cursor.x + lead;
  }
  if (scroll.x + lead > cursor.x) {
    scroll.x = -lead + cursor.x;
  }
  if (scroll.x + ww / 2 > size) {
    scroll.x = size - ww / 2;
  }
  if (scroll.x < 0) {
    scroll.x = 0;
  }
  // wrapped
  if (wrap) {
    scroll.x = 0;
  }
}

textfield_t::textfield_t() : editor_t() {
  std::u16string empty = u"";
  doc->initialize(empty);
}

bool textfield_t::on_input(int ch, std::string key_sequence) {
  std::vector<std::string> key_sequences = {"tab"};

  for (auto d : key_sequences) {
    if (key_sequence == d) {
      return false;
    }
  }

  Command &cmd = command_from_keys(key_sequence, last_key_sequence);

  if (cmd.command == "await") {
    last_key_sequence = key_sequence;
    return false;
  }
  last_key_sequence = "";

  std::vector<std::string> drop_commands = {
      "save", "indent", "unindent", "toggle_block_fold", "toggle_wrap", "tab"};

  for (auto d : drop_commands) {
    if (cmd.command == d) {
      return false;
    }
  }

  if (key_sequence == "enter") {
    if (on_submit) {
      // std::u16string value = doc->buffer.text();
      optional<std::u16string> row = doc->buffer.line_for_row(0);
      if (row) {
        return on_submit(*row);
      }
    }
    return false;
  }

  bool res = editor_t::on_input(ch, key_sequence);
  request_autocomplete = false;
  request_treesitter = false;
  return res;
}

editors_t::editors_t() : selected(0) {}

editor_ptr editors_t::add_editor(std::string path) {
  // find existing
  std::string fp = expanded_path(path);
  for (auto c : editors) {
    if (c->doc->file_path == fp) {
      return c;
    }
  }

  editor_ptr e = std::make_shared<editor_t>();
  selected = editors.size();
  editors.push_back(e);

  e->draw_tab_stops = true;
  e->request_treesitter = true;

  DocumentPtr doc = e->doc;
  doc->load(path);
  int lang_id = Textmate::load_language(path);
  if (lang_id != -1) {
    doc->set_language(Textmate::language_info(lang_id));
  }
  return e;
}

editor_ptr editors_t::current_editor() {
  if (!editors.size()) {
    return nullptr;
  }
  for (auto e : editors) {
    e->show = false;
  }
  editors[selected]->show = true;
  return editors[selected];
}

void editors_t::close_current_editor() {
  auto it = std::find(editors.begin(), editors.end(), current_editor());
  if (it != editors.end()) {
    editors.erase(it);
  }
  if (selected >= editors.size()) {
    selected = editors.size() - 1;
  }
}