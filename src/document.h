#ifndef TE_DOCUMENT_H
#define TE_DOCUMENT_H

#include <core/marker-index.h>
#include <core/text-buffer.h>
#include <string>
#include <vector>
#include <map>

#include "parse.h"
#include "cursor.h"

class Block {
public:
  Block();
  int line;
  bool dirty;
  parse::stack_ptr parser_state;
};

typedef std::shared_ptr<Block> BlockPtr;

class Document {
public:
  Document();
  ~Document();

  std::vector<std::string> outputs;
  
  TextBuffer buffer;
  TextBuffer::Snapshot *snapshot;
  MarkerIndex cursor_markers;
  std::vector<Cursor> cursors;
  std::vector<BlockPtr> blocks;

  void initialize(std::u16string& str);

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

  bool has_selection();
  void clear_selection();
  void clear_cursors();
  void add_cursor(Cursor cursor);
  void begin_cursor_markers();
  void end_cursor_markers(int id);

  void undo();
  void redo();

  void snap();

  int size();

  BlockPtr block_at(int line);
  BlockPtr add_block_at(int line);
  BlockPtr erase_block_at(int line);
  void update_blocks(int line, int count);
};

#endif // TE_DOCUMENT_H