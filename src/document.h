#ifndef TE_DOCUMENT_H
#define TE_DOCUMENT_H

#include <core/marker-index.h>
#include <core/text-buffer.h>
#include <map>
#include <string>
#include <vector>

#include "cursor.h"
#include "parse.h"
#include "textmate.h"

class Request {
public:
  Request(TextBuffer *buffer);
  ~Request();

  bool ready;
  bool mark_dispose;
  TextBuffer::Snapshot *snapshot;
};

typedef std::shared_ptr<Request> RequestPtr;

class Block {
public:
  Block();
  int line;
  bool dirty;
  parse::stack_ptr parser_state;
  std::vector<textstyle_t> styles;
  std::vector<Range> words;

  bool comment_line;
  bool comment_block;
  bool prev_comment_block;

  void make_dirty() {
    dirty = true;
    words.clear();
  }
};

typedef std::shared_ptr<Block> BlockPtr;

class Document {
public:
  Document();
  ~Document();

  std::vector<std::string> outputs;

  std::vector<RequestPtr> subsequence;
  std::u16string clipboard_data;

  TextBuffer buffer;
  TextBuffer::Snapshot *snapshot;
  MarkerIndex cursor_markers;
  std::vector<Cursor> cursors;
  std::vector<BlockPtr> blocks;

  void initialize(std::u16string &str);

  Cursor &cursor();

  void move_up(bool anchor = false);
  void move_down(bool anchor = false);
  void move_left(bool anchor = false);
  void move_right(bool anchor = false);

  void insert_text(std::u16string text);
  void delete_text(int number_of_characters = 1);

  void move_to_start_of_document(bool anchor = false);
  void move_to_end_of_document(bool anchor = false);
  void move_to_start_of_line(bool anchor = false);
  void move_to_end_of_line(bool anchor = false);
  void move_to_previous_word(bool anchor = false);
  void move_to_next_word(bool anchor = false);

  void selection_to_uppercase();
  void selection_to_lowercase();
  void select_word_under();
  bool has_selection();
  void clear_selection();
  void clear_cursors();
  void add_cursor(Cursor cursor);
  void begin_cursor_markers(Cursor &cursor);
  void end_cursor_markers(Cursor &cursor);

  std::u16string selected_text();
  optional<Range> find_from_cursor(std::u16string text, Cursor cursor);

  void copy();
  void paste();
  void undo();
  void redo();

  void snap();

  int size();

  BlockPtr block_at(int line);
  BlockPtr add_block_at(int line);
  BlockPtr erase_block_at(int line);
  BlockPtr previous_block(BlockPtr block);
  BlockPtr next_block(BlockPtr block);
  void update_blocks(int line, int count);

  std::vector<Range> words_in_line(int line);
  std::vector<int> word_indices_in_line(int line, bool start = true, bool end = true);
};

#endif // TE_DOCUMENT_H