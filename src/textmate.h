#ifndef TE_TEXTMATE_H
#define TE_TEXTMATE_H

#include <string>

struct theme_color_t {
  int8_t r;
  int8_t g;
  int8_t b;
};

struct theme_info_t {
  int8_t fg_r;
  int8_t fg_g;
  int8_t fg_b;
  int8_t bg_r;
  int8_t bg_g;
  int8_t bg_b;
  int8_t sel_r;
  int8_t sel_g;
  int8_t sel_b;
  // theme_color_t fg;
  // theme_color_t bg;
  // theme_color_t sel;
};

struct textstyle_t {
  int32_t start;
  int32_t length;
  int32_t flags;
  int8_t r;
  int8_t g;
  int8_t b;
  int8_t bg_r;
  int8_t bg_g;
  int8_t bg_b;
  int8_t caret;
  bool bold;
  bool italic;
  bool underline;
  bool strike;
};

struct rgba_t {
  int r;
  int g;
  int b;
  int a;
};

struct span_info_t {
  int start;
  int length;
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
};

#endif // TE_TEXTMATE_H