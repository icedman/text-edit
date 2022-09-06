#ifndef TE_JS_H
#define TE_JS_H

extern "C" {
#include "quickjs-libc.h"
#include "quickjs.h"
}

#include <string>

class JS
{
public:
    JS();

    static JS* instance();

    void initialize();
    void shutdown();
    int run_script(std::string script, std::string path);
    int run_file(std::string path);

    void loop();

    JSRuntime* rt = 0;
    JSContext* ctx = 0;
};

#endif // TE_JS_H