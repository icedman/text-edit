#ifndef TE_KEYBINDING_H
#define TE_KEYBINDING_H

#include <string>
#include <map>

class Command {
public:
  std::string command;
  std::string params;
};

Command& command_from_keys(std::string keys, std::string previous);

#endif // TE_KEYBINDING_H
