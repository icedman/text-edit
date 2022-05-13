#ifndef TE_TEXTMATE_H
#define TE_TEXTMATE_H

#include "theme.h"
#include <string>
#include <vector>

#define SCOPE_COMMENT (1 << 1)
#define SCOPE_COMMENT_BLOCK (1 << 2)
#define SCOPE_STRING (1 << 3)
#define SCOPE_BRACKET (1 << 4)
#define SCOPE_BRACKET_CURLY (1 << 4)
#define SCOPE_BRACKET_ROUND (1 << 5)
#define SCOPE_BRACKET_SQUARE (1 << 6)
#define SCOPE_BEGIN (1 << 7)
#define SCOPE_END (1 << 8)
#define SCOPE_TAG (1 << 9)
#define SCOPE_VARIABLE (1 << 10)
#define SCOPE_CONSTANT (1 << 11)
#define SCOPE_KEYWORD (1 << 12)
#define SCOPE_ENTITY (1 << 13)
#define SCOPE_ENTITY_CLASS (1 << 14)
#define SCOPE_ENTITY_FUNCTION (1 << 15)

class Block;

struct rgba_t {
  int r;
  int g;
  int b;
  int a;
};

struct theme_info_t {
  int16_t fg_r;
  int16_t fg_g;
  int16_t fg_b;
  int16_t fg_a;
  int16_t bg_r;
  int16_t bg_g;
  int16_t bg_b;
  int16_t bg_a;
  int16_t sel_r;
  int16_t sel_g;
  int16_t sel_b;
  int16_t sel_a;
  int16_t cmt_r;
  int16_t cmt_g;
  int16_t cmt_b;
  int16_t cmt_a;
};

struct textstyle_t {
  int16_t start;
  int16_t length;
  int16_t flags;
  int16_t r;
  int16_t g;
  int16_t b;
  int16_t a;
  int16_t bg_r;
  int16_t bg_g;
  int16_t bg_b;
  int16_t caret;
  bool bold;
  bool italic;
  bool underline;
  bool strike;
};

struct span_info_t {
  int16_t start;
  int16_t length;
  rgba_t fg;
  rgba_t bg;
  bool bold;
  bool italic;
  bool underline;
  std::string scope;
};

class Textmate {
public:
  static void initialize(std::string path);
  static int load_theme(std::string path);
  static int load_language(std::string path);
  static language_info_ptr language_info(int id);
  static std::vector<textstyle_t>
  run_highlighter(char *_text, language_info_ptr lang, theme_ptr theme,
                  Block *block = NULL, Block *prev = NULL, Block *next = NULL);
  static theme_info_t theme_info();
  static theme_ptr theme();
  static bool has_running_threads();
};

#endif // TE_TEXTMATE_H