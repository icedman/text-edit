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
  int size = document->size();
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
  Range range = normalized();
  int c = 1;
  int r = 0;
  int start_size = document->size();
  buffer->set_text_in_range(range, text.data());
  int size_diff = document->size() - start_size;
  if (size_diff > 0) {
    r = size_diff;
    c = 0;
  }
  document->cursor_markers.splice(range.start, {0,0}, {r,c});
  document->update_blocks(range.start.row, size_diff);
  clear_selection();
  move_right();
}

void Cursor::delete_text(int number_of_characters) {
  for(int i=0; i<number_of_characters; i++) {
    if (!has_selection()) {
      move_right(true);
    }
    Range range = normalized();
    int c = 1;
    int r = 0;
    int start_size = document->size();
    buffer->set_text_in_range(range, u"");
    int size_diff = document->size() - start_size;
    if (size_diff < 0) {
      r = -size_diff;
      c = 0;
    }
    document->cursor_markers.splice(range.start, {r,c}, {0,0});
    document->update_blocks(range.start.row, size_diff);
    clear_selection();
  }
}