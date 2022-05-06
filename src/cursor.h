#ifndef TE_CURSOR_H
#define TE_CURSOR_H

#include <core/text-buffer.h>
#include <string>

class Document;
class Cursor : public Range {
public:
  TextBuffer *buffer;
  Document *document;
  int id;

  void move_up(bool anchor = false);
  void move_down(bool anchor = false);
  void move_left(bool anchor = false);
  void move_right(bool anchor = false);

  Cursor copy();
  Cursor copy_from(Cursor cursor);
  bool is_normalized();
  Cursor normalized();
  bool has_selection();
  void clear_selection();
  bool is_within(int row, int column);
  void insert_text(std::u16string text);
  void delete_text(int number_of_characters = 1);
};

#endif // TE_CURSOR_H