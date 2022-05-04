#ifndef TE_DOCUMENT_H
#define TE_DOCUMENT_H

#include <core/marker-index.h>
#include <core/text-buffer.h>
#include <string>
#include <vector>

#include "cursor.h"

class Document {
public:
  Document();
  ~Document();

  std::vector<std::string> outputs;

  TextBuffer buffer;
  TextBuffer::Snapshot *snapshot;

  MarkerIndex cursor_markers;

  std::vector<Cursor> cursors;
  Cursor &cursor();

  void move_up(bool anchor = false);
  void move_down(bool anchor = false);
  void move_left(bool anchor = false);
  void move_right(bool anchor = false);
  void insert_text(std::u16string text);
  void delete_text(int number_of_characters = 1);

  bool has_selection();
  void clear_selection();
  void clear_cursors();
  void add_cursor(Cursor cursor);
  void begin_cursor_markers();
  void end_cursor_markers(int id);

  void undo();
  void redo();

  void snap();
};

#endif // TE_DOCUMENT_H