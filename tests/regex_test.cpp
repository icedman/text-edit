#include <core/encoding-conversion.h>
#include <core/regex.h>
#include <core/text-buffer.h>
#include <core/text.h>
#include <stdio.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "onigmognu.h"
#include "utf8.h"

// regex test
int main(int argc, char **argv) {

  {
    std::u16string pattern = u"a(.*)b|[e-f]+";
    std::u16string str = u"azzzzbffffffffb";

    std::u16string error;
    Regex regex(pattern.c_str(), &error, false, false);
    Regex::MatchData data(regex);
    Regex::MatchResult res =
        regex.match(str.c_str(), strlen((char *)str.c_str()), data);
    printf(">%d %ld %ld\n", res.type, res.start_offset, res.end_offset);
  }

  {
    std::u16string pattern = u"[a-zA-Z_0-9]+";
    std::u16string str = u"for(int i; i<20; i++) { int l=i*2; }";

    std::u16string error;
    Regex regex(pattern.c_str(), &error, false, false);
    Regex::MatchData data(regex);
    Regex::MatchResult res = {Regex::MatchResult::None, 0, 0};

    int offset = 0;
    do {
      offset += res.end_offset;
      char16_t *_str = (char16_t *)str.c_str();
      _str += offset;
      res = regex.match(_str, strlen((char *)_str), data);
      printf(">%d %ld %ld\n", res.type, offset + res.start_offset,
             offset + res.end_offset);
      std::u16string _w = str.substr(offset + res.start_offset,
                                     (res.end_offset - res.start_offset));
      std::string _w8 = u16string_to_string(_w);
      printf(">>%s\n", _w8.c_str());
    } while (res.type == Regex::MatchResult::Full);
  }

  return 0;
}