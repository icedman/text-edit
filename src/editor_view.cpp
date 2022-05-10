#include "editor_view.h"
#include "autocomplete.h"
#include "input.h"
#include "keybindings.h"
#include "treesitter.h"
#include "utf8.h"

editor_t::editor_t()
    : view_t(), request_treesitter(false), request_autocomplete(false) {
  doc = std::make_shared<Document>();
}

bool editor_t::on_input(int ch, std::string key_sequence) {
  int hh = computed.h;

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
  }

  if (cmd.command == "undo") {
    doc->undo();
  }

  if (cmd.command == "copy") {
    doc->copy();
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
  if (cmd.command == "select_line") {
    doc->move_to_start_of_line();
    doc->move_to_end_of_line(true);
  }
  if (cmd.command == "expand_cursor") {
    Cursor cur = doc->cursor().normalized();
    optional<Cursor> block_cursor = doc->block_cursor(cur);
    if (block_cursor) {
      doc->clear_cursors();
      doc->cursor().copy_from(*block_cursor);
    }
  }
  if (cmd.command == "contract_cursor") {
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

  Cursor cursor = doc->cursor();

  if (key_sequence == "enter") {
    key_sequence = "";
    ch = '\n';
  }
  if (key_sequence == "tab") {
    key_sequence = "";
    ch = '\t';
  }

  // input
  if (key_sequence == "" && ch != -1) {
    std::u16string text = u"x";
    text[0] = ch;
    if (ch == '\t') {
      text = doc->tab_string;
    }
    doc->insert_text(text);
    request_autocomplete = true;
    request_treesitter = true;
  }

  if (!request_autocomplete && autocomplete) {
    doc->clear_autocomplete();
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

  int size = doc->size();
  int lead = 0;
  if (cursor.start.row >= scroll.y + (hh - 1) - lead) {
    scroll.y = -(hh - 1) + cursor.start.row + lead;
  }
  if (scroll.y + lead > cursor.start.row) {
    scroll.y = -lead + cursor.start.row;
  }
  if (scroll.y + hh / 2 > size) {
    scroll.y = size - hh / 2;
  }
  if (scroll.y < 0) {
    scroll.y = 0;
  }
  int ww = computed.w;
  if (cursor.start.column >= scroll.x + (ww - 1) - lead) {
    scroll.x = -(ww - 1) + cursor.start.column + lead;
  }
  if (scroll.x + lead > cursor.start.column) {
    scroll.x = -lead + cursor.start.column;
  }
  if (scroll.x + ww / 2 > size) {
    scroll.x = size - ww / 2;
  }
  if (scroll.x < 0) {
    scroll.x = 0;
  }

  return false;
}

bool editor_t::on_idle(int frame) {
  if (frame == 500 && request_treesitter) {
    doc->run_treesitter();
    request_treesitter = false;
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
  if (frame == 750) {
    SearchPtr search = doc->search();
    if (search && search->state != Search::State::Consumed) {
      search->set_consumed();
      return true;
    }
  }
  return false;
}

void editor_t::on_draw() {}

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

  std::vector<std::string> drop_commands = {"save", "indent", "unindent"};

  for (auto d : drop_commands) {
    if (cmd.command == d) {
      return false;
    }
  }

  if (key_sequence == "enter") {
    if (on_submit) {
      return on_submit(doc->buffer.text());
    }
    return false;
  }

  bool res = editor_t::on_input(ch, key_sequence);
  request_autocomplete = false;
  request_treesitter = false;
  return res;
}
