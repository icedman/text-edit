#include "js.h"
#include "util.h"

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

void delay(int ms);

static JS* js_instance;

static JSValue js_log(JSContext* ctx, JSValueConst this_val,
    int argc, JSValueConst* argv)
{
    std::stringstream ss;
    
    // ss << "log: ";
    int i;
    const char* str;

    for (i = 0; i < argc; i++) {
        str = JS_ToCString(ctx, argv[i]);
        if (!str)
            return JS_EXCEPTION;
        ss << str << "\n";
        JS_FreeCString(ctx, str);
    }

    log("%s", ss.str().c_str());

    return JS_UNDEFINED;
}

/* also used to initialize the worker context */
static JSContext *JS_NewCustomContext(JSRuntime *rt)
{
    JSContext *ctx;
    ctx = JS_NewContext(rt);
    if (!ctx)
        return NULL;
    /* system modules */
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");
    return ctx;
}

JS::JS()
{
    js_instance = this;
}

JS* JS::instance()
{
    return js_instance;
}

void JS::initialize()
{
    rt = JS_NewRuntime();

    js_std_set_worker_new_context_func(JS_NewCustomContext);
    js_std_init_handlers(rt);
    ctx = JS_NewCustomContext(rt);
    if (!ctx) {
        log("qjs: cannot allocate JS context");
        // handle this
    }

    /* loader for ES6 modules */
    JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);

    JSValue global_obj, app;

    global_obj = JS_GetGlobalObject(ctx);
    app = JS_NewObject(ctx);
    // JS_SetPropertyStr(ctx, global_obj, "log", JS_NewCFunction(ctx, js_log, "log", 1));
    JS_SetPropertyStr(ctx, app, "log", JS_NewCFunction(ctx, js_log, "log", 1));
    JS_SetPropertyStr(ctx, global_obj, "app", app);
    JS_FreeValue(ctx, global_obj);

    JSValue ret;
    std::string init_script = "import * as std from 'std';";
                init_script += "import * as os from 'os';";
                init_script += "app.executablePath = os.realpath('./');";
                init_script += "globalThis.os = os;";
                init_script += "globalThis.std = std;";
                init_script += "globalThis.exports = {}";

    ret = JS_Eval(ctx, init_script.c_str(), init_script.length(), "<input>", JS_EVAL_TYPE_MODULE);
    if (JS_IsException(ret)) {
        js_std_dump_error(ctx);
        JS_ResetUncatchableError(ctx);
    }

    run_file("/home/iceman/Developer/Projects/text_edit_superstring/js/require.js");
    run_file("/home/iceman/Developer/Projects/text_edit_superstring/js/polyfill.js");
    run_file("/home/iceman/Developer/Projects/text_edit_superstring/tests/index.js");
}

void JS::shutdown()
{   
    js_std_free_handlers(rt);

    JS_RunGC(rt);
    free(ctx);
    free(rt);
}

int JS::run_script(std::string script, std::string path)
{
    JSValue ret;
    ret = JS_Eval(ctx, script.c_str(), script.length(), path.c_str(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(ret)) {
        js_std_dump_error(ctx);
        JS_ResetUncatchableError(ctx);
        log(">error running script");
        return 1;
    }
    return 0;
}

int JS::run_file(std::string path)
{
    std::ifstream file(path, std::ifstream::in);

    std::ifstream t(path);
    std::stringstream buffer;
    buffer << t.rdbuf();

    return run_script(buffer.str(), path);
}

void JS::loop()
{
    js_std_loop(ctx);
}