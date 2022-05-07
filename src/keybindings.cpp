#include "keybindings.h"

#include <map>
#include <string>

std::map<std::string, Command> commands = {
    {"unknown", Command{"", ""}},
    {"escape", Command{"cancel", ""}},
    {"ctrl+q", Command{"quit", ""}},
    // {"ctrl+shift+f", Command{"search_text_in_files", ""}},
    // {"ctrl+p", Command{"search_files", ""}},
    // {"ctrl+f", Command{"search_text", ""}},
    // {"ctrl+g", Command{"jump_to_line", ""}},
    {"ctrl+z", Command{"undo", ""}},
    // {"ctrl+shift+z", Command{"redo", ""}},

    {"backspace", Command{"backspace", ""}},
    {"delete", Command{"delete", ""}},

    {"ctrl+up",     Command{"add_cursor_and_move_up", ""}},
    {"ctrl+down",   Command{"add_cursor_and_move_down", ""}},
    {"ctrl+left",   Command{"move_to_previous_word", ""}},
    {"ctrl+right",  Command{"move_to_next_word", ""}},
    {"up",          Command{"move_up", ""}},
    {"down",        Command{"move_down", ""}},
    {"left",        Command{"move_left", ""}},
    {"right",       Command{"move_right", ""}},
    {"pageup",      Command{"pageup", ""}},
    {"pagedown",    Command{"pagedown", ""}},
    {"home",        Command{"move_to_start_of_line", ""}},
    {"end",         Command{"move_to_end_of_line", ""}},
    {"shift+up",    Command{"move_up", "anchored"}},
    {"shift+down",  Command{"move_down", "anchored"}},
    {"shift+left",  Command{"move_left", "anchored"}},
    {"shift+right", Command{"move_right", "anchored"}},

    {"ctrl+k+ctrl+p", Command{"indent", ""}},
    {"ctrl+k+ctrl+o", Command{"unindent", ""}},
    // {"ctrl+/", Command{"toggle_comment", ""}},
    {"ctrl+c", Command{"copy", ""}},
    {"ctrl+x", Command{"cut", ""}},
    {"ctrl+v", Command{"paste", ""}},
    {"ctrl+a", Command{"select_all", ""}},
    {"ctrl+l", Command{"select_line", ""}},
    {"ctrl+d", Command{"select_word", ""}},
    {"ctrl+shift+d", Command{"duplicate_selection", ""}},
    // {"ctrl+alt+[", Command{"toggle_fold", ""}},
    // {"ctrl+alt+]", Command{"unfold_all", ""}},
    // {"ctrl+s", Command{"save", ""}},
    // {"ctrl+w", Command{"close", ""}},
    // {"ctrl+1", Command{"switch_tab", "0"}},
    // {"ctrl+2", Command{"switch_tab", "1"}},
    // {"ctrl+3", Command{"switch_tab", "2"}},
    // {"ctrl+4", Command{"switch_tab", "3"}},
    // {"ctrl+5", Command{"switch_tab", "4"}},
    // {"ctrl+6", Command{"switch_tab", "5"}},
    // {"ctrl+7", Command{"switch_tab", "6"}},
    // {"ctrl+8", Command{"switch_tab", "7"}},
    // {"ctrl+9", Command{"switch_tab", "8"}},
    // {"ctrl+0", Command{"switch_tab", "9"}},
    // {"ctrl+shift+|", Command{"toggle_pinned", ""}},
    {"ctrl+k", Command{"await", ""}},
    {"ctrl+k+ctrl+u", Command{"selection_to_uppercase", ""}},
    {"ctrl+k+ctrl+l", Command{"selection_to_lowercase", ""}}
};

Command& command_from_keys(std::string keys, std::string previous)
{
    std::map<std::string, Command> *cmds = & commands;
    if (previous != "") {
        keys = previous + "+" + keys;
    }
    if (cmds->contains(keys)) {
        return (*cmds)[keys];
    }
    return (*cmds)["unknown"];
}