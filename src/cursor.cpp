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
void Cursor::move_to_start_of_document(bool anchor) {
  start.column = 0;
  start.row = 0;
  if (!anchor) {
    end = start;
  }
}

void Cursor::move_to_end_of_document(bool anchor) {
  start = buffer->extent();
  if (!anchor) {
    end = start;
  }
}

void Cursor::move_to_start_of_line(bool anchor) {
  start.column = 0;
  if (!anchor) {
    end = start;
  }
}

void Cursor::move_to_end_of_line(bool anchor) {
  optional<uint32_t> l = (*buffer).line_length_for_row(start.row);
  if (l) {
    start.column = *l;
  }
  if (!anchor) {
    end = start;
  }
}

void Cursor::move_to_previous_word(bool anchor)
{
  std::vector<int> indices = document->word_indices_in_line(start.row, false, true);
  int target = 0;
  for(auto i : indices) {
    if (i >= start.column) {
      break;
    }
    target = i;
  }
  start.column = target;
  if (!anchor) {
    end = start;
  }
}

void Cursor::move_to_next_word(bool anchor)
{
  std::vector<int> indices = document->word_indices_in_line(start.row, true, false);
  int target = 0;
  for(auto i : indices) {
    target = i;
    if (i > start.column) {
      break;
    }
  }
  start.column = target;
  if (!anchor) {
    end = start;
  }
}

Cursor Cursor::copy() { return Cursor{start, end, buffer, document}; }
Cursor Cursor::copy_from(Cursor cursor) {
  return Cursor{cursor.start, cursor.end, cursor.buffer, cursor.document};
}

bool Cursor::is_normalized() {
  if (start.row > end.row ||
      (start.row == end.row && start.column > end.column)) {
    return false;
  }
  return true;
}

Cursor Cursor::normalized(bool force_flip) {
  Cursor c = copy();
  bool flip = !is_normalized();
  if (force_flip) {
    flip = !flip;
  }
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
  document->cursor_markers.splice(range.start, {0, 0}, {r, c});
  document->update_blocks(range.start.row, size_diff);
  clear_selection();
  for (int i = 0; i < text.size(); i++) {
    move_right();
  }
  if (start.row != range.start.row && size_diff == 0) {
    move_left();
  }
}

void Cursor::delete_text(int number_of_characters) {
  for (int i = 0; i < number_of_characters; i++) {
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
    document->cursor_markers.splice(range.start, {r, c}, {0, 0});
    document->update_blocks(range.start.row, size_diff);
    clear_selection();
  }
}

std::u16string Cursor::selected_text() {
  return buffer->text_in_range(normalized());
}
