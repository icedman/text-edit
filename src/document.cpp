#include "document.h"
#include "utf8.h"

#include <core/regex.h>
#include <sstream>

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

void Document::move_to_previous_word(bool anchor) {
  for (auto &c : cursors) {
    c.move_to_previous_word(anchor);
  }
}

void Document::move_to_next_word(bool anchor) {
  for (auto &c : cursors) {
    c.move_to_next_word(anchor);
  }
}

void Document::select_word_from_cursor() {
  for (auto &c : cursors) {
    c.clear_selection();
    c.select_word_under();
  }
}

void Document::add_cursor_from_selected_word() {
  if (cursor().has_selection()) {
    bool normed = cursor().is_normalized();
    std::u16string res = cursor().selected_text();
    Cursor cur = cursor().copy();
    cur.move_right();
    optional<Range> m = find_from_cursor(res, cur);
    if (m) {
      std::u16string mt = buffer.text_in_range(*m);
      cur.start = (*m).start;
      cur.end = (*m).end;
      cur = cur.normalized(!normed);
      add_cursor(cur);
    }
  } else {
    select_word_from_cursor();
  }
}

void Document::selection_to_uppercase() {
  for (auto &c : cursors) {
    c.selection_to_uppercase();
  }
}

void Document::selection_to_lowercase() {
  for (auto &c : cursors) {
    c.selection_to_lowercase();
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
  while (blocks.size() < size()) {
    add_block_at(0);
  }
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
  if (tab_string.length() == 0) {
    optional<std::u16string> row = buffer.line_for_row(line);
    if (row) {
      for (int i = 0; i < (*row).length(); i++) {
        if ((*row)[i] == u' ') {
          tab_string += u" ";
        } else {
          break;
        }
      }
    }
  }
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
  if (!block) {
    return;
  }
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
      words.push_back(Range{Point{line, offset + res.start_offset},
                            Point{line, offset + res.end_offset}});
    }
  } while (res.type == Regex::MatchResult::Full);

  return words;
}

std::vector<int> Document::word_indices_in_line(int line, bool start,
                                                bool end) {
  std::vector<int> indices;
  BlockPtr block = block_at(line);
  if (block->words.size() == 0) {
    block->words = words_in_line(line);
  }
  for (auto r : block->words) {
    if (start) {
      indices.push_back(r.start.column);
    }
    if (end) {
      indices.push_back(r.end.column);
    }
  }
  return indices;
}

void Document::indent() {
  std::map<int, bool> lines;
  for (auto c : cursors) {
    lines[c.start.row] = true;
  }
  for (auto m : lines) {
    Cursor c = cursor().copy();
    c.start.row = m.first;
    c.start.column = 0;
    c.end = c.start;
    c.insert_text(tab_string);
  }
}

void Document::unindent() {}

optional<Range> Document::subsequence_range() {
  Cursor cur = cursor().normalized();
  std::vector<int> indices = word_indices_in_line(cur.start.row, true, true);
  for (int i = 0; i < indices.size(); i += 2) {
    Range range =
        Range{{cur.start.row, indices[i]}, {cur.start.row, indices[i + 1]}};
    if (range.end.column == cur.end.column) {
      return range;
    }
    if (range.start.column > cur.start.column)
      break;
  }
  return optional<Range>();
}

std::u16string Document::subsequence_text() {
  optional<Range> range = subsequence_range();
  if (range) {
    return buffer.text_in_range(*range);
  }
  return u"";
}

void Document::run_autocomplete() {
  if (cursors.size() == 1) {
    std::u16string sub = subsequence_text();
    if (sub.size() < 3)
      return;
    if (autocomplete_substring != sub) {
      autocomplete_substring = sub;
      if (autocompletes.find(sub) != autocompletes.end()) {
        if (autocompletes[sub] != nullptr) {
          autocompletes[sub]->state = AutoComplete::State::Ready;
          return;
        }
      }

      AutoCompletePtr autocomplete = std::make_shared<AutoComplete>(sub);
      autocomplete->document = this;
      autocomplete->buffer = &buffer;
      autocomplete->snapshot = buffer.create_snapshot();
      autocompletes[sub] = autocomplete;
      AutoComplete::run(autocomplete.get());
    }
  }
}

AutoCompletePtr Document::autocomplete() {
  if (cursors.size() == 1) {
    if (autocompletes.find(autocomplete_substring) != autocompletes.end()) {
      if (autocompletes[autocomplete_substring]->state !=
              AutoComplete::State::Loading &&
          autocompletes[autocomplete_substring]->matches.size() > 0) {
        return autocompletes[autocomplete_substring];
      }
    }
  }

  // remove disposables
  return nullptr;
}

void Document::clear_autocomplete(bool force) {
  std::u16string sub = subsequence_text();
  if (force || sub != autocomplete_substring) {
    autocomplete_substring = u"";
  }
}