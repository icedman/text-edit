#include "document.h"
#include "utf8.h"

#include <sstream>

Block::Block() : line(0), dirty(true) {}

Document::Document() : snapshot(0) {}

Document::~Document() {
  if (snapshot) {
    delete snapshot;
  }
}

void Document::initialize(std::u16string& str)
{
  buffer.set_text(str);
  buffer.flush_changes();
  blocks.clear();
  snap();

  blocks.clear();
  int l = size();
  for(int i=0; i<l; i++) {
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
    begin_cursor_markers();
    c.insert_text(text);
    end_cursor_markers(c.id);
  }
}

void Document::delete_text(int number_of_characters) {
  for (auto &c : cursors) {
    begin_cursor_markers();
    c.delete_text(number_of_characters);
    end_cursor_markers(c.id);
  }
}

void Document::move_to_start_of_document(bool anchor)
{}

void Document::move_to_end_of_document(bool anchor)
{}

void Document::move_to_start_of_line(bool anchor)
{}

void Document::move_to_end_of_line(bool anchor)
{}

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
  while(it != changes.rend()) {
    auto c = *it++;
    if (cursors.size() > 0 && c.old_text->content.compare(prev)!=0) break;
    Range range = Range({c.old_start, c.old_end});
    buffer.set_text_in_range(range, c.new_text->content.data());
    if (cur.start != c.new_end && cur.end != c.new_start) {
      cur.start = c.new_end;
      cur.end = c.new_start;
      cursors.push_back(cur.copy());
    }
    prev = c.old_text->content;
  }
}

void Document::redo() {}

void Document::begin_cursor_markers() {
  int idx = 0;
  for (auto &c : cursors) {
    c.id = idx++;
    cursor_markers.insert(c.id, c.start, c.end);
  }
}

void Document::end_cursor_markers(int id) {
  for (auto &c : cursors) {
    if (c.id != id) {
      c.start = cursor_markers.get_start(c.id);
      c.end = cursor_markers.get_end(c.id);
    }
    cursor_markers.remove(c.id);
  }
}

int Document::size()
{
  return buffer.extent().row;
}

BlockPtr Document::block_at(int line)
{
  if (line >= blocks.size()) return NULL;
  blocks[line]->line = line;
  return blocks[line];
}

BlockPtr Document::add_block_at(int line)
{
  BlockPtr block = std::make_shared<Block>();
  block->line = line;
  if (line >= blocks.size()) {
    blocks.push_back(block);
  } else {
    blocks.insert(blocks.begin() + line, block);
  }
  return block;
}

BlockPtr Document::erase_block_at(int line)
{
  if (line >= blocks.size()) return NULL;
  auto it = blocks.begin() + line;
  BlockPtr block = *it;
  blocks.erase(it);
  outputs.push_back("remove");
  return block;
}

void Document::update_blocks(int line, int count) {
  block_at(line)->dirty = true;
  if (count == 0) return;
  int r = count > 0 ? count : -count;
  for(int i=0;i<r;i++) {
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