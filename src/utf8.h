#ifndef TE_UTF8_H
#define TE_UTF8_H

#include <string>

const char* utf8_to_codepoint(const char* p, unsigned* dst);
int codepoint_to_utf8(uint32_t utf, char* out);
std::string u16string_to_utf8string(std::u16string text);
std::u16string utf8string_to_u16string(std::string text);

#endif // TE_UTF8_H

