#ifndef TE_DOCUMENT_H
#define TE_DOCUMENT_H

#include <core/marker-index.h>
#include <core/text-buffer.h>
#include <map>
#include <string>
#include <vector>

#include "autocomplete.h"
#include "cursor.h"
#include "parse.h"
#include "search.h"
#include "textmate.h"
#include "treesitter.h"

class Block {
public:
  Block();
  int line;
  int line_height;
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

  std::string file_path;

  TextBuffer buffer;
  TextBuffer::Snapshot *snapshot;
  MarkerIndex cursor_markers;
  std::vector<Cursor> cursors;
  std::vector<BlockPtr> blocks;

  std::u16string tab_string;

  std::u16string autocomplete_substring;
  std::map<std::u16string, AutoCompletePtr> autocompletes;
  std::u16string search_key;
  std::map<std::u16string, SearchPtr> searches;
  std::vector<TreeSitterPtr> treesitters;

  language_info_ptr language;

  static std::u16string &empty();

  void initialize(std::u16string &str);
  bool load(std::string path);
  bool save(std::string path);

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

  void go_to_line(int line);

  void selection_to_uppercase();
  void selection_to_lowercase();
  void select_word_from_cursor();
  void add_cursor_from_selected_word();
  bool has_selection();
  void clear_selection();
  void clear_cursors();
  void add_cursor(Cursor cursor);
  void begin_cursor_markers(Cursor &cursor);
  void end_cursor_markers(Cursor &cursor);

  std::u16string subsequence_text();
  optional<Range> subsequence_range();
  std::u16string selected_text();
  std::vector<int> selected_lines();
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
  void make_dirty();

  void indent();
  void unindent();
  void toggle_comment();

  std::vector<Range> words_in_line(int line);
  std::vector<int> word_indices_in_line(int line, bool start = true,
                                        bool end = true);

  void run_autocomplete();
  void clear_autocomplete(bool force = false);
  AutoCompletePtr autocomplete();

  void run_search(std::u16string key, Point first_index = {0, 0});
  void clear_search(bool force = false);
  SearchPtr search();

  void run_treesitter();
  TreeSitterPtr treesitter();

  optional<Cursor> block_cursor(Cursor cursor);
  optional<Cursor> span_cursor(Cursor cursor);
};

typedef std::shared_ptr<Document> DocumentPtr;

#endif // TE_DOCUMENT_H