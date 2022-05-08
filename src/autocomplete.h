#ifndef TE_AUTOCOMPLETE_H
#define TE_AUTOCOMPLETE_H

#include <core/text-buffer.h>
#include <memory>
#include <string>

class Document;
class AutoComplete {
public:
  enum State { Loading, Ready, Consumed, Disposable };

  AutoComplete(std::u16string p);
  ~AutoComplete();

  std::u16string prefix;
  State state;
  TextBuffer *buffer;
  TextBuffer::Snapshot *snapshot;
  Document *document;

  class Match {
  public:
    std::u16string string;
    int score;
  };

  std::vector<Match> matches;
  int selected;

  int thread_id;

  static void run(AutoComplete *autocomplete);
};

typedef std::shared_ptr<AutoComplete> AutoCompletePtr;

#endif // TE_AUTOCOMPLETE_H