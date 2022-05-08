#include "autocomplete.h"
#include "document.h"

#include <core/text-buffer.h>
#include <pthread.h>

#include "document.h"

AutoComplete::AutoComplete(std::u16string p)
    : prefix(p), state(State::Loading), snapshot(0), buffer(0), document(0),
      selected(0) {}

AutoComplete::~AutoComplete() {
  if (snapshot) {
    delete snapshot;
  }
}

void *autocomplete_thread(void *arg) {
  AutoComplete *autocomplete = (AutoComplete *)arg;
  Document *doc = autocomplete->document;
  TextBuffer *buffer = autocomplete->buffer;

  std::u16string k = autocomplete->prefix;
  std::vector<TextBuffer::SubsequenceMatch> res =
      buffer->find_words_with_subsequence_in_range(k, k,
                                                   Range::all_inclusive());
  for (auto r : res) {
    if (r.score < 0)
      break;
    if (r.word == k)
      continue;
    autocomplete->matches.push_back(AutoComplete::Match{r.word, r.score});
    if (autocomplete->matches.size() > 20)
      break;
  }

  autocomplete->state = AutoComplete::State::Ready;
  autocomplete->thread_id = 0;
}

void AutoComplete::run(AutoComplete *autocomplete) {
  pthread_create((pthread_t *)&(autocomplete->thread_id), NULL,
                 &autocomplete_thread, (void *)(autocomplete));
}