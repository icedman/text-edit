#include "autocomplete.h"
#include "document.h"

#include <core/text-buffer.h>
#include <pthread.h>

#include "document.h"

#define AUTOCOMPLETE_TTL 32

AutoComplete::AutoComplete(std::u16string p)
    : prefix(p), state(State::Loading), snapshot(0), document(0), selected(0),
      ttl(AUTOCOMPLETE_TTL) {}

AutoComplete::~AutoComplete() {
  if (snapshot) {
    delete snapshot;
  }
}

void AutoComplete::set_ready() { state = AutoComplete::State::Ready; }

void AutoComplete::set_consumed() { state = AutoComplete::State::Consumed; }

void AutoComplete::keep_alive() { ttl = AUTOCOMPLETE_TTL; }

bool AutoComplete::is_disposable() {
  if (state < AutoComplete::State::Ready) {
    return false;
  }
  return --ttl <= 0;
}

void *autocomplete_thread(void *arg) {
  AutoComplete *autocomplete = (AutoComplete *)arg;
  Document *doc = autocomplete->document;
  TextBuffer::Snapshot *snapshot = autocomplete->snapshot;

  std::u16string k = autocomplete->prefix;
  std::vector<TextBuffer::SubsequenceMatch> res =
      snapshot->find_words_with_subsequence_in_range(k, k,
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

  autocomplete->thread_id = 0;
  autocomplete->set_ready();

  delete autocomplete->snapshot;
  autocomplete->snapshot = NULL;
  return NULL;
}

void AutoComplete::run(AutoComplete *autocomplete) {
  pthread_create((pthread_t *)&(autocomplete->thread_id), NULL,
                 &autocomplete_thread, (void *)(autocomplete));
}