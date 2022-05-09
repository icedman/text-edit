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
  Document *document;
  std::string content;

  int ttl;
  int thread_id;

  TSTree *tree;

  static void run(TreeSitter *treesitter);
  void set_ready();
  void set_consumed();
  void keep_alive();
  bool is_disposable();

  std::vector<TSNode> walk(int row, int column);
};

typedef std::shared_ptr<TreeSitter> TreeSitterPtr;

#endif // TE_SITTER_H