#include "document.h"
#include "utf8.h"

#include <sstream>

Document::Document() : snapshot(0) {}

Document::~Document() {
  if (snapshot) {
    delete snapshot;
  }
}

void Document::snap() {
  if (snapshot) {
    delete snapshot;
  }
  snapshot = buffer.create_snapshot();
}

Cursor &Document::cursor() {
  if (cursors.size() == 0) {
    cursors.push_back(Cursor{Point(0, 0), Point(0, 0), &buffer, this});
  }
  return cursors.back();
}

void Document::move_up(bool anchor) {
  for (auto &c : cursors) {
    c.move_up(anchor);
  }
}

void Document::move_down(bool anchor) {
  for (auto &c : cursors) {
    c.move_down(anchor);
  }
}

void Document::move_left(bool anchor) {
  for (auto &c : cursors) {
    c.move_left(anchor);
  }
}

void Document::move_right(bool anchor) {
  for (auto &c : cursors) {
    c.move_right(anchor);
  }
}

void Document::insert_text(std::u16string text) {
  for (auto &c : cursors) {
    begin_cursor_markers();
    c.insert_text(text);
    end_cursor_markers(c.id);
  }
}

void Document::delete_text(int number_of_characters) {
  for (auto &c : cursors) {
    begin_cursor_markers();
    c.delete_text(number_of_characters);
    end_cursor_markers(c.id);
  }
}

bool Document::has_selection() {
  for (auto &c : cursors) {
    if (c.has_selection())
      return true;
  }
  return false;
}

void Document::clear_selection() {
  for (auto &c : cursors) {
    c.clear_selection();
  }
}

void Document::clear_cursors() {
  Cursor c = cursor();
  c.clear_selection();
  cursors.clear();
  cursors.push_back(c);
}

void Document::add_cursor(Cursor cursor) { cursors.push_back(cursor.copy()); }

void Document::undo() {
  if (!snapshot)
    return;
  clear_cursors();

  // buffer.flush_changes();
  auto patch = buffer.get_inverted_changes(snapshot);
  std::vector<Patch::Change> changes = patch.get_changes();
  if (changes.size() > 0) {
    Patch::Change c = changes.back();
    Range range = Range({c.old_start, c.old_end});

    std::stringstream s;
    s << "---";
    s << c;
    outputs.push_back(s.str());

    buffer.set_text_in_range(range, c.new_text->content.data());
    Cursor &cur = cursor();
    cur.start = c.old_start;
    cur.end = c.old_start;
  }
}

void Document::redo() {}

void Document::begin_cursor_markers() {
  int idx = 0;
  for (auto &c : cursors) {
    c.id = idx++;
    cursor_markers.insert(c.id, c.start, c.end);
  }
}

void Document::end_cursor_markers(int id) {
  for (auto &c : cursors) {
    if (c.id != id) {
      c.start = cursor_markers.get_start(c.id);
      c.end = cursor_markers.get_end(c.id);
    }
    cursor_markers.remove(c.id);
  }
}
