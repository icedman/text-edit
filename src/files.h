#ifndef TE_FILES_H
#define TE_FILES_H

#include <map>
#include <memory>
#include <string>
#include <vector>

class FileItem;
typedef std::shared_ptr<FileItem> FileItemPtr;
typedef std::vector<FileItemPtr> FileList;

class FileItem {
public:
  enum State { Loading, Ready, Consumed, Disposable };

  FileItem(std::string p);
  ~FileItem();

  State state;

  std::string name;
  std::string full_path;

  bool expanded;
  bool is_directory;
  int depth;

  FileList files;

  int ttl;
  int thread_id;

  void set_path(std::string path);

  static void run(FileItem *FileItem);
  void set_ready();
  void set_consumed();
  void keep_alive();
  bool is_disposable();
};

class Files {
public:
  Files();

  FileItemPtr root;
  std::map<std::string, FileItemPtr> requests;

  void build_files(FileList &list, FileItemPtr node, int depth = 0,
                   bool expanded_only = false);
  void rebuild_tree();

  void set_root_path(std::string path);
  void load(std::string path);
  bool update();

  bool has_running_threads();
  bool has_unconsumed_requests();

  FileList &tree();
  FileList _tree;
};

typedef std::shared_ptr<Files> FilesPtr;

#endif // TE_FILES_H