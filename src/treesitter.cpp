#include "treesitter.h"
#include "document.h"
#include "utf8.h"

#include <algorithm>
#include <functional>
#include <map>
#include <pthread.h>

extern "C" {
const TSLanguage *tree_sitter_c(void);
const TSLanguage *tree_sitter_cpp(void);
const TSLanguage *tree_sitter_c_sharp(void);
const TSLanguage *tree_sitter_css(void);
const TSLanguage *tree_sitter_html(void);
const TSLanguage *tree_sitter_java(void);
const TSLanguage *tree_sitter_javascript(void);
const TSLanguage *tree_sitter_json(void);
const TSLanguage *tree_sitter_python(void);
}

#include "document.h"
#include "util.h"

#define ENABLE_INCREMENTAL_UPDATE
#define PARSER_TIMEOUT 1000 * 1000 * 5

std::map<std::string, std::function<const TSLanguage *()>> ts_languages = {
    {"c", tree_sitter_c},
    {"cpp", tree_sitter_cpp},
    {"h", tree_sitter_cpp},
    {"hpp", tree_sitter_cpp},
    {"cs", tree_sitter_c_sharp},
    {"css", tree_sitter_css},
    {"html", tree_sitter_html},
    {"xml", tree_sitter_html},
    {"java", tree_sitter_java},
    {"jsx", tree_sitter_javascript},
    {"js", tree_sitter_javascript},
    {"javascript", tree_sitter_javascript},
    {"vue", tree_sitter_javascript},
    {"json", tree_sitter_json},
    {"python", tree_sitter_python},
};

void walk_tree(TSTreeCursor *cursor, int depth, int row, int column,
               std::vector<TSNode> *nodes) {
  TSNode node = ts_tree_cursor_current_node(cursor);
  int start = ts_node_start_byte(node);
  int end = ts_node_end_byte(node);

  const char *type = ts_node_type(node);
  TSPoint startPoint = ts_node_start_point(node);
  TSPoint endPoint = ts_node_end_point(node);

  // if (strcmp(type, "ERROR") == 0) {
  //   nodes->clear();
  //   return;
  // }

  startPoint.column /= 2;
  endPoint.column /= 2;
  
  // log("(%d %d)-(%d %d) %s", startPoint.row, startPoint.column, endPoint.row,
  // endPoint.column, type);

  if (row != -1 && column != -1) {
    if (row < startPoint.row ||
        (row == startPoint.row && column < startPoint.column))
      return;
    if (row > endPoint.row || (row == endPoint.row && column > endPoint.column))
      return;
  }
  if (row != -1 && (row < startPoint.row || row > endPoint.row)) {
    return;
  }

  if (startPoint.row <= row && endPoint.row >= row) {
    if (nodes != NULL) {
      nodes->push_back(node);
    }
  }

  if (!ts_tree_cursor_goto_first_child(cursor)) {
    return;
  }

  do {
    walk_tree(cursor, depth + 1, row, column, nodes);
  } while (ts_tree_cursor_goto_next_sibling(cursor));
}

// doesn't work correctly
const char *_read(void *payload, uint32_t byte_index, TSPoint position,
                  uint32_t *bytes_read) {
  TextBuffer::Snapshot *snapshot = (TextBuffer::Snapshot *)payload;
  int extent = snapshot->extent().row + 1;

  *bytes_read = 0;

  if (position.row < extent) {
    optional<uint32_t> length = snapshot->line_length_for_row(position.row);

    log("reading: (%d %d)", position.row, position.column);
    if (length) {
      uint32_t l = *length;
      int ll = l - position.column;
      if (position.column < l) {
        std::u16string str =
            snapshot->text_in_range({{position.row, position.column},
                                     {position.row, position.column + ll}});
        *bytes_read = str.size();
        log(">>(%d %d) %s %d", position.row, position.column,
            u16string_to_string(str).c_str(), ll);
        // return (char*)str.c_str();//u16string_to_string(str).c_str();
        return (char *)u16string_to_string(str).c_str();
      }
    }

    if (*bytes_read == 0) {
      *bytes_read = 1;
      return "\n";
    }
  }

  return "";
}

void build_tree(TreeSitter *treesitter) {
  Document *doc = treesitter->document;
  TextBuffer::Snapshot *snapshot = treesitter->snapshot;

  // log("%x", treesitter->thread_id);

  TSTree *old_tree = NULL;
  TSInputEdit edit;

#ifdef ENABLE_INCREMENTAL_UPDATE
  // get changes from last treesitter run
  if (treesitter->reference) {
    old_tree = treesitter->reference->tree;

    std::vector<TSInputEdit> edits;

    int base_row = -1;
    for (auto c : treesitter->patch.get_changes()) {
      // TODO
      // This considers only if insert is on a single line
      // This is dropped if it is a multiline edit
      if (c.old_start.row != c.new_start.row ||
          c.old_end.column < c.new_end.column) {
        old_tree = NULL;
        break;
      }
      std::u16string s = snapshot->text_in_range(
          {{0, 0}, {c.old_start.row, c.old_start.column}});

      if (base_row == -1) {
        base_row = c.old_start.row;
      } else if (base_row != c.old_start.row) {
        old_tree = NULL;
        break;
      }
      int len = c.old_end.column - c.new_end.column;

      edit.start_byte = s.size();
      edit.old_end_byte = edit.start_byte;
      edit.new_end_byte = edit.start_byte + len;

      edit.start_point.row = c.old_start.row;
      edit.start_point.column = c.old_start.column;
      edit.old_end_point = edit.start_point;
      edit.new_end_point = edit.start_point;
      edit.new_end_point.column += len;
      edits.push_back(edit);

      // log(" %d %d %d %d %s %d", c.old_start.row, c.old_start.column,
      // edit.start_byte, edit.old_end_byte, c.old_text->content.data(), len);
    }

    if (old_tree) {
      // log("...update tree");
      for (auto e : edits) {
        ts_tree_edit(old_tree, &e);
      }
    }
  }
#endif

  treesitter->tree = NULL;

  std::string langId = doc->language ? doc->language->id : "--unknown--";

  if (ts_languages.find(langId) == ts_languages.end()) {
    log("language not available %s\n", langId.c_str());
    return;
  }
  std::function<const TSLanguage *()> lang = ts_languages[langId];

  TSParser *parser = ts_parser_new();
#ifdef PARSER_TIMEOUT
  ts_parser_set_timeout_micros(parser, PARSER_TIMEOUT);
#endif
  if (!ts_parser_set_language(parser, lang())) {
    log("invalid language\n");
    return;
  }

#if 0
  // doesn't work correctly
  TSInput tsinput;
  tsinput.payload = (void*)snapshot;
  tsinput.read = _read;
  tsinput.encoding = TSInputEncodingUTF8;
  TSTree *tree = ts_parser_parse(parser, old_tree, tsinput);
#else
  // TSTree *tree = ts_parser_parse_string(parser, old_tree,
  //                                       (char *)(treesitter->content.c_str()),
  //                                       treesitter->content.size());

  TSTree *tree = ts_parser_parse_string_encoding(parser, old_tree,
                                      (char *)(treesitter->snapshot->text().c_str()),
                                      treesitter->snapshot->size()*2,
                                      TSInputEncodingUTF16);
#endif

  if (tree) {
    treesitter->tree = tree;

    // log("dump----------");
    // TSNode root_node = ts_tree_root_node(tree);
    // TSTreeCursor cursor = ts_tree_cursor_new(root_node);
    // std::vector<TSNode> nodes;
    // walk_tree(&cursor, 0, -1, -1, &nodes);
    // ts_tree_cursor_delete(&cursor);

  } else {
    log(">>error parsing tree");
  }

  ts_parser_delete(parser);
}

// void walk_tree_for_reference(std::string &content, TSTreeCursor *cursor,
//                              int depth, std::vector<std::string> *identifiers) {
//   TSNode node = ts_tree_cursor_current_node(cursor);
//   int start = ts_node_start_byte(node);
//   int end = ts_node_end_byte(node);
//   int l = end - start;

//   const char *type = ts_node_type(node);
//   TSPoint startPoint = ts_node_start_point(node);
//   TSPoint endPoint = ts_node_end_point(node);

//   if (strcmp(type, "ERROR") == 0) {
//     return;
//   }

//   if (strcmp(type, "identifier") == 0 && l > 2) {
//     std::string tmp = content.substr(start, end - start);
//     if (std::find(identifiers->begin(), identifiers->end(), tmp) ==
//         identifiers->end()) {
//       identifiers->push_back(tmp);
//     }
//   }

//   TSTreeCursor _cur = ts_tree_cursor_new(node);
//   if (ts_tree_cursor_goto_first_child(&_cur)) {
//     do {
//       walk_tree_for_reference(content, &_cur, depth + 1, identifiers);
//     } while (ts_tree_cursor_goto_next_sibling(&_cur));
//   }

//   ts_tree_cursor_delete(&_cur);
// }

// void build_reference(TreeSitter *treesitter) {
//   if (!treesitter->tree)
//     return;

//   Document *doc = treesitter->document;
//   TextBuffer::Snapshot *snapshot = treesitter->snapshot;

//   TSNode root_node = ts_tree_root_node(treesitter->tree);
//   TSTreeCursor cursor = ts_tree_cursor_new(root_node);
//   walk_tree_for_reference(treesitter->content, &cursor, 0,
//                           &treesitter->identifiers);
//   ts_tree_cursor_delete(&cursor);
// }

#define TREESITTER_TTL 32

bool TreeSitter::is_available(std::string lang_id) {
  std::string langId = lang_id;
  if (ts_languages.find(langId) == ts_languages.end()) {
    // printf("language not available\n");
    return true;
  }
  return false;
}

TreeSitter::TreeSitter()
    : state(State::Loading), snapshot(0), document(0), tree(NULL),
      ttl(TREESITTER_TTL), thread_id(0), reference_ready(false) {}

TreeSitter::~TreeSitter() {
  if (snapshot) {
    delete snapshot;
  }
  if (tree) {
    ts_tree_delete(tree);
  }
  // log("~treesitter");
}

void TreeSitter::set_ready() { state = TreeSitter::State::Ready; }

void TreeSitter::set_consumed() { state = TreeSitter::State::Consumed; }

void TreeSitter::keep_alive() { ttl = TREESITTER_TTL; }

bool TreeSitter::is_disposable() {
  if (state < TreeSitter::State::Ready) {
    return false;
  }
  return --ttl <= 0;
}

std::vector<TSNode> TreeSitter::walk(int row, int column) {
  std::vector<TSNode> nodes;

  TSNode root_node = ts_tree_root_node(tree);
  TSTreeCursor cursor = ts_tree_cursor_new(root_node);
  walk_tree(&cursor, 0, row, column, &nodes);
  ts_tree_cursor_delete(&cursor);
  return nodes;
}

TSNode TreeSitter::node_at(int row, int column) {
  TSNode res;
  res.id = 0;
  std::vector<TSNode> nodes = walk(row, column);
  if (nodes.size() > 0) {
    return nodes.back();
  }
  return res;
}

void perf_begin_timer(std::string);
void perf_end_timer(std::string);

void *treeSitter_thread(void *arg) {
  perf_begin_timer("treesitter");

  TreeSitter *treesitter = (TreeSitter *)arg;

  build_tree(treesitter);

  treesitter->thread_id = 0;
  treesitter->set_ready();

  // build_reference(treesitter);
  // treesitter->reference_ready = true;

  // delete treesitter->snapshot;
  // treesitter->snapshot = NULL;

  perf_end_timer("treesitter");
  return NULL;
}

void TreeSitter::run(TreeSitter *treesitter) {
  pthread_create((pthread_t *)&(treesitter->thread_id), NULL,
                 &treeSitter_thread, (void *)(treesitter));
}