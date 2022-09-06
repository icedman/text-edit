#include "highlight.h"
#include "textmate.h"

static Highlight *hl_instance;

Highlight::Highlight() { hl_instance = this; }

Highlight *Highlight::instance() { return hl_instance; }

void Highlight::initialize() {
  Textmate::initialize("/home/iceman/.editor/extensions/");
  Textmate::initialize("/home/iceman/.vscode/extensions/");
}

void Highlight::shutdown() { Textmate::shutdown(); }

void Highlight::load_theme(std::string theme) { Textmate::load_theme(theme); }

bool Highlight::has_running_threads() {
  return Textmate::has_running_threads();
}