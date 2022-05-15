#include "search.h"
#include "document.h"

#include <core/text-buffer.h>
#include <pthread.h>

#include "document.h"

#define SEARCH_TTL 32

Search::Search(std::u16string p, Point first_index)
    : key(p), state(State::Loading), snapshot(0), document(0), selected(0),
      first_index(first_index), ttl(SEARCH_TTL) {}

Search::~Search() {
  if (snapshot) {
    delete snapshot;
  }
}

void Search::set_ready() { state = Search::State::Ready; }

void Search::set_consumed() { state = Search::State::Consumed; }

void Search::keep_alive() { ttl = SEARCH_TTL; }

bool Search::is_disposable() {
  if (state < Search::State::Ready) {
    return false;
  }
  if (matches.size() == 0) {
    ttl /= 2;
  }
  return --ttl <= 0;
}

void *search_thread(void *arg) {
  Search *search = (Search *)arg;
  Document *doc = search->document;
  TextBuffer::Snapshot *snapshot = search->snapshot;

  std::u16string error;
  Regex regex(search->key.c_str(), &error, false, false);

  std::u16string k = search->key;
  search->matches = snapshot->find_all(regex, Range::all_inclusive());

  int idx = 0;
  for (auto m : search->matches) {
    if (search->first_index.row <= m.start.row) {
      search->selected = idx;
      break;
    }
    idx++;
  }

  search->thread_id = 0;
  search->set_ready();

  delete search->snapshot;
  search->snapshot = NULL;
  return NULL;
}

void Search::run(Search *search) {
  pthread_create((pthread_t *)&(search->thread_id), NULL, &search_thread,
                 (void *)(search));
}