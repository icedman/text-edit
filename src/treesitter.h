#ifndef TE_SITTER_H
#define TE_SITTER_H

#include <core/text-buffer.h>
#include <memory>
#include <string>

extern "C" {
#include <tree_sitter/api.h>
}

class Document;
class TreeSitter {
public:
  enum State { Loading, Ready, Consumed, Disposable };

  TreeSitter();
  ~TreeSitter();

  State state;
  TextBuffer::Snapshot *snapshot;
  Patch patch;
  Document *document;
  TSTree *tree;

  int ttl;
  long thread_id;
  bool reference_ready;

  std::string lang_id;
  std::vector<std::string> identifiers;
  std::string content;

  static void run(TreeSitter *treesitter);
  void set_ready();
  void set_consumed();
  void keep_alive();
  bool is_disposable();

  std::vector<TSNode> walk(int row, int column);
  TSNode node_at(int row, int column);

  static bool is_available(std::string lang_id);

  std::shared_ptr<TreeSitter> reference;
};

typedef std::shared_ptr<TreeSitter> TreeSitterPtr;

#endif // TE_SITTER_H