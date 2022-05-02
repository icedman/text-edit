#include <stdio.h>
#include <core/text-buffer.h>
#include <core/text.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

#include <time.h>
#include <curses.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k)&0x1f)

int width = 0;
int height = 0;

enum KEY_ACTION {
    K_NULL = 0, /* NULL */
    K_ENTER = 13,
    K_TAB = 9,
    K_ESC = 27,
    K_BACKSPACE = 127,
    K_RESIZE = 150,
    /* The following are just soft codes, not really reported by the terminal directly. */
    K_ALT_ = 1000,
    K_CTRL_,
    K_CTRL_ALT_,
    K_CTRL_SHIFT_,
    K_CTRL_SHIFT_ALT_,
    K_CTRL_UP,
    K_CTRL_DOWN,
    K_CTRL_LEFT,
    K_CTRL_RIGHT,
    K_CTRL_HOME,
    K_CTRL_END,
    K_CTRL_SHIFT_UP,
    K_CTRL_SHIFT_DOWN,
    K_CTRL_SHIFT_LEFT,
    K_CTRL_SHIFT_RIGHT,
    K_CTRL_SHIFT_HOME,
    K_CTRL_SHIFT_END,
    K_CTRL_ALT_UP,
    K_CTRL_ALT_DOWN,
    K_CTRL_ALT_LEFT,
    K_CTRL_ALT_RIGHT,
    K_CTRL_ALT_HOME,
    K_CTRL_ALT_END,
    K_CTRL_SHIFT_ALT_LEFT,
    K_CTRL_SHIFT_ALT_RIGHT,
    K_CTRL_SHIFT_ALT_HOME,
    K_CTRL_SHIFT_ALT_END,

    K_SHIFT_HOME,
    K_SHIFT_END,
    K_HOME_KEY,
    K_END_KEY,
    K_PAGE_UP,
    K_PAGE_DOWN
};

#define TIMER_BEGIN                                                            \
  clock_t start, end;                                                          \
  double cpu_time_used;                                                        \
  start = clock();

#define TIMER_RESET                                                            \
  start = clock();
  
#define TIMER_END                                                              \
  end = clock();                                                               \
  cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

// if != 0, then there is data to be read on stdin
int kbhit(int timeout = 500)
{
    // timeout structure passed into select
    struct timeval tv;
    // fd_set passed into select
    fd_set fds;
    // Set up the timeout.  here we can wait for 1 second
    tv.tv_sec = 0;
    tv.tv_usec = timeout;

    // Zero out the fd_set - make sure it's pristine
    FD_ZERO(&fds);
    // Set the FD that we want to read
    FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
    // select takes the last file descriptor value + 1 in the fdset to check,
    // the fdset for reads, writes, and errors.  We are only passing in reads.
    // the last parameter is the timeout.  select will return if an FD is ready or
    // the timeout has occurred
    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    // return 0 if STDIN is not ready to be read.

    return FD_ISSET(STDIN_FILENO, &fds);
}

static int readMoreEscapeSequence(int c, std::string& keySequence)
{
    char tmp[32];

    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')) {
        sprintf(tmp, "alt+%c", c);
        keySequence = tmp;
        return K_ALT_;
    }

    if (c >= 'A' && c <= 'Z') {
        sprintf(tmp, "alt+shift+%c", (c + 'a' - 'A'));
        keySequence = tmp;
        return K_ALT_;
    }

    if ((c + 'a' - 1) >= 'a' && (c + 'a' - 1) <= 'z') {
        sprintf(tmp, "ctrl+alt+%c", (c + 'a' - 1));
        keySequence = tmp;
        return K_CTRL_ALT_;
    }

    printf("escape+%d a:%d A:%d 0:%d 9:%d\n", c, 'a', 'A', '0', '9');

    return K_ESC;
}

static int readEscapeSequence(std::string& keySequence)
{
    keySequence = "";
    std::string sequence = "";

    char seq[4];
    int wait = 500;

    if (!kbhit(wait)) {
        return K_ESC;
    }
    read(STDIN_FILENO, &seq[0], 1);

    if (!kbhit(wait)) {
        return readMoreEscapeSequence(seq[0], keySequence);
    }
    read(STDIN_FILENO, &seq[1], 1);

    /* ESC [ sequences. */
    if (seq[0] == '[') {
        if (seq[1] >= '0' && seq[1] <= '9') {

            /* Extended escape, read additional byte. */
            if (!kbhit(wait)) {
                return K_ESC;
            }
            read(STDIN_FILENO, &seq[2], 1);

            if (seq[2] == '~') {
                switch (seq[1]) {
                case '3':
                    keySequence = "delete";
                    return KEY_DC;
                case '5':
                    keySequence = "pageup";
                    return K_PAGE_UP;
                case '6':
                    keySequence = "pagedown";
                    return K_PAGE_DOWN;
                }
            }

            if (seq[2] == ';') {
                if (!kbhit(wait)) {
                    return K_ESC;
                }
                read(STDIN_FILENO, &seq[0], 1);
                if (!kbhit(wait)) {
                    return K_ESC;
                }
                read(STDIN_FILENO, &seq[1], 1);

                sequence = "shift+";
                if (seq[0] == '2') {
                    // printf("shift+%d\n", seq[1]);
                    switch (seq[1]) {
                    case 'A':
                        keySequence = sequence + "up";
                        return KEY_SR;
                    case 'B':
                        keySequence = sequence + "down";
                        return KEY_SF;
                    case 'C':
                        keySequence = sequence + "right";
                        return KEY_SRIGHT;
                    case 'D':
                        keySequence = sequence + "left";
                        return KEY_SLEFT;
                    case 'H':
                        keySequence = sequence + "home";
                        return K_SHIFT_HOME;
                    case 'F':
                        keySequence = sequence + "end";
                        return K_SHIFT_END;
                    }
                }

                sequence = "ctrl+";
                if (seq[0] == '5') {
                    // printf("ctrl+%d\n", seq[1]);
                    switch (seq[1]) {
                    case 'A':
                        keySequence = sequence + "up";
                        return K_CTRL_UP;
                    case 'B':
                        keySequence = sequence + "down";
                        return K_CTRL_DOWN;
                    case 'C':
                        keySequence = sequence + "right";
                        return K_CTRL_RIGHT;
                    case 'D':
                        keySequence = sequence + "left";
                        return K_CTRL_LEFT;
                    case 'H':
                        keySequence = sequence + "home";
                        return K_CTRL_HOME;
                    case 'F':
                        keySequence = sequence + "end";
                        return K_CTRL_END;
                    }
                }

                sequence = "ctrl+shift+";
                if (seq[0] == '6') {
                    // printf("ctrl+shift+%d\n", seq[1]);
                    switch (seq[1]) {
                    case 'A':
                        keySequence = sequence + "up";
                        return K_CTRL_SHIFT_UP;
                    case 'B':
                        keySequence = sequence + "down";
                        return K_CTRL_SHIFT_DOWN;
                    case 'C':
                        keySequence = sequence + "right";
                        return K_CTRL_SHIFT_RIGHT;
                    case 'D':
                        keySequence = sequence + "left";
                        return K_CTRL_SHIFT_LEFT;
                    case 'H':
                        keySequence = sequence + "home";
                        return K_CTRL_SHIFT_HOME;
                    case 'F':
                        keySequence = sequence + "end";
                        return K_CTRL_SHIFT_END;
                    }
                }

                sequence = "ctrl+alt+";
                if (seq[0] == '7') {
                    // printf("ctrl+alt+%d\n", seq[1]);
                    switch (seq[1]) {
                    case 'A':
                        keySequence = sequence + "up";
                        return K_CTRL_ALT_UP;
                    case 'B':
                        keySequence = sequence + "down";
                        return K_CTRL_ALT_DOWN;
                    case 'C':
                        keySequence = sequence + "right";
                        return K_CTRL_ALT_RIGHT;
                    case 'D':
                        keySequence = sequence + "left";
                        return K_CTRL_ALT_LEFT;
                    case 'H':
                        keySequence = sequence + "home";
                        return K_CTRL_ALT_HOME;
                    case 'F':
                        keySequence = sequence + "end";
                        return K_CTRL_ALT_END;
                    }
                }

                sequence = "ctrl+shift+alt+";
                if (seq[0] == '8') {
                    // printf("ctrl+shift+alt+%d\n", seq[1]);
                    switch (seq[1]) {
                    case 'A':
                        keySequence = sequence + "up";
                        return KEY_SR;
                    case 'B':
                        keySequence = sequence + "down";
                        return KEY_SF;
                    case 'C':
                        keySequence = sequence + "right";
                        return K_CTRL_SHIFT_ALT_RIGHT;
                    case 'D':
                        keySequence = sequence + "left";
                        return K_CTRL_SHIFT_ALT_LEFT;
                    case 'H':
                        keySequence = sequence + "home";
                        return K_CTRL_SHIFT_ALT_HOME;
                    case 'F':
                        keySequence = sequence + "end";
                        return K_CTRL_SHIFT_ALT_END;
                    }
                }

                return K_ESC;
            }

        }
    }

    if (seq[0] == '[' || seq[0] == 'O') {
        switch (seq[1]) {
        case 'A':
            keySequence = "up";
            return KEY_UP;
        case 'B':
            keySequence = "down";
            return KEY_DOWN;
        case 'C':
            keySequence = "right";
            return KEY_RIGHT;
        case 'D':
            keySequence = "left";
            return KEY_LEFT;
        case 'H':
            keySequence = "home";
            return K_HOME_KEY;
        case 'F':
            keySequence = "end";
            return K_END_KEY;
        }
        printf("escape+%c+%c\n", seq[0], seq[1]);
    }

    /* ESC O sequences. */
    // else if (seq[0] == 'O') {
    //     printf("escape+O+%d\n", seq[1]);
    //     switch (seq[1]) {
    //     case 'H':
    //         return K_HOME_KEY;
    //     case 'F':
    //         return K_END_KEY;
    //     }
    // }

    return K_ESC;
}

int readKey(std::string& keySequence)
{
    if (kbhit(100) != 0) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 0) {

            if (c == K_ESC) {
                return readEscapeSequence(keySequence);
            }

            switch (c) {
            case K_TAB:
                keySequence = "tab";
                return K_TAB;
            case K_ENTER:
                keySequence = "enter";
                return c;
            case K_BACKSPACE:
            case KEY_BACKSPACE:
                keySequence = "backspace";
                return c;
            case K_RESIZE:
            case KEY_RESIZE:
                keySequence = "resize";
                return c;
            }

            if (CTRL_KEY(c) == c) {
                keySequence = "ctrl+";
                c = 'a' + (c - 1);
                if (c >= 'a' && c <= 'z') {
                    keySequence += c;
                    return c;
                } else {
                    keySequence += '?';
                }

                printf("ctrl+%d\n", c);
                return c;
            }

            return c;
        }
    }
    return -1;
}

const char* utf8_to_codepoint(const char* p, unsigned* dst)
{
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

int codepoint_to_utf8(uint32_t utf, char* out)
{
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

std::string u16string_to_utf8string(std::u16string text)
{
    std::string res;
    for (auto c : text) {
        char tmp[5];
        codepoint_to_utf8(c, (char*)tmp);
        res += tmp;
    }
    return res;
}

std::u16string utf8string_to_u16string(std::string text)
{
    std::u16string res;
    char* p = (char*)text.c_str();
    while (*p) {
        unsigned cp;
        p = (char*)utf8_to_codepoint(p, &cp);
        res += (wchar_t)cp;
    }

    return res;
}

void drawLine(int r, const char* text) {
    move(r, 0);
    clrtoeol();
    addstr(text);
}

void drawText(TextBuffer &text, int start)
{
    int lines = text.size();
    for(int i=0;i<lines && i<height;i++) {
        optional<uint32_t> l = text.line_length_for_row(start + i);
        int line = l ? *l : 0;
        optional<std::u16string> row = text.line_for_row(start + i);
        if (row) {
            std::string s = u16string_to_utf8string(*row);
            // printf("index:%d\n", doc.getIndexForLine(i));
            // printf("Line %d, %s\n", i, s.c_str());
            drawLine(i, s.c_str());
            // drawLine(i, (wchar_t*)(*row->c_str()));
        } else {
            drawLine(i, "");
        }
    }
}

int main(int argc, char **argv) {
    // std::ifstream t("./tests/tinywl.c");
    std::ifstream t("./tests/sqlite3.c");
    std::stringstream buffer;
    buffer << t.rdbuf();

    // std::u16string str = u"hello world";
    std::u16string str = utf8string_to_u16string(buffer.str());
    TextBuffer text(str);

    // optional<uint32_t> l = text.line_length_for_row(0);
    // TIMER_BEGIN
    // int lines = text.size();
    // // for(int j=0;j<100;j++)
    // for(int i=0;i<lines;i++) {
    //     optional<uint32_t> l = text.line_length_for_row(i);
    //     int line = l ? *l : 0;
    //     optional<std::u16string> row = text.line_for_row(i);
    //     if (row) {
    //         std::string s = u16string_to_utf8string(*row);
    //         // printf("index:%d\n", doc.getIndexForLine(i));
    //         printf("Line %d, %s\n", i, s.c_str());
    //     }
    // }
    // TIMER_END
    // printf("time: %f\n------\n", cpu_time_used);

    // printf(">%d\n", l);

    setlocale(LC_ALL, "");

    initscr();
    raw();
    keypad(stdscr, true);
    noecho();
    nodelay(stdscr, true);

    use_default_colors();
    start_color();

    curs_set(0);
    clear();

    static struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    width = ws.ws_col;
    height = ws.ws_row;

    std::vector<std::string> keys;

    int start = 0;
    bool end = false;
    while(!end) {
        int size = text.extent().row;
        drawText(text, start);

        std::stringstream status;
        status << start;
        status << "/";
        status << size;
        drawLine(height - 1, status.str().c_str());
        refresh();

        int ch = -1;
        std::string keySequence;
        while (true) {
            ch = readKey(keySequence);
            if (ch != -1) {
                break;
            }
        }
        if (ch == 27 || keySequence == "ctrl+q") {
            end = true;
        }
        if (keySequence == "up") start --;
        if (keySequence == "down") start ++;
        if (keySequence == "pageup") start -= height;
        if (keySequence == "pagedown") start += height;
        if (start + height/2 > size) start = size - height/2;
        if (start < 0) start = 0;
        keys.push_back(keySequence);

        if (keySequence == "" && ch != -1) {
            Range range= Range({Point(start, 0), Point(start, 0)});
            std::u16string k = u"x";
            k[0] = ch;
            text.set_text_in_range(range, k.data());
        }

    }

    endwin();

    for(auto k : keys) {
        printf(">%s\n", k.c_str());
    }
    return 0;
}
