#include "extension.h"
#include "grammar.h"
#include "parse.h"
#include "reader.h"
#include "theme.h"

#include "textmate.h"

#include <time.h>
#define SKIP_PARSE_THRESHOLD 500

#include <iostream>
#include <string>

#define MAX_STYLED_SPANS 512
#define MAX_BUFFER_LENGTH (1024 * 4)

inline bool color_is_set(rgba_t clr) {
  return clr.r >= 0 && (clr.r != 0 || clr.g != 0 || clr.b != 0 || clr.a != 0);
}

inline textstyle_t construct_style(std::vector<span_info_t> &spans, int index) {
  textstyle_t res = {
      index, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, false,
  };

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

  for (auto span : spans) {
    if (index >= span.start && index < span.start + span.length) {
      if (!color_is_set({res.r, res.g, res.b, 0}) && color_is_set(span.fg)) {
        res.r = span.fg.r;
        res.g = span.fg.g;
        res.b = span.fg.b;
      }
      res.italic = res.italic || span.italic;

      if (span.scope.find("comment.block") == 0) {
        res.flags = res.flags | SCOPE_COMMENT_BLOCK;
      }
      if (span.scope.find("string.quoted") == 0) {
        res.flags = res.flags | SCOPE_STRING;
      }
    }
  }
  return res;
}

inline bool textstyles_equal(textstyle_t &first, textstyle_t &second) {
  return first.italic == second.italic && first.underline == second.underline &&
         first.r == second.r && first.g == second.g && first.b == second.b &&
         first.bg_r == second.bg_r && first.bg_g == second.bg_g &&
         first.bg_b == second.bg_b && first.caret == second.caret &&
         first.flags == second.flags;
}

static extension_list extensions;
static std::vector<theme_ptr> themes;
static icon_theme_ptr icons;
static std::vector<language_info_ptr> languages;

static textstyle_t textstyle_buffer[MAX_STYLED_SPANS];
static char text_buffer[MAX_BUFFER_LENGTH];

theme_ptr current_theme() { return themes[0]; }
theme_ptr Textmate::theme() { return themes[0]; }

void Textmate::initialize(std::string path) {
  load_extensions(path, extensions);
  // for(auto ext : extensions) {
  //     printf("%s\n", ext.name.c_str());
  // }
}

rgba_t theme_color_from_scope_fg_bg(char *scope, bool fore = true) {
  rgba_t res = {-1, 0, 0, 0};
  if (current_theme()) {
    style_t scoped = current_theme()->styles_for_scope(scope);
    color_info_t sclr = scoped.foreground;
    if (!fore) {
      sclr = scoped.background;
    }
    res.r = sclr.red * 255;
    res.g = sclr.green * 255;
    res.b = sclr.blue * 255;
    if (sclr.red == -1) {
      color_info_t clr;
      current_theme()->theme_color(scope, clr);
      if (clr.red == -1) {
        current_theme()->theme_color(
            fore ? "editor.foreground" : "editor.background", clr);
      }
      if (clr.red == -1) {
        current_theme()->theme_color(fore ? "foreground" : "background", clr);
      }
      clr.red *= 255;
      clr.green *= 255;
      clr.blue *= 255;
      res.r = clr.red;
      res.g = clr.green;
      res.b = clr.blue;
    }
  }
  return res;
}

rgba_t theme_color(char *scope) { return theme_color_from_scope_fg_bg(scope); }

theme_info_t themeInfo;
int themeInfoId = -1;

theme_info_t Textmate::theme_info() {
  char _default[32] = "default";
  theme_info_t info;
  color_info_t fg;
  if (current_theme()) {
    current_theme()->theme_color("editor.foreground", fg);
    if (fg.is_blank()) {
      current_theme()->theme_color("foreground", fg);
    }
    if (fg.is_blank()) {
      rgba_t tc = theme_color_from_scope_fg_bg(_default);
      fg.red = (float)tc.r / 255;
      fg.green = (float)tc.g / 255;
      fg.blue = (float)tc.b / 255;
    }
  }

  fg.red *= 255;
  fg.green *= 255;
  fg.blue *= 255;

  color_info_t bg;
  if (current_theme()) {
    current_theme()->theme_color("editor.background", bg);
    if (bg.is_blank()) {
      current_theme()->theme_color("background", bg);
    }
    if (bg.is_blank()) {
      rgba_t tc = theme_color_from_scope_fg_bg(_default, false);
      bg.red = (float)tc.r / 255;
      bg.green = (float)tc.g / 255;
      bg.blue = (float)tc.b / 255;
    }
  }

  bg.red *= 255;
  bg.green *= 255;
  bg.blue *= 255;

  color_info_t sel;
  if (current_theme())
    current_theme()->theme_color("editor.selectionBackground", sel);
  sel.red *= 255;
  sel.green *= 255;
  sel.blue *= 255;

  color_info_t cmt;
  if (current_theme()) {
    // current_theme()->theme_color("comment", cmt);
    style_t style = current_theme()->styles_for_scope("comment");
    cmt = style.foreground;
    if (cmt.is_blank()) {
      current_theme()->theme_color("editor.foreground", cmt);
    }
    if (cmt.is_blank()) {
      rgba_t tc = theme_color_from_scope_fg_bg(_default, false);
      cmt.red = (float)tc.r / 255;
      cmt.green = (float)tc.g / 255;
      cmt.blue = (float)tc.b / 255;
    }
  }

  cmt.red *= 255;
  cmt.green *= 255;
  cmt.blue *= 255;

  info.fg_r = fg.red;
  info.fg_g = fg.green;
  info.fg_b = fg.blue;
  info.fg_a = color_info_t::nearest_color_index(fg.red, fg.green, fg.blue);
  info.bg_r = bg.red;
  info.bg_g = bg.green;
  info.bg_b = bg.blue;
  info.bg_a = color_info_t::nearest_color_index(bg.red, bg.green, bg.blue);
  info.sel_r = sel.red;
  info.sel_g = sel.green;
  info.sel_b = sel.blue;
  info.sel_a = color_info_t::nearest_color_index(sel.red, sel.green, sel.blue);
  info.cmt_r = cmt.red;
  info.cmt_g = cmt.green;
  info.cmt_b = cmt.blue;
  info.cmt_a = color_info_t::nearest_color_index(cmt.red, cmt.green, cmt.blue);

  // why does this happen?
  if (info.sel_r < 0 && info.sel_g < 0 && info.sel_b < 0) {
    info.sel_r *= -1;
    info.sel_g *= -1;
    info.sel_b *= -1;
  }

  return info;
}

int Textmate::load_theme(std::string path) {
  theme_ptr theme = theme_from_name(path, extensions);
  if (theme != NULL) {
    themes.emplace_back(theme);
    return themes.size() - 1;
  }
  return 0;
}

int load_icons(std::string path) {
  icons = icon_theme_from_name(path, extensions);
  return 0;
}

int Textmate::load_language(std::string path) {
  language_info_ptr lang = language_from_file(path, extensions);
  if (lang != NULL) {
    languages.emplace_back(lang);
    return languages.size() - 1;
  }
  return 0;
}

language_info_ptr Textmate::language_info(int id) { return languages[id]; }

void dump_tokens(std::map<size_t, scope::scope_t> &scopes) {
  std::map<size_t, scope::scope_t>::iterator it = scopes.begin();
  while (it != scopes.end()) {
    size_t n = it->first;
    scope::scope_t scope = it->second;
    std::cout << n << " size:" << scope.size()
              << " atoms:" << to_s(scope).c_str() << std::endl;

    it++;
  }
}

std::map<size_t, parse::stack_ptr> parser_states;
std::map<size_t, std::string> block_texts;

std::vector<textstyle_t>
Textmate::run_highlighter(char *_text, language_info_ptr lang, theme_ptr theme,
                          block_data_t *block, block_data_t *prev_block,
                          block_data_t *next_block) {

  std::vector<textstyle_t> textstyle_buffer;

  if (strlen(_text) > SKIP_PARSE_THRESHOLD) {
    return textstyle_buffer;
  }

  // printf("hl %d %s\n", block, _text);

  parse::grammar_ptr gm = lang->grammar;
  themeInfo = theme_info();

  std::map<size_t, scope::scope_t> scopes;

  std::string str = _text;
  str += "\n";

  const char *text = str.c_str();

  size_t l = str.length();
  const char *first = text;
  const char *last = first + l;

  parse::stack_ptr parser_state;
  if (prev_block != NULL) {
    parser_state = prev_block->parser_state;
    block->prev_comment_block = prev_block->comment_block;
    block->prev_string_block = prev_block->string_block;
  }

  bool firstLine = false;
  if (parser_state == NULL) {
    parser_state = gm->seed();
    firstLine = true;
  }

  // TIMER_BEGIN
  parser_state = parse::parse(first, last, parser_state, scopes, firstLine);
  // TIMER_END

  // if ((cpu_time_used > 0.01)) {
  // printf(">>%f %s", cpu_time_used, text);
  // printf("%s\n", text);
  // dump_tokens(scopes);
  // }

  block->parser_state = parser_state;

  std::map<size_t, scope::scope_t>::iterator it = scopes.begin();
  size_t n = 0;

  std::vector<span_info_t> spans;

  while (it != scopes.end()) {
    n = it->first;
    scope::scope_t scope = it->second;
    std::string scopeName(scope);
    style_t style = theme->styles_for_scope(scopeName);

    scopeName = scope.back();
    // printf(">%s %d\n", scopeName.c_str());

    span_info_t span = {.start = (int)n,
                        .length = (int)(l - n),
                        .fg =
                            {
                                (int)(255 * style.foreground.red),
                                (int)(255 * style.foreground.green),
                                (int)(255 * style.foreground.blue),
                                style.foreground.index,
                            },
                        .bg = {0, 0, 0, 0},
                        .bold = style.bold == bool_true,
                        .italic = style.italic == bool_true,
                        .underline = style.underlined == bool_true,
                        // .state = state,
                        .scope = scopeName};

    if (spans.size() > 0) {
      span_info_t &prevSpan = spans.front();
      prevSpan.length = n - prevSpan.start;
    }

    spans.push_back(span);
    it++;
  }

  {
    span_info_t *prev = NULL;
    for (auto &s : spans) {
      if (prev) {
        prev->length = s.start - prev->start;
      }
      prev = &s;
    }
  }

  int idx = 0;
  textstyle_t *prev = NULL;

  // if (spans.size() == 1) {
  //   return textstyle_buffer;
  // }

  for (int i = 0; i < l && i < MAX_STYLED_SPANS; i++) {
    textstyle_buffer.push_back(construct_style(spans, i));
    textstyle_t *ts = &textstyle_buffer[idx];

    if (!color_is_set({ts->r, ts->g, ts->b, 0})) {
      if (ts->r + ts->g + ts->b == 0) {
        ts->r = themeInfo.fg_r;
        ts->g = themeInfo.fg_g;
        ts->b = themeInfo.fg_b;
        ts->a = themeInfo.fg_a;
      }
    }

    if (i > 0 &&
        textstyles_equal(textstyle_buffer[idx], textstyle_buffer[idx - 1])) {
      textstyle_buffer[idx - 1].length++;
      idx--;
    }

    idx++;
  }

  if (idx > 0) {
    block->comment_block =
        (textstyle_buffer[idx - 1].flags & SCOPE_COMMENT_BLOCK);
    block->string_block = (textstyle_buffer[idx - 1].flags & SCOPE_STRING);
  }

  if (next_block) {
    if (next_block->prev_string_block != block->string_block ||
        next_block->prev_comment_block != block->comment_block) {
      next_block->make_dirty();
    }
  }

  return textstyle_buffer;
}

char *language_definition(int langId) {
  language_info_ptr lang = languages[langId];
  std::ostringstream ss;
  ss << lang->definition;
  strcpy(text_buffer, ss.str().c_str());
  return text_buffer;
}

char *icon_for_filename(char *filename) {
  icon_t icon = icon_for_file(icons, filename, extensions);
  strcpy(text_buffer, icon.path.c_str());
  return text_buffer;
}

bool Textmate::has_running_threads() {
  return parse::grammar_t::running_threads > 0;
}