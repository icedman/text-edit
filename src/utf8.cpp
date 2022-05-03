#include "utf8.h"

const char *utf8_to_codepoint(const char *p, unsigned *dst) {
  unsigned res, n;
  switch (*p & 0xf0) {
  case 0xf0:
    res = *p & 0x07;
    n = 3;
    break;
  case 0xe0:
    res = *p & 0x0f;
    n = 2;
    break;
  case 0xd0:
  case 0xc0:
    res = *p & 0x1f;
    n = 1;
    break;
  default:
    res = *p;
    n = 0;
    break;
  }
  while (n--) {
    res = (res << 6) | (*(++p) & 0x3f);
  }
  *dst = res;
  return p + 1;
}

int codepoint_to_utf8(uint32_t utf, char *out) {
  if (utf <= 0x7F) {
    // Plain ASCII
    out[0] = (char)utf;
    out[1] = 0;
    return 1;
  } else if (utf <= 0x07FF) {
    // 2-byte unicode
    out[0] = (char)(((utf >> 6) & 0x1F) | 0xC0);
    out[1] = (char)(((utf >> 0) & 0x3F) | 0x80);
    out[2] = 0;
    return 2;
  } else if (utf <= 0xFFFF) {
    // 3-byte unicode
    out[0] = (char)(((utf >> 12) & 0x0F) | 0xE0);
    out[1] = (char)(((utf >> 6) & 0x3F) | 0x80);
    out[2] = (char)(((utf >> 0) & 0x3F) | 0x80);
    out[3] = 0;
    return 3;
  } else if (utf <= 0x10FFFF) {
    // 4-byte unicode
    out[0] = (char)(((utf >> 18) & 0x07) | 0xF0);
    out[1] = (char)(((utf >> 12) & 0x3F) | 0x80);
    out[2] = (char)(((utf >> 6) & 0x3F) | 0x80);
    out[3] = (char)(((utf >> 0) & 0x3F) | 0x80);
    out[4] = 0;
    return 4;
  } else {
    // error - use replacement character
    out[0] = (char)0xEF;
    out[1] = (char)0xBF;
    out[2] = (char)0xBD;
    out[3] = 0;
    return 0;
  }
}

std::string u16string_to_utf8string(std::u16string text) {
  std::string res;
  for (auto c : text) {
    char tmp[5];
    codepoint_to_utf8(c, (char *)tmp);
    res += tmp;
  }
  return res;
}

std::u16string utf8string_to_u16string(std::string text) {
  std::u16string res;
  char *p = (char *)text.c_str();
  while (*p) {
    unsigned cp;
    p = (char *)utf8_to_codepoint(p, &cp);
    res += (wchar_t)cp;
  }

  return res;
}
