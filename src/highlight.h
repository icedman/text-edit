#ifndef TE_HIGHLIGHT_H
#define TE_HIGHLIGHT_H

#include "document.h"
#include <string>

class Highlight {
public:
    Highlight();

    static Highlight* instance();

    void initialize();
    void shutdown();

    void load_theme(std::string theme);
    bool has_running_threads();
};

#endif // TE_HIGHLIGHT_H