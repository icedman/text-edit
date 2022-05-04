#include "cursor.h"
#include "document.h"

void Cursor::move_up(bool anchor) {
  if (start.row > 0) {
    start.row--;
  }
  if (!anchor) {
    end = start;
  }
}

void Cursor::move_down(bool anchor) {
  start.row++;
  int size = buffer->extent().row;
  if (start.row >= size) {
    start.row = size - 1;
  }
  if (!anchor) {
    end = start;
  }
}

void Cursor::move_left(bool anchor) {
  if (start.column == 0) {
    if (start.row > 0) {
      start.row--;
      start.column = *(*buffer).line_length_for_row(start.row);
    }
  } else {
    start.column--;
  }

  if (!anchor) {
    end = start;
  }
}

void Cursor::move_right(bool anchor) {
  start.column++;
  if (start.column > *(*buffer).line_length_for_row(start.row)) {
    start.row++;
    start.column = 0;
  }
  if (!anchor) {
    end = start;
  }
}

Cursor Cursor::copy() { return Cursor{start, end, buffer, document}; }

bool Cursor::is_normalized() {
  if (start.row > end.row ||
      (start.row == end.row && start.column > end.column)) {
    return false;
  }
  return true;
}

Cursor Cursor::normalized() {
  Cursor c = copy();
  bool flip = !is_normalized();
  if (flip) {
    c.end = start;
    c.start = end;
  }
  return c;
}

bool Cursor::has_selection() { return start != end; }

void Cursor::clear_selection() {
  Cursor n = normalized();
  start = n.start;
  end = start;
}

bool Cursor::is_within(int row, int column) {
  Cursor c = normalized();
  if (row < c.start.row || (row == c.start.row && column < c.start.column))
    return false;
  if (row > c.end.row || (row == c.end.row && column > c.end.column))
    return false;
  return true;
}

void Cursor::insert_text(std::u16string text) {
  // todo .. this only works for single character entries
  int c = 1;
  int r = 0;
  if (text == u"\n") {
    r = 1;
    c = 0;
  }
  Range range = normalized();
  buffer->set_text_in_range(range, text.data());
  document->cursor_markers.splice(range.start, {0,0}, {r,c});
  clear_selection();
  move_right();
}

void Cursor::delete_text(int number_of_characters) {
  if (!has_selection()) {
    move_right(true);
  }
  Range range = normalized();
  // todo .. this only works for single character entries
  int c = 1;
  int r = 0;
  if (start.row != end.row) {
    r = 1;
    c = 0;
  }
  buffer->set_text_in_range(range, u"");
  document->cursor_markers.splice(range.start, {r,c}, {0,0});
  clear_selection();
}