#include "keybindings.h"

#include <map>
#include <string>

std::map<std::string, Command> commands = {
    {"unknown", Command{"", ""}},
    {"escape", Command{"cancel", ""}},
    {"ctrl+q", Command{"quit", ""}},
    // {"ctrl+shift+f", Command{"search_text_in_files", ""}},
    {"ctrl+p", Command{"search_files", ""}},
    {"ctrl+f", Command{"search_text", ""}},
    {"ctrl+r", Command{"search_and_replace", ""}},
    {"ctrl+g", Command{"jump_to_line", ""}},
    {"ctrl+z", Command{"undo", ""}},
    {"alt+z", Command{"redo", ""}},

    {"backspace", Command{"backspace", ""}},
    {"delete", Command{"delete", ""}},

    {"ctrl+up", Command{"add_cursor_and_move_up", ""}},
    {"ctrl+down", Command{"add_cursor_and_move_down", ""}},
    {"ctrl+left", Command{"move_to_previous_word", ""}},
    {"ctrl+right", Command{"move_to_next_word", ""}},
    {"up", Command{"move_up", ""}},
    {"down", Command{"move_down", ""}},
    {"left", Command{"move_left", ""}},
    {"right", Command{"move_right", ""}},
    {"pageup", Command{"pageup", ""}},
    {"pagedown", Command{"pagedown", ""}},
    {"home", Command{"move_to_start_of_line", ""}},
    {"end", Command{"move_to_end_of_line", ""}},
    {"shift+up", Command{"move_up", "anchored"}},
    {"shift+down", Command{"move_down", "anchored"}},
    {"shift+left", Command{"move_left", "anchored"}},
    {"shift+right", Command{"move_right", "anchored"}},

    {"alt+up", Command{"focus_up", ""}},
    {"alt+down", Command{"focus_down", ""}},
    {"alt+left", Command{"focus_left", ""}},
    {"alt+right", Command{"focus_right", ""}},

    {"insert", Command{"insert", ""}},
    {"ctrl+c", Command{"copy", ""}},
    {"ctrl+x", Command{"cut", ""}},
    {"ctrl+v", Command{"paste", ""}},
    {"ctrl+a", Command{"select_all", ""}},
    {"ctrl+l", Command{"select_line", ""}},
    {"ctrl+d", Command{"select_word", ""}},

    {"alt+d", Command{"duplicate_selection", ""}},
    {"ctrl+shift+down", Command{"duplicate_line", ""}},

    {"ctrl+s", Command{"save", ""}},
    {"alt+s", Command{"save_as", ""}},
    {"ctrl+w", Command{"close", ""}},
    {"alt+1", Command{"switch_tab", "0"}},
    {"alt+2", Command{"switch_tab", "1"}},
    {"alt+3", Command{"switch_tab", "2"}},
    {"alt+4", Command{"switch_tab", "3"}},
    {"alt+5", Command{"switch_tab", "4"}},
    {"alt+6", Command{"switch_tab", "5"}},
    {"alt+7", Command{"switch_tab", "6"}},
    {"alt+8", Command{"switch_tab", "7"}},
    {"alt+9", Command{"switch_tab", "8"}},
    {"alt+0", Command{"switch_tab", "9"}},

    {"ctrl+k", Command{"await", ""}},
    {"ctrl+k+ctrl+u", Command{"selection_to_uppercase", ""}},
    {"ctrl+k+ctrl+l", Command{"selection_to_lowercase", ""}},
    {"ctrl+k+ctrl+b", Command{"expand_to_block", ""}},
    {"ctrl+k+ctrl+p", Command{"indent", ""}},
    {"ctrl+k+ctrl+o", Command{"unindent", ""}},
    {"ctrl+k+ctrl+j", Command{"toggle_block_fold", ""}},
    {"ctrl+/", Command{"toggle_comment", ""}},
    // {"ctrl+`", Command{"toggle_console", ""}},

    {"ctrl+t", Command{"await", ""}},
    {"ctrl+t+ctrl+r", Command{"toggle_tree_sitter", ""}},
    {"ctrl+t+ctrl+g", Command{"toggle_gutter", ""}},
    {"ctrl+t+ctrl+s", Command{"toggle_statusbar", ""}},
    {"ctrl+t+ctrl+w", Command{"toggle_wrap", ""}},
    {"ctrl+t+ctrl+e", Command{"toggle_explorer", ""}},
    {"ctrl+t+ctrl+b", Command{"toggle_tabs", ""}}

};

std::string keys_from_command_name(std::string name) {
  std::map<std::string, Command> *cmds = &commands;
  for (auto c : commands) {
    if (c.second.command == name) {
      return c.first;
    }
  }
  return "unknown";
}

Command &command_from_keys(std::string keys, std::string previous) {
  std::map<std::string, Command> *cmds = &commands;
  if (keys.size() > 1) {
    if (keys[0] == '!') {
      keys = keys_from_command_name(keys.substr(1));
    }
  }
  if (previous != "") {
    keys = previous + "+" + keys;
  }
  if (cmds->contains(keys)) {
    return (*cmds)[keys];
  }
  return (*cmds)["unknown"];
}
