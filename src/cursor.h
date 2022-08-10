#ifndef TE_CURSOR_H
#define TE_CURSOR_H

#include <core/optional.h>
#include <core/text-buffer.h>
#include <string>

bool is_point_within_range(Point p, Range r);
bool compare_range(Range a, Range b);
bool compare_range_reverse(Range a, Range b);
int count_indent_size(std::string text);
int count_indent_size(std::u16string text);
optional<Range> intersect_row(Range range, int row, int length);

class Document;
class Cursor : public Range {
public:
  TextBuffer *buffer;
  Document *document;
  int id;

  bool move_up(bool anchor = false);
  bool move_down(bool anchor = false);
  bool move_left(bool anchor = false);
  bool move_right(bool anchor = false);

  void move_to_start_of_document(bool anchor = false);
  void move_to_end_of_document(bool anchor = false);
  void move_to_start_of_line(bool anchor = false);
  void move_to_end_of_line(bool anchor = false);
  void move_to_previous_word(bool anchor = false);
  void move_to_next_word(bool anchor = false);

  Cursor copy();
  void copy_from(Cursor cursor);
  bool is_normalized();
  Cursor normalized(bool force_flip = false);
  void selection_to_uppercase();
  void selection_to_lowercase();
  void select_word_under();
  bool has_selection();
  void clear_selection();
  bool is_within(int row, int column);
  bool is_edge(int row, int column);
  void insert_text(std::u16string text);
  void delete_text(int number_of_characters = 1);
  void delete_next_text(std::u16string text);

  std::u16string selected_text();
};

#endif // TE_CURSOR_H