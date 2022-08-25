#include "files.h"
#include <time.h>

void delay(int ms) {
  struct timespec waittime;
  waittime.tv_sec = (ms / 1000);
  ms = ms % 1000;
  waittime.tv_nsec = ms * 1000 * 1000;
  nanosleep(&waittime, NULL);
}

int main(int argc, char **argv) {
  FilesPtr files = std::make_shared<Files>();
  files->set_root_path("./");
  files->load("./libs/tree-sitter-grammars");
  files->load("./libs");

  while (files->has_running_threads() || files->has_unconsumed_requests()) {
    files->update();
    delay(50);
  }

  FileList tree;
  files->build_files(tree, files->root);
  for (auto f : tree) {
    for (int i = 0; i < f->depth; i++) {
      printf("  ");
    }
    printf("%s\n", f->full_path.c_str());
  }

  return 0;
}