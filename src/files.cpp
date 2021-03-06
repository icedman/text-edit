#include "files.h"
#include "extensions/util.h"

#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

#define MAX_PRELOAD_DEPTH 4
#define MAX_PRELOAD_THREADS 8

std::string base_name(std::string path) {
  std::set<char> delims = {'\\', '/'};
  std::vector<std::string> spath = split_path(path, delims);
  return spath.back();
}

std::string expanded_path(std::string path) {
  char *cpath = (char *)malloc(path.length() + 1 * sizeof(char));
  strcpy(cpath, path.c_str());
  expand_path((char **)(&cpath));
  std::string full_path = std::string(cpath);
  free(cpath);
  return full_path;
}

std::string directory_path(std::string path, std::string name) {
  return path.substr(0, path.size() - name.size());
}

static bool compare_files(FileItemPtr f1, FileItemPtr f2) {
  if (f1->is_directory && !f2->is_directory) {
    return true;
  }
  if (!f1->is_directory && f2->is_directory) {
    return false;
  }
  return f1->name < f2->name;
}

FileItem::FileItem(std::string p)
    : state(State::Consumed), expanded(false), is_directory(false), depth(0),
      thread_id(0), preloaded(false) {
  set_path(p);
}

FileItem::~FileItem() {}

void FileItem::set_path(std::string path) {
  char buf[PATH_MAX + 1] = "";
  realpath(path.c_str(), buf);
  name = base_name(buf);
  full_path = expanded_path(path);
}

void FileItem::set_ready() { state = FileItem::State::Ready; }

void FileItem::set_consumed() { state = FileItem::State::Consumed; }

void FileItem::keep_alive() {}

bool FileItem::is_disposable() { return false; }

void *filetem_thread(void *arg) {
  FileItem *item = (FileItem *)arg;
  item->files.clear();

  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(item->full_path.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if (ent->d_name[0] == '.') {
        continue;
      }

      std::string fn = ent->d_name;
      std::string fp = item->full_path + "/" + fn;

      size_t pos = fp.find("//");
      if (pos != std::string::npos) {
        fp.replace(fp.begin() + pos, fp.begin() + pos + 2, "/");
      }

      FileItemPtr file = std::make_shared<FileItem>(fp);
      file->is_directory = ent->d_type == DT_DIR;

      item->files.push_back(file);
    }
    closedir(dir);
  }

  std::sort(item->files.begin(), item->files.end(), compare_files);

  item->thread_id = 0;
  item->set_ready();
  return NULL;
}

void FileItem::run(FileItem *item) {
  pthread_create((pthread_t *)&(item->thread_id), NULL, &filetem_thread,
                 (void *)(item));
}

Files::Files() { root = std::make_shared<FileItem>(""); }

void Files::set_root_path(std::string path) {
  std::string fp = expanded_path(path);
  root->set_path(fp);
  root->expanded = true;
  root->is_directory = true;
  load(path);
}

void Files::load(std::string path) {
  FileItemPtr item;

  std::string fp = expanded_path(path);
  // find in tree first
  if (requests.find(fp) == requests.end()) {
    item = std::make_shared<FileItem>(fp);
    requests[fp] = item;
  }

  item = requests[fp];
  if (item->state == FileItem::State::Loading) {
    return;
  }

  item->state = FileItem::State::Loading;
  FileItem::run(item.get());
}

void Files::build_files(FileList &list, FileItemPtr node, int depth,
                        bool expanded_only) {
  list.push_back(node);
  node->depth = depth;
  if (node->is_directory) {
    if (!node->expanded && expanded_only) {
      return;
    }
    for (auto f : node->files) {
      build_files(list, f, depth + 1, expanded_only);
    }
  }
}

void Files::rebuild_tree() {
  _tree.clear();
  build_files(_tree, root, 0, true);
}

void merge_files(FileItemPtr target, FileItemPtr payload, FileList &added_files,
                 FileList &removed_files) {
  for (auto c : payload->files) {
    bool exists = false;
    for (auto cc : target->files) {
      if (c->full_path == cc->full_path) {
        exists = true;
        break;
      }
    }
    if (!exists) {
      added_files.push_back(c);
    }
  }

  for (auto c : target->files) {
    bool exists = false;
    for (auto cc : payload->files) {
      if (c->full_path == cc->full_path) {
        exists = true;
        break;
      }
    }
    if (!exists) {
      removed_files.push_back(c);
    }
  }

  for (auto c : added_files) {
    target->files.push_back(c);
  }
  for (auto c : removed_files) {
    auto it = std::find(target->files.begin(), target->files.end(), c);
    target->files.erase(it);
  }

  // sort
  std::sort(target->files.begin(), target->files.end(), compare_files);
}

bool Files::update() {
  if (files.size() == 0) {
    build_files(files, root);
  }

  bool did_update = false;
  std::vector<std::string> disposables;

  for (auto r : requests) {
    FileItemPtr item = r.second;
    if (item->state == FileItem::State::Consumed) {
      disposables.push_back(r.first);
      continue;
    }
    if (item->state == FileItem::State::Ready) {
      for (auto f : files) {
        if (f->is_directory) {
          if (f->full_path == r.first) {
            FileList added, removed;
            merge_files(f, item, added, removed);
            item->state = FileItem::State::Consumed;
            did_update = true;
            f->preloaded = true;
          }
        }
      }
    }
  }

  // dispose consumed
  for (auto d : disposables) {
    requests.erase(d);
  }

  if (did_update) {
    rebuild_tree();
  }

  if (requests.size() < MAX_PRELOAD_THREADS) {
    for (auto f : files) {
      if (f->depth < MAX_PRELOAD_DEPTH) {
        f->preloaded = true;
      }
      if (!f->preloaded && f->is_directory) {
        load(f->full_path);
      }
    }
  }

  if (did_update) {
    files.clear();
  }
  return did_update;
}

bool Files::has_running_threads() {
  for (auto r : requests) {
    FileItemPtr item = r.second;
    if (item->state < FileItem::State::Ready) {
      return true;
    }
  }
  return false;
}

bool Files::has_unconsumed_requests() { return requests.size() > 0; }

FileList &Files::tree() { return _tree; }