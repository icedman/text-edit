#include <core/regex.h>
#include <core/text-buffer.h>
#include <core/text.h>
#include <stdio.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "cursor.h"
#include "document.h"
#include "editor.h"
#include "files.h"
#include "highlight.h"
#include "input.h"
#include "js.h"
#include "keybindings.h"
#include "menu.h"
#include "render.h"
#include "theme.h"
#include "ui.h"
#include "utf8.h"
#include "util.h"
#include "view.h"

// symbols
extern const wchar_t *symbol_tab;

// theme
extern int fg;
extern int bg;
extern int hl;
extern int sel;
extern int cmt;
extern int fn;
extern int kw;
extern int var;

int width = 0;
int height = 0;
int last_layout_hash = -1;

std::map<std::string, clock_t> clocks;

void perf_begin_timer(std::string id) { clocks[id] = clock(); }

void perf_end_timer(std::string id) {
  if (clocks.find(id) == clocks.end()) {
    return;
  }
  clock_t start = clocks[id];
  clock_t end = clock();
  double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
  log("%s >%f\n", id.c_str(), cpu_time_used);
}

void delay(int ms) {
  struct timespec waittime;
  waittime.tv_sec = (ms / 1000);
  ms = ms % 1000;
  waittime.tv_nsec = ms * 1000 * 1000;
  nanosleep(&waittime, NULL);
}

bool get_dimensions(int *width, int *height) {
  static struct winsize ws;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  int _width = ws.ws_col;
  int _height = ws.ws_row;
  if (_width != *width || _height != *height) {
    *width = _width;
    *height = _height;
    return true;
  }
  return false;
}

void layout(view_ptr root) {
  _clear();
  int margin = 0;
  root->layout(
      rect_t{margin, margin, width - (margin * 2), height - (margin * 2)});
  root->finalize();
}

void relayout() { last_layout_hash = -1; }

editor_ptr open_document(std::string path, editors_t &editors, view_ptr view) {
  editor_ptr e = editors.add_editor(path);
  e->flex = 1;
  int idx = 0;
  for (auto c : view->children) {
    if (c == e) {
      editors.selected = idx;
      view_t::input_focus = editors.current_editor();
      return e;
    }
    idx++;
  }
  view->add_child(e);
  view_t::input_focus = editors.current_editor();
  return e;
}

void close_current_editor(editors_t &editors, view_ptr view) {
  auto it = std::find(view->children.begin(), view->children.end(),
                      editors.current_editor());
  if (it != view->children.end()) {
    view->children.erase(it);
  }
  editors.close_current_editor();
  view_t::input_focus = editors.current_editor();
}

int main(int argc, char **argv) {
  perf_begin_timer("first render");

  int last_arg = 0;
  std::string file_path;
  if (argc > 1) {
    last_arg = argc - 1;
  }

  const char *defaultTheme = "Dracula";
  const char *argTheme = defaultTheme;
  for (int i = 0; i < argc - 1; i++) {
    if (strcmp(argv[i], "-t") == 0) {
      if (last_arg == i + 1) {
        last_arg = 0;
      }
      argTheme = argv[i + 1];
    }
  }

  if (last_arg != 0) {
    file_path = argv[last_arg];
  }

  JS js;
  Highlight hl;

  js.initialize();
  hl.initialize();

  // js.run_script("log('hello world')");

  hl.load_theme(argTheme);

  FilesPtr files = std::make_shared<Files>();
  // files->load("./libs/tree-sitter-grammars");
  // files->load("./libs");

  menu_ptr menu = std::make_shared<menu_t>();

  view_ptr root = std::make_shared<column_t>();
  view_ptr main = std::make_shared<row_t>();
  status_bar_ptr status = std::make_shared<status_bar_t>();
  root->add_child(main);
  root->add_child(status);

  menu_ptr explorer = std::make_shared<menu_t>();
  explorer->frame.w = 22;
  explorer->focusable = true;
  main->add_child(explorer);

  view_ptr gutter = std::make_shared<view_t>();

  view_ptr editor_views = std::make_shared<view_t>();
  editor_views->flex = 1;

  // the editors
  editors_t editors;
  editor_ptr editor = open_document(file_path, editors, editor_views);
  files->set_root_path(
      directory_path(editor->doc->file_path, editor->doc->name));

  // for(int i=0; i<8; i++)
  // open_document("./src/autocomplete.cpp", editors, editor_views);

  tabs_ptr tabs = std::make_shared<tabs_t>();
  tabs->on_change = [&editors](int selected) {
    // printf(">>%d\n", selected);
    editors.selected = selected;
    return true;
  };

  tabs->frame.h = 1;
  view_ptr tabs_and_content = std::make_shared<column_t>();
  tabs_and_content->flex = 3;
  main->add_child(tabs_and_content);

  view_ptr gutter_and_content = std::make_shared<row_t>();
  gutter_and_content->flex = 1;
  gutter_and_content->add_child(gutter);
  gutter_and_content->add_child(editor_views);
  editor_views->flex = 1;

  tabs_and_content->add_child(tabs);
  tabs_and_content->add_child(gutter_and_content);

  view_ptr sitter = std::make_shared<view_t>();
  main->add_child(sitter);

  main->flex = 1;
  gutter->frame.w = 8;
  sitter->flex = 1;
  sitter->show = false;

  goto_ptr gotoline = std::make_shared<goto_t>();
  find_ptr find = std::make_shared<find_t>();
  cmd_line_ptr cmdline = std::make_shared<cmd_line_t>();
  root->add_child(gotoline);
  root->add_child(find);
  root->add_child(cmdline);

  std::vector<editor_ptr> input_popups = {gotoline->input, find->input,
                                          find->replace, cmdline->input};

  // callbacks
  explorer->on_submit = [&files, &explorer, &editors, &editor_views](int idx) {
    FileList &tree = files->tree();
    FileItemPtr item = tree[idx];
    if (item->is_directory) {
      item->expanded = !item->expanded;
      if (item->expanded) {
        files->load(item->full_path);
      } else {
        files->rebuild_tree();
        explorer->items.clear();
      }
    } else {
      open_document(item->full_path, editors, editor_views);
    }
    return true;
  };

  explorer->show = false; // files->root->name.size() > 0;

  init_renderer();

  // printf("\x1b[?2004h");
  update_colors();

  _curs_set(0);
  _clear();

  get_dimensions(&width, &height);

  std::stringstream message;
  message << "Welcome to text-edit";

  int warm_start = 3;
  std::string last_key_sequence;
  bool did_first_render = false;

  editor_ptr prev;
  bool running = true;
  while (running) {
    if (!editors.editors.size()) {
      break;
    }

    // perf_begin_timer("render");

    tabs->show = editors.editors.size() > 1;

    if (prev != editors.current_editor()) {
      editor = editors.current_editor();
      layout(root);
    }
    prev = editor;
    DocumentPtr doc = editor->doc;
    TextBuffer &text = doc->buffer;
    int size = doc->size();

    bool has_input = false;
    for (auto i : input_popups) {
      bool has_children_focus = i->parent->has_children_focus();
      if (has_children_focus != i->parent->show) {
        i->parent->show = has_children_focus;
        relayout();
      }
      has_input = has_input | i->has_focus();
    }
    // status->show = !has_input;

    if (last_layout_hash != size) {
      std::stringstream s;
      s << "  ";
      s << size;
      gutter->frame.w = s.str().size();
      layout(root);
      last_layout_hash = size;
    }

    // explorer
    draw_menu(explorer, pair_for_color(cmt, false, false),
              explorer->has_focus() ? pair_for_color(fn, false, true)
                                    : pair_for_color(cmt, false, false));

    draw_tabs(tabs, editors);
    for (auto e : editors.editors) {
      draw_text_buffer(e, !did_first_render ? height : -1);
      draw_gutter(e, gutter);
    }

    SearchPtr search = doc->search();
    if (find->show && search) {
      if (search) {
        message.str("");
        message << " ";
        if (search->matches.size()) {
          message << (search->selected + 1);
          message << " of ";
        }
        message << search->matches.size();
        message << " found";

        if (search->selected < search->matches.size()) {
          Range range = search->matches[search->selected];
          Cursor &cursor = doc->cursor();
          if (range != cursor) {
            cursor.start = range.start;
            cursor.end = range.end;
            editor->on_input(-1, ""); // ping
            continue;
          }
        }
      }
    }

    if (find->show) {
      find->items["title"]->color = find->input->has_focus()
                                        ? pair_for_color(fn, false, false)
                                        : pair_for_color(cmt, false, false);
      find->items["replace"]->color = find->replace->has_focus()
                                          ? pair_for_color(fn, false, false)
                                          : pair_for_color(cmt, false, false);
    }

    if (gotoline->show) {
      gotoline->items["title"]->color = gotoline->input->has_focus()
                                            ? pair_for_color(fn, false, false)
                                            : pair_for_color(cmt, false, false);
    }

    if (cmdline->show) {
      cmdline->items["title"]->color = cmdline->input->has_focus()
                                           ? pair_for_color(fn, false, false)
                                           : pair_for_color(cmt, false, false);
    }

    // other inputs
    for (auto input : input_popups) {
      if (!input->parent->show) {
        continue;
      }
      draw_text_buffer(input);
      draw_status((status_line_t *)(input->parent));
    }

    Cursor cursor = doc->cursor();

    // status
    if (status->show) {
      std::stringstream ss;
      if (doc->insert_mode) {
        ss << "INS   ";
      } else {
        ss << "OVR   ";
      }
      if (doc->language) {
        ss << doc->language->id;
        ss << "  ";
      }
      // ss << " line ";
      ss << (cursor.start.row + 1);
      // ss << " col ";
      ss << ",";
      ss << (cursor.start.column + 1);
      ss << "  ";
      // ss << doc->buffer.extent().column;

      if (size > 0) {
        int p = (100 * cursor.start.row) / size;
        if (p == 0) {
          ss << "top";
        } else if (cursor.start.row + 1 >= size) {
          ss << "end";
        } else {
          ss << p;
          ss << "%";
        }
      }

      status->items["message"]->text = message.str();
      status->items["info"]->text = ss.str();
      status->items["message"]->color = pair_for_color(cmt, false, false);
      status->items["info"]->color = pair_for_color(cmt, false, false);
      draw_status(status.get());
    }

    // tree sitter
    TreeSitterPtr treesitter = doc->treesitter();
    if (treesitter) {
      draw_tree_sitter(editor, sitter, treesitter, doc->cursor());
    }

    // auto complete menu
    menu->show = false;
    menu->items.clear();
    AutoCompletePtr autocomplete = doc->autocomplete();
    if (!menu->show && autocomplete) {
      int margin = 1;
      int w = 0;
      for (auto m : autocomplete->matches) {
        if (w < m.string.size()) {
          w = m.string.size();
        }
        menu->items.push_back(menu_item_t{u16string_to_string(m.string)});
      }
      w += margin * 2;
      menu->computed.w = w;
      menu->computed.h = menu->items.size();
      if (menu->computed.h > 10) {
        menu->computed.h = 10;
      }
      int h = menu->computed.h;

      int offset_row = 1;
      int screen_row = editor->cursor.y;
      int screen_col = editor->cursor.x;

      optional<Range> sub = doc->subsequence_range();
      if (sub) {
        screen_col -= ((*sub).end.column - (*sub).start.column);
      }
      if (editor->cursor.x + w > width) {
        screen_col = editor->cursor.x - w;
      }
      if (editor->cursor.y + 2 + h > height) {
        offset_row = -(h + 1);
      }

      screen_col -= margin;
      menu->computed.x = screen_col;
      menu->computed.y = screen_row + offset_row;
      menu->scroll.x = 0;
      menu->update_scroll();

      menu->on_submit = [&doc](int selected) {
        AutoCompletePtr autocomplete = doc->autocomplete();
        optional<Range> range = doc->subsequence_range();
        if (range) {
          std::u16string value = autocomplete->matches[selected].string;
          doc->clear_cursors();
          doc->cursor().copy_from(Cursor{(*range).start, (*range).end});
          doc->insert_text(value);
        }
        doc->clear_autocomplete(true);
        return true;
      };

      menu->on_cancel = [&doc]() {
        doc->clear_autocomplete(true);
        return true;
      };

      menu->show = (menu->items.size() > 0);
    }
    draw_menu(menu);

    // blit
    _move(view_t::input_focus->cursor.y, view_t::input_focus->cursor.x);
    _refresh();
    _curs_set(1);

    if (!did_first_render) {
      perf_end_timer("first render");
      did_first_render = true;
    } else {
      // perf_end_timer("render");
    }

    // input
    int ch = -1;
    std::string key_sequence;
    int frames = 0;
    int idle = 0;
    while (running) {
      ch = readKey(key_sequence);
      if (ch != -1) {
        break;
      }
      frames++;

      // highlight request
      if (editor->request_highlight) {
        break;
      }

#if ENABLE_JS
      if (frames % 2 == 0) {
        js.loop();
      }
#endif

      // background tasks
      if (files->update() || !explorer->items.size()) {
        explorer->items.clear();
        FileList &tree = files->tree();
        for (auto f : tree) {
          std::string n;
          int d = f->depth;
          for (int i = 0; i < d * 1; i++) {
            n += " ";
          }

          if (f->is_directory) {
            if (f->expanded) {
              n += "- ";
            } else {
              n += "+ ";
            }
          } else {
            n += " ";
          }

          n += f->name;
          explorer->items.push_back(menu_item_t{n, f->full_path});
        }
        break;
      }

      if (editor->on_idle(frames)) {
        break;
      }

      // check dimensions
      if (frames == 1800 && get_dimensions(&width, &height)) {
        layout(root);
        editor->doc->make_dirty();
        break;
      }

      // loop
      if (frames > 2000) {
        frames = 0;
        idle++;
        if (hl.has_running_threads() || (warm_start-- > 0)) {
          editor->doc->make_dirty();
          break;
        }
      }

      if (idle > 8) {
        delay(50);
        if (idle > 32) {
          delay(250);
          _curs_set(0);
        }
      }
    }

    _curs_set(0);
    if (ch == 27) {
      key_sequence = "escape";
      ch = -1;
    }

    // todo .. move somewhere?
    if (find->show) {
      if (key_sequence == "tab" && find->enable_replace) {
        if (view_t::input_focus != find->replace) {
          view_t::input_focus = find->replace;
        } else {
          view_t::input_focus = find->input;
        }
        continue;
      }
    }

    if (search) {
      if (key_sequence == "up") {
        search->selected--;
      }
      if (key_sequence == "down") {
        search->selected++;
      }
      if (search->selected < 0) {
        search->selected = 0;
      }
      if (search->selected >= search->matches.size()) {
        search->selected = search->matches.size() - 1;
      }
    }

    if (view_t::input_focus == editor && menu->show) {
      if (menu->on_input(ch, key_sequence)) {
        continue;
      }
    }
    if (view_t::input_focus->on_input(ch, key_sequence)) {
      if (!view_t::input_focus) {
        view_t::input_focus = editors.current_editor();
      }
      view_t::input_focus->update_scroll();
      continue;
    }

    // globl commands
    Command &cmd = command_from_keys(key_sequence, last_key_sequence);
    if (cmd.command == "await") {
      last_key_sequence = key_sequence;
      continue;
    }
    last_key_sequence = "";

    if (key_sequence != "") {
      message.str("");
      message << "[";
      if (cmd.command != "") {
        message << cmd.command;
        if (cmd.params != "") {
          message << ":";
          message << cmd.params;
        }
      }
      message << "] ";

      message << key_sequence;
      message << " ";
      message << doc->buffer.is_modified();
    }

    if (cmd.command == "toggle_explorer") {
      if (!explorer->show) {
        explorer->show = (explorer->show = files->root->name.size() > 0);
        if (explorer->show) {
          view_t::input_focus = explorer;
          relayout();
        }
        continue;
      } else if (explorer->show) {
        explorer->show = false;
        view_t::input_focus = editor;
        relayout();
        continue;
      }
    }
    if (cmd.command == "toggle_tabs") {
      if (!tabs->show) {
        tabs->show = true;
        view_t::input_focus = tabs;
        relayout();
        continue;
      } else if (tabs->show) {
        tabs->show = false;
        view_t::input_focus = editor;
        relayout();
        continue;
      }
    }

    if (cmd.command == "switch_tab") {
      int idx = std::stoi(cmd.params);
      if (idx < editors.editors.size()) {
        editors.selected = idx;
        view_t::input_focus = editors.current_editor();
      }
    }

    if (cmd.command == "focus_left") {
      view_ptr next = view_t::find_next_focus(root, view_t::input_focus, -1, 0);
      if (next) {
        view_t::input_focus = next;
      }
    }
    if (cmd.command == "focus_right") {
      view_ptr next = view_t::find_next_focus(root, view_t::input_focus, 1, 0);
      if (next) {
        view_t::input_focus = next;
      }
    }
    if (cmd.command == "focus_up") {
      view_ptr next = view_t::find_next_focus(root, view_t::input_focus, 0, -1);
      if (next) {
        view_t::input_focus = next;
      }
    }
    if (cmd.command == "focus_down") {
      view_ptr next = view_t::find_next_focus(root, view_t::input_focus, 0, 1);
      if (next) {
        view_t::input_focus = next;
      }
    }

    // todo move to view
    if (cmd.command == "search_text" || cmd.command == "search_and_replace") {
      view_t::input_focus = find->input;
      find->enable_replace = cmd.command == "search_and_replace";
      find->input->on_input(-1, "!select_all"); // << todo
      find->input->cursor = {0, 0};
      find->input->on_submit = [&editor, &find](std::u16string value) {
        editor->doc->run_search(value, editor->doc->cursor().start);
        editor->doc->clear_cursors();

        if (find->input->has_focus() && editor->doc->search()) {
          editor->doc->search()->selected++;
          if (editor->doc->search()->selected >=
              editor->doc->search()->matches.size()) {
            editor->doc->search()->selected = 0;
          }
        }
        return true;
      };
      find->replace->cursor = {0, 0};
      find->replace->on_submit = [&editor, &search,
                                  &find](std::u16string value) {
        if (!search || search->matches.size() <= 0) {
          find->input->on_input(-1, "enter");
          return true;
        }
        if (search->selected >= 0) {
          editor->doc->insert_text(value);
          search->matches.erase(search->matches.begin() + search->selected);
        }
        if (search->selected >= search->matches.size()) {
          search->selected = search->matches.size() - 1;
        } else {
          search->selected = 0;
        }
        return true;
      };
      find->replace_title->show = find->enable_replace;
      find->replace->show = find->enable_replace;
      if (find->enable_replace && search && search->matches.size()) {
        view_t::input_focus = find->replace;
      }
      relayout();
    }
    if (cmd.command == "jump_to_line") {
      view_t::input_focus = gotoline->input;
      gotoline->input->cursor = {0, 0};
      gotoline->input->on_submit = [&editor, &message](std::u16string value) {
        view_t::input_focus = editor;
        try {
          int line = std::stoi(u16string_to_string(value)) - 1;
          editor->doc->go_to_line(line);
          editor->on_input(-1, ""); // ping
        } catch (std::exception &e) {
          message.str(e.what());
        }
        return true;
      };
      relayout();
    }

#if ENABLE_JS
    if (cmd.command == "show_command_line") {
      view_t::input_focus = cmdline->input;
      cmdline->input->cursor = {0, 0};
      cmdline->input->on_submit = [&editor, &message, &js,
                                   &cmdline](std::u16string value) {
        view_t::input_focus = editor;
        try {
          if (value.size() > 0) {
            cmdline->history.push_back(value);
            cmdline->selected = -1;
            cmdline->input->clear();
            std::string script = u16string_to_string(value);
            js.run_script(script, "<input>");
          }
        } catch (std::exception &e) {
          message.str(e.what());
        }
        return true;
      };
      relayout();
    }

    if (view_t::input_focus == cmdline->input) {
      if (key_sequence == "up") {
        cmdline->selected++;
        cmdline->select_history();
      }
      if (key_sequence == "down") {
        log("down %d", cmdline->selected);
        cmdline->selected--;
        log(" after down %d", cmdline->selected);
        cmdline->select_history();
      }
    }
#endif

    if (cmd.command == "cancel") {
      if (view_t::input_focus != editor) {
        view_t::input_focus = editor;
        editor->doc->clear_search(true);
      }
    }
    if (cmd.command == "toggle_tree_sitter") {
      sitter->show = !sitter->show;
      layout(root);
      editor->doc->make_dirty();
    }
    if (cmd.command == "toggle_gutter") {
      gutter->show = !gutter->show;
      layout(root);
      editor->doc->make_dirty();
    }
    if (cmd.command == "toggle_statusbar") {
      status->show = !status->show;
      layout(root);
      editor->doc->make_dirty();
    }
    if (cmd.command == "quit") {
      running = false;
    }
    if (cmd.command == "close") {
      close_current_editor(editors, editor_views);
    }
  }

  shutdown_renderer();

  // graceful exit... shutting down...
  int idx = 20;
  while ((hl.has_running_threads() || files->has_running_threads()) &&
         idx-- > 0) {
    delay(50);
  }

  js.shutdown();
  hl.shutdown();
  return 0;
}
