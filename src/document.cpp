#include "document.h"
#include "utf8.h"

#include <core/regex.h>
#include <sstream>

Request::Request(TextBuffer *buffer)
    : ready(false), mark_dispose(false), snapshot(NULL) {
  snapshot = buffer->create_snapshot();
}

Request::~Request() {
  if (snapshot) {
    delete snapshot;
  }
}

Block::Block()
    : line(0), comment_line(false), comment_block(false),
      prev_comment_block(false), dirty(true) {}

Document::Document() : snapshot(0) {}

Document::~Document() {
  if (snapshot) {
    delete snapshot;
  }
}

void Document::initialize(std::u16string &str) {
  buffer.set_text(str);
  buffer.flush_changes();
  blocks.clear();
  snap();

  blocks.clear();
  int l = size();
  for (int i = 0; i < l + 1; i++) {
    add_block_at(i + 1);
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
    begin_cursor_markers(c);
    c.insert_text(text);
    end_cursor_markers(c);
  }
}

void Document::delete_text(int number_of_characters) {
  for (auto &c : cursors) {
    begin_cursor_markers(c);
    c.delete_text(number_of_characters);
    end_cursor_markers(c);
  }
}

void Document::move_to_start_of_document(bool anchor) {
  for (auto &c : cursors) {
    c.move_to_start_of_document(anchor);
  }
}

void Document::move_to_end_of_document(bool anchor) {
  for (auto &c : cursors) {
    c.move_to_end_of_document(anchor);
  }
}

void Document::move_to_start_of_line(bool anchor) {
  for (auto &c : cursors) {
    c.move_to_start_of_line(anchor);
  }
}

void Document::move_to_end_of_line(bool anchor) {
  for (auto &c : cursors) {
    c.move_to_end_of_line(anchor);
  }
}

void Document::move_to_previous_word(bool anchor)
{
  for (auto &c : cursors) {
    c.move_to_previous_word(anchor);
  }
}

void Document::move_to_next_word(bool anchor)
{
    for (auto &c : cursors) {
    c.move_to_next_word(anchor);
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

std::u16string Document::selected_text() {
  std::u16string res;
  // todo normalize cursors
  for (auto c : cursors) {
    res += c.selected_text();
  }
  return res;
}

void Document::undo() {
  if (!snapshot)
    return;

  Cursor cur = cursor();
  cursors.clear();

  // buffer.flush_changes();

  auto patch = buffer.get_inverted_changes(snapshot);
  std::vector<Patch::Change> changes = patch.get_changes();

  std::u16string prev;
  auto it = changes.rbegin();
  while (it != changes.rend()) {
    auto c = *it++;
    if (cursors.size() > 0 && c.old_text->content.compare(prev) != 0)
      break;
    Range range = Range({c.old_start, c.old_end});
    buffer.set_text_in_range(range, c.new_text->content.data());
    if (cur.start != c.new_end && cur.end != c.new_start) {
      cur.start = c.new_end;
      cur.end = c.new_start;
      cursors.push_back(cur.copy());
    }
    prev = c.old_text->content;
  }

  // dirty all
  for (auto b : blocks) {
    b->make_dirty();
  }
}

void Document::redo() {}

void Document::begin_cursor_markers(Cursor &cur) {
  int idx = 0;
  for (auto &c : cursors) {
    c.id = idx++;
    cursor_markers.insert(c.id, c.start, c.end);
  }
}

void Document::end_cursor_markers(Cursor &cur) {
  for (auto &c : cursors) {
    if (c.id != cur.id) {
      c.start = cursor_markers.get_start(c.id);
      c.end = cursor_markers.get_end(c.id);
    }
    cursor_markers.remove(c.id);
  }
}

int Document::size() { return buffer.extent().row; }

BlockPtr Document::block_at(int line) {
  if (line >= blocks.size() || line < 0)
    return NULL;
  if (blocks[line]->line != line) {
    blocks[line]->make_dirty();
    blocks[line]->line = line;
  }
  return blocks[line];
}

BlockPtr Document::add_block_at(int line) {
  BlockPtr block = std::make_shared<Block>();
  block->line = line;
  if (line >= blocks.size()) {
    blocks.push_back(block);
  } else {
    blocks.insert(blocks.begin() + line, block);
  }
  return block;
}

BlockPtr Document::erase_block_at(int line) {
  if (line >= blocks.size())
    return NULL;
  auto it = blocks.begin() + line;
  BlockPtr block = *it;
  blocks.erase(it);
  outputs.push_back("remove");
  return block;
}

BlockPtr Document::previous_block(BlockPtr block) {
  return block_at(block->line - 1);
}

BlockPtr Document::next_block(BlockPtr block) {
  return block_at(block->line + 1);
}

void Document::update_blocks(int line, int count) {
  BlockPtr block = block_at(line);
  block->make_dirty();

  // todo... detect change at run_highlighter
  BlockPtr next_block = block_at(line + 1);
  if (next_block)
    next_block->make_dirty();

  if (count == 0)
    return;
  int r = count > 0 ? count : -count;
  for (int i = 0; i < r; i++) {
    if (count < 0) {
      erase_block_at(line);
    } else {
      add_block_at(line);
    }
  }
  // std::stringstream s;
  // s << blocks.size();
  // s << "/";
  // s << size();
  // outputs.push_back(s.str());
}

optional<Range> Document::find_from_cursor(std::u16string text, Cursor cursor) {
  std::u16string error;
  Regex regex(text.c_str(), &error, false, false);
  Range range = cursor.normalized();
  range.end = buffer.extent();
  optional<Range> res;
  res = buffer.find(regex, range);
  return res;
}

void Document::copy() { clipboard_data = selected_text(); }

void Document::paste() {
  if (clipboard_data.size() > 0) {
    insert_text(clipboard_data);
  }
}

std::vector<Range> Document::words_in_line(int line) {
  std::vector<Range> words;

  optional<std::u16string> row = buffer.line_for_row(line);
  if (!row) {
    return words;
  }

  std::u16string str = *row;

  static std::u16string pattern = u"[a-zA-Z_0-9]+";

  std::u16string error;
  Regex regex(pattern.c_str(), &error, false, false);
  Regex::MatchData data(regex);
  Regex::MatchResult res = {Regex::MatchResult::None, 0, 0};

  int offset = 0;
  do {
    offset += res.end_offset;
    char16_t *_str = (char16_t *)str.c_str();
    _str += offset;
    res = regex.match(_str, strlen((char *)_str), data);
    // printf(">%d %ld %ld\n", res.type, offset + res.start_offset,
    //        offset + res.end_offset);
    // std::u16string _w = str.substr(offset + res.start_offset,
    //                                (res.end_offset - res.start_offset));
    // std::string _w8 = u16string_to_string(_w);
    // printf(">>%s\n", _w8.c_str());
    if (res.type == Regex::MatchResult::Full) {
      words.push_back(Range{Point{line, offset + res.start_offset}, Point{line, offset + res.end_offset}});
    }
  } while (res.type == Regex::MatchResult::Full);

  return words;
}

std::vector<int> Document::word_indices_in_line(int line, bool start, bool end) {
  std::vector<int> indices;
  BlockPtr block = block_at(line);
  if (block->words.size() == 0) {
    block->words = words_in_line(line);
  }
  for(auto r : block->words) {
    if (start) {
      indices.push_back(r.start.column);
    }
    if (end) {
      indices.push_back(r.end.column);
    }
  }
  return indices;
}
