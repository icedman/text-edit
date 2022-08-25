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
    regex_t *reg = NULL;
    OnigErrorInfo einfo;

    // std::u16string spattern = u"a(.*)b|[e-f]+";
    std::u16string spattern = u"[a-zA-Z_0-9]+";
    std::string _spattern = u16string_to_string(spattern);
    UChar *pattern = (UChar *)spattern.c_str();

    int opts = ONIG_OPTION_DEFAULT;
    opts = opts | ONIG_OPTION_IGNORECASE;

    int r = onig_new(&reg, pattern, pattern + spattern.size() * 2, opts,
                     ONIG_ENCODING_UTF16_LE, ONIG_SYNTAX_DEFAULT, &einfo);
    if (r != ONIG_NORMAL) {
      OnigUChar s[ONIG_MAX_ERROR_MESSAGE_LEN];
      onig_error_code_to_str(s, r, &einfo);
      // *error_message = u"error";
      printf("ERROR: %s\n", s);
      return -1;
    }

    OnigRegion *region = onig_region_new();
    // int r;
    unsigned char *start, *range, *end;

    unsigned options = 0;
    unsigned int onig_options = ONIG_OPTION_NONE;

    std::u16string ss = u"the quick brown fox";
    std::string _ss = u16string_to_string(ss);

    UChar *str = (UChar *)ss.c_str();
    end = str + ss.size() * 2;
    start = str;
    range = end;
    r = onig_search(reg, str, end, start, range, region, onig_options);
    // printf(">%d\n", r);

    for (int i = 0; i < region->num_regs; i++) {
      printf("%d: (%ld-%ld)\n", i, region->beg[i] / 2, region->end[i] / 2);
      // break;
    }
    return 0;
  }

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
