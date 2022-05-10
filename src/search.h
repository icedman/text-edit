#ifndef TE_SEARCH_H
#define TE_SEARCH_H

#include <core/text-buffer.h>
#include <memory>
#include <string>

class Document;
class Search {
public:
  enum State { Loading, Ready, Consumed, Disposable };

  Search(std::u16string p);
  ~Search();

  std::u16string key;
  State state;
  TextBuffer::Snapshot *snapshot;
  Document *document;

  std::vector<Range> matches;
  int selected;
  int ttl;
  int thread_id;

  static void run(Search *Search);
  void set_ready();
  void set_consumed();
  void keep_alive();
  bool is_disposable();
};

typedef std::shared_ptr<Search> SearchPtr;

#endif // TE_SEARCH_H