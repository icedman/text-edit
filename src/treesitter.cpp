#include "treesitter.h"
#include "document.h"
#include "utf8.h"

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

std::map<std::string, std::function<const TSLanguage *()>> ts_languages = {
    {"c", tree_sitter_c},
    {"cpp", tree_sitter_cpp},
    {"csharp", tree_sitter_c_sharp},
    {"css", tree_sitter_css},
    {"html", tree_sitter_html},
    {"xml", tree_sitter_html},
    {"java", tree_sitter_java},
    {"js", tree_sitter_javascript},
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

  // if (strlen(type) == 0 || type[0] == '\n') return;

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

// TODO
// const char* _tinput_read(
//   void *payload,
//   uint32_t byte_offset,
//   TSPoint position,
//   uint32_t *bytes_read
// ) {
//   TextBuffer::Snapshot *snapshot = (TextBuffer::Snapshot*)payload;
//   Point point = snapshot->position_for_offset(byte_offset);
//   std::u16string str =
//   return str.substr(byte_offset, );
// }
// typedef struct {
//   void *payload;
//   const char *(*read)(
//     void *payload,
//     uint32_t byte_offset,
//     TSPoint position,
//     uint32_t *bytes_read
//   );
//   TSInputEncoding encoding;
// } TSInput;
// TSInput input;
// input.payload = (void*)snapshot;
// input.encoding = TSInputEncodingUTF16;
// input.read = _tinput_read;

void build_tree(TreeSitter *treesitter) {
  Document *doc = treesitter->document;
  TextBuffer::Snapshot *snapshot = treesitter->snapshot;

  treesitter->tree = NULL;

  std::string langId = doc->language ? doc->language->id : "--unknown--";
  if (ts_languages.find(langId) == ts_languages.end()) {
    // printf("language not available\n");
    return;
  }
  std::function<const TSLanguage *()> lang = ts_languages[langId];

  TSParser *parser = ts_parser_new();
  if (!ts_parser_set_language(parser, lang())) {
    printf("Invalid language\n");
    return;
  }

  std::string content = u16string_to_string(
      snapshot->text()); /* TODO: parse TSInputEncodingUTF16 */
  TSTree *tree =
      ts_parser_parse_string(parser, NULL /* TODO: old_tree */,
                             (char *)(content.c_str()), content.size());

  if (tree) {
    treesitter->tree = tree;
  } else {
    // printf(">>Error parsing\n");
  }

  ts_parser_delete(parser);
}

#define TREESITTER_TTL 32

TreeSitter::TreeSitter()
    : state(State::Loading), snapshot(0), document(0), ttl(TREESITTER_TTL),
      tree(NULL) {}

TreeSitter::~TreeSitter() {
  if (snapshot) {
    delete snapshot;
  }
  if (tree) {
    ts_tree_delete(tree);
  }
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

  return nodes;
}

void *treeSitter_thread(void *arg) {
  TreeSitter *treesitter = (TreeSitter *)arg;

  build_tree(treesitter);

  treesitter->thread_id = 0;
  treesitter->set_ready();
  return NULL;
}

void TreeSitter::run(TreeSitter *treesitter) {
  pthread_create((pthread_t *)&(treesitter->thread_id), NULL,
                 &treeSitter_thread, (void *)(treesitter));
}