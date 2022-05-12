#include "document.h"
#include "utf8.h"

#include <algorithm>
#include <core/encoding-conversion.h>
#include <core/regex.h>
#include <fstream>
#include <iostream>
#include <sstream>

#define TS_DOC_SIZE_LIMIT 10000
#define TS_FIND_FROM_CURSOR_LIMIT 1000

static std::u16string clipboard_data;

Block::Block()
    : line(0), line_height(1), comment_line(false), comment_block(false),
      prev_comment_block(false), dirty(true) {}

void Block::make_dirty() {
  dirty = true;
  words.clear();
  line_height = 1;
}

Document::Document() : snapshot(0), insert_mode(true) {}

Document::~Document() {
  if (snapshot) {
    delete snapshot;
  }
}

std::u16string _detect_tab_string(std::u16string text) {
  std::u16string str;
  for (int i = 0; i < text.length(); i++) {
    if (text[i] == u' ') {
      str += u" ";
    } else {
      break;
    }
  }
  return str;
}

std::u16string &Document::empty() {
  static std::u16string _empty;
  return _empty;
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

    // detect tab size
    if (tab_string.size() == 0) {
      optional<std::u16string> row = buffer.line_for_row(i);
      if (row) {
        tab_string = _detect_tab_string((*row));
      }
    }
  }

  if (tab_string.size() == 0) {
    tab_string = u"  ";
  }

  run_treesitter();
}

void Document::set_language(language_info_ptr lang) {
  language = lang;
  comment_string = u"";

  // comments
  // if (j['comments'] != null) {
  //   final comments = j['comments'] ?? {};
  //   if (comments['lineComment'] != null) {
  //     l.lineComment = comments['lineComment'];
  //   }
  //   if (comments['blockComment'] != null) {
  //     l.blockComment = [];
  //     for (final c in comments['blockComment']) {
  //       l.blockComment.add('$c');
  //     }
  //   }
  // }

  Json::Value comments = lang->definition["comments"];
  if (comments.isObject()) {
    Json::Value comment_line = comments["lineComment"];
    if (comment_line.isString()) {
      std::string scomment = comment_line.asString();
      scomment += " ";
      comment_string = string_to_u16string(scomment);
    }
  }
}

bool Document::load(std::string path) {
  file_path = path;

  std::ifstream t(path);
  std::stringstream buffer;
  buffer << t.rdbuf();

  std::u16string str;

  optional<EncodingConversion> enc = transcoding_from("UTF-8");
  (*enc).decode(str, buffer.str().c_str(), buffer.str().size());

  initialize(str);
  return true;
}

bool Document::save(std::string path) {
  std::string _sstring = u16string_to_string(buffer.text());
  std::ofstream t(path);
  t << _sstring;
  return true;
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

void Document::go_to_line(int line) {
  if (line < 0) {
    line = 0;
  }
  if (line >= size()) {
    line = size() - 1;
  }
  clear_cursors();
  Cursor &cur = cursor();
  cur.start.row = line;
  cur.end = cur.start;
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

std::vector<int> Document::selected_lines() {
  std::vector<int> res;
  for (auto c : cursors) {
    Cursor cc = c.normalized();
    for (int i = 0; i < cc.end.row - cc.start.row + 1; i++) {
      int row = cc.start.row + i;
      res.push_back(row);
    }
  }
  return res;
}

void Document::undo() {
  if (!snapshot)
    return;

  if (folds.size() > 0) {
    folds.clear();
    return;
  }

  // buffer.flush_changes();

  auto patch = buffer.get_inverted_changes(snapshot);
  std::vector<Patch::Change> changes = patch.get_changes();
  if (!changes.size()) {
    return;
  }

  Cursor cur = cursor();
  cursors.clear();

  // limitation
  // begin_fold_markers();

  std::u16string prev;
  auto it = changes.rbegin();
  while (it != changes.rend()) {
    auto c = *it++;
    if (cursors.size() > 0 && c.old_text->content.compare(prev) != 0)
      break;
    Range range = Range({c.old_start, c.old_end});
    buffer.set_text_in_range(range, c.new_text->content.data());

    // delete / insert
    // update_markers(c.old_start, {0,0}, {0, c.new_text->content.size()});

    if (cur.start != c.new_end && cur.end != c.new_start) {
      cur.start = c.new_end;
      cur.end = c.new_start;
      cursors.push_back(cur.copy());
    }
    redo_patches.push_back(
        Redo({c.old_text->content.data(), {c.new_start, c.new_end}}));

    prev = c.old_text->content;
  }

  make_dirty();
}

void Document::redo() {
  if (redo_patches.size() == 0) {
    return;
  }

  Cursor cur = cursor();
  cursors.clear();

  if (folds.size() > 0) {
    folds.clear();
    return;
  }

  int redid = 0;
  std::u16string prev;
  auto it = redo_patches.rbegin();
  while (it != redo_patches.rend()) {
    Redo c = *it++;
    buffer.set_text_in_range(c.range, c.text.data());
    cur.start = c.range.start;
    cur.end = cur.start;
    for (int i = 0; i < c.text.length(); i++) {
      cur.move_right();
    }
    cursors.push_back(cur.copy());
    redid++;
    if (redid > 1 && prev != c.text) {
      break;
    }
    prev = c.text;
  }

  for (int i = 0; i < redid; i++) {
    redo_patches.pop_back();
  }

  make_dirty();
}

void Document::make_dirty(int line) {
  // dirty all
  while (blocks.size() < size()) {
    add_block_at(0);
  }

  while (blocks.size() > size()) {
    blocks.pop_back();
  }

  auto it = blocks.begin();
  if (line >= size())
    return;
  it += line;
  while (it != blocks.end()) {
    BlockPtr b = *it++;
    b->make_dirty();
  }
}

void Document::begin_cursor_markers(Cursor &cur) {
  redo_patches.clear();
  int idx = 0;
  for (auto &c : cursors) {
    c.id = idx++;
    cursor_markers.insert(c.id, c.start, c.end);
  }
  begin_fold_markers();
}

void Document::end_cursor_markers(Cursor &cur) {
  for (auto &c : cursors) {
    if (c.id != cur.id) {
      c.start = cursor_markers.get_start(c.id);
      c.end = cursor_markers.get_end(c.id);
    }
    cursor_markers.remove(c.id);
  }
  end_fold_markers();
}

void Document::begin_fold_markers() {
  int idx = 0;
  for (auto &c : folds) {
    c.id = idx++;
    fold_markers.insert(c.id, c.start, c.end);
  }
}

void Document::end_fold_markers() {
  std::vector<Cursor> disposables;

  for (auto &c : folds) {
    c.start = fold_markers.get_start(c.id);
    c.end = fold_markers.get_end(c.id);
    fold_markers.remove(c.id);
    if (c.end.row - c.start.row == 0) {
      disposables.push_back(c);
    }
  }

  for (auto c : disposables) {
    auto it = std::find(folds.begin(), folds.end(), c);
    if (it != folds.end()) {
      folds.erase(it);
    }
  }
}

bool Document::is_within_fold(int row, int column) {
  for (auto f : folds) {
    if (is_point_within_range({row, column}, {f.start, f.end})) {
      return true;
    }
  }
  return false;
}

void Document::update_markers(Point a, Point b, Point c) {
  cursor_markers.splice(a, b, c);
  fold_markers.splice(a, b, c);
}

int Document::computed_line(int line) {
  int prev_end = 0;
  for (auto f : folds) {
    if (f.start.row < prev_end)
      continue;
    if (line > f.start.row) {
      line += f.end.row - f.start.row;
    }
    prev_end = f.end.row;
  }
  return line;
}

int Document::computed_size() {
  int l = size();
  int sz = 0;
  int prev_end = 0;
  for (auto f : folds) {
    if (f.start.row < prev_end)
      continue;
    sz += f.start.row - f.end.row;
    prev_end = f.end.row;
  }
  l -= sz;
  if (l < 1) {
    l = 1;
  }
  return l;
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
  BlockPtr next = next_block(block);
  if (next)
    next->make_dirty();

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
}

// todo ... move to thread?
optional<Range> Document::find_from_cursor(std::u16string text, Cursor cursor) {
  optional<Range> range;
  std::u16string error;

  Regex regex(text.c_str(), &error, false, false);

  Regex::MatchData data(regex);
  Regex::MatchResult res = {Regex::MatchResult::None, 0, 0};

  for (int i = 0; i < TS_FIND_FROM_CURSOR_LIMIT; i++) {
    int line = cursor.start.row + i;
    optional<std::u16string> row = buffer.line_for_row(line);
    if (!row) {
      break;
    }
    std::u16string _row = *row;
    int offset = 0;
    if (i == 0) {
      offset = cursor.start.column;
      _row = _row.substr(offset);
    }
    char16_t *_str = (char16_t *)(_row).c_str();
    res = regex.match(_str, strlen((char *)_str), data);
    if (res.type == Regex::MatchResult::Full) {
      range = Range({line, res.start_offset + offset},
                    {line, res.end_offset + offset});
      break;
    }
  }

  return range;

  // return buffer.find(regex, range);
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
  std::vector<int> lines = selected_lines();
  if (lines.size() < 0) {
    return;
  }
  for (auto m : lines) {
    Cursor c = cursor().copy();
    c.start.row = m;
    c.start.column = 0;
    c.end = c.start;
    c.id = -1;
    begin_cursor_markers(c);
    c.insert_text(tab_string);
    end_cursor_markers(c);
  }
}

void Document::unindent() {
  std::vector<int> lines = selected_lines();
  if (lines.size() < 0) {
    return;
  }
  for (auto m : lines) {
    optional<std::u16string> row = buffer.line_for_row(m);
    if (!row) {
      continue;
    }
    if (!(*row).starts_with(tab_string)) {
      continue;
    }
    Cursor c = cursor().copy();
    c.start.row = m;
    c.start.column = 0;
    c.end = c.start;
    c.id = -1;
    begin_cursor_markers(c);
    c.delete_text(tab_string.length());
    end_cursor_markers(c);
  }
}

void Document::toggle_comment() {
  std::vector<int> lines = selected_lines();
  if (lines.size() < 0) {
    return;
  }

  std::map<int, int> comment_indices;
  std::vector<int> filtered_lines;

  // detect tab
  bool has_uncommented = false;
  int count = -1;
  for (auto m : lines) {
    optional<std::u16string> row = buffer.line_for_row(m);
    if (!row) {
      continue;
    }
    filtered_lines.push_back(m);
    int idx = (*row).find(comment_string);
    comment_indices[m] = idx;
    if (idx == std::string::npos) {
      has_uncommented = true;
    }
    std::u16string t = _detect_tab_string(*row);
    if ((count == 0 || count > t.length())) {
      count = t.length();
    }
  }

  for (auto m : filtered_lines) {
    Cursor c = cursor().copy();
    c.start.row = m;
    c.start.column = count;
    c.end = c.start;
    c.id = -1;
    begin_cursor_markers(c);
    if (has_uncommented) {
      c.insert_text(comment_string);
    } else {
      c.start.column = comment_indices[m];
      c.end = c.start;
      c.delete_text(comment_string.length());
    }
    end_cursor_markers(c);
  }
}

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
        // autocompletes[autocomplete_substring]->set_ready();
        autocompletes[autocomplete_substring]->keep_alive();
        return autocompletes[autocomplete_substring];
      }
    }
  }
  return nullptr;
}

void Document::clear_autocomplete(bool force) {
  if (force || autocomplete_substring != subsequence_text()) {
    autocomplete_substring = u"";
  }

  if (autocompletes.size() < 100) {
    return;
  }

  std::vector<std::u16string> disposables;

  // dispose
  for (auto it : autocompletes) {
    AutoCompletePtr ac = it.second;
    if (ac && ac->is_disposable()) {
      disposables.push_back(it.first);
    }
  }

  for (auto d : disposables) {
    autocompletes.erase(d);
    if (autocompletes.size() < 40) {
      break;
    }
  }
}

void Document::run_search(std::u16string key, Point first_index) {
  if (key.size() < 3)
    return;
  if (search_key != key) {
    search_key = key;
    if (searches.find(key) != searches.end()) {
      if (searches[key] != nullptr) {
        searches[key]->state = Search::State::Ready;
        return;
      }
    }

    SearchPtr search = std::make_shared<Search>(key, first_index);
    search->document = this;
    search->snapshot = buffer.create_snapshot();
    searches[key] = search;
    Search::run(search.get());
  }
}

SearchPtr Document::search() {
  if (searches.find(search_key) != searches.end()) {
    if (searches[search_key]->state != Search::State::Loading) {
      // searches[search_key]->set_ready();
      searches[search_key]->keep_alive();
      return searches[search_key];
    }
  }
  return nullptr;
}

void Document::clear_search(bool force) {
  if (force || search_key != subsequence_text()) {
    search_key = u"";
  }

  std::vector<std::u16string> disposables;

  // dispose
  for (auto it : searches) {
    SearchPtr ac = it.second;
    if (ac && ac->is_disposable()) {
      disposables.push_back(it.first);
    }
  }

  for (auto d : disposables) {
    searches.erase(d);
  }
}

void Document::run_treesitter() {
  if (!language)
    return;

  // disable
  if (size() > TS_DOC_SIZE_LIMIT)
    return;

  TreeSitterPtr treesitter = std::make_shared<TreeSitter>();
  treesitter->document = this;
  treesitter->snapshot = buffer.create_snapshot();
  treesitters.push_back(treesitter);
  TreeSitter::run(treesitter.get());
}

TreeSitterPtr Document::treesitter() {
  if (treesitters.size() == 0) {
    return nullptr;
  }
  TreeSitterPtr front = treesitters.front();
  if (front->state == TreeSitter::State::Loading || !front->tree) {
    front = nullptr;
  }
  TreeSitterPtr back = treesitters.back();
  if (back->state == TreeSitter::State::Loading || !back->tree) {
    back = nullptr;
  }

  if (front && back && front != back &&
      front->state != TreeSitter::State::Loading) {
    treesitters.erase(treesitters.begin());
  }

  if (size() > TS_DOC_SIZE_LIMIT && treesitters.size()) {
    back = nullptr;
    treesitters.erase(treesitters.begin());
  }
  return back;
}

optional<Cursor> Document::block_cursor(Cursor cursor) {
  optional<Cursor> res;
  TreeSitterPtr tree = treesitter();
  if (!tree) {
    return res;
  }

  std::vector<TSNode> nodes =
      treesitter()->walk(cursor.start.row, cursor.start.column);

  if (nodes.size() == 0) {
    return res;
  }

  TSNode deepest = nodes.back();
  Cursor base = cursor.copy().normalized();
  Cursor cur = cursor.copy();

  while (deepest.id) {
    const char *type = ts_node_type(deepest);
    if (strlen(type) == 0 || type[0] == '\n')
      return res;

    TSPoint start = ts_node_start_point(deepest);
    TSPoint end = ts_node_end_point(deepest);
    cur.start = {start.row, start.column};
    cur.end = {end.row, end.column};
    res = cur;
    if (ts_node_child_count(deepest) > 0 && cur.start.row != cur.end.row) {
      break;
    }
    TSNode p = ts_node_parent(deepest);
    if (!p.id) {
      break;
    }
    deepest = p;
  }
  return res;
}

optional<Cursor> Document::span_cursor(Cursor cursor) {
  optional<Cursor> res;
  TreeSitterPtr tree = treesitter();
  if (!tree) {
    return res;
  }

  std::vector<TSNode> nodes =
      treesitter()->walk(cursor.start.row, cursor.start.column);

  if (nodes.size() == 0) {
    return res;
  }

  TSNode deepest = nodes.back();
  Cursor base = cursor.copy().normalized();
  Cursor cur = cursor.copy();

  while (deepest.id) {
    const char *type = ts_node_type(deepest);
    if (strlen(type) == 0 || type[0] == '\n')
      return res;

    TSPoint start = ts_node_start_point(deepest);
    TSPoint end = ts_node_end_point(deepest);
    cur.start = {start.row, start.column};
    cur.end = {end.row, end.column};
    res = cur;
    if (ts_node_child_count(deepest) > 0) {
      break;
    }
    TSNode p = ts_node_parent(deepest);
    if (!p.id) {
      break;
    }
    deepest = p;
  }
  return res;
}
