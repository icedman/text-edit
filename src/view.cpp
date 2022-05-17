#include "view.h"

bool rects_overlap(rect_t a, rect_t b) {
  if (a.x >= b.x + b.w || b.x >= a.x + a.w | a.y >= b.y + b.h ||
      b.y >= a.y + a.h) {
    return false;
  }

  return true;
}

bool point_in_rect(point_t p, rect_t r) {
  return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

static inline int min(int a, int b) { return a < b ? a : b; }
static inline int max(int a, int b) { return a > b ? a : b; }

rect_t intersect_rects(rect_t a, rect_t b) {
  int x1 = max(a.x, b.x);
  int y1 = max(a.y, b.y);
  int x2 = min(a.x + a.w, b.x + b.w);
  int y2 = min(a.y + a.h, b.y + b.h);
  return (rect_t){x1, y1, max(0, x2 - x1), max(0, y2 - y1)};
}

rect_t merge_rects(rect_t a, rect_t b) {
  int x1 = min(a.x, b.x);
  int y1 = min(a.y, b.y);
  int x2 = max(a.x + a.w, b.x + b.w);
  int y2 = max(a.y + a.h, b.y + b.h);
  return (rect_t){x1, y1, x2 - x1, y2 - y1};
}

bool rects_equal(rect_t a, rect_t b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

view_ptr view_t::input_focus;

void build_focusable_list(view_list &list, view_ptr node) {
  if (!node->show)
    return;
  if (node->focusable) {
    list.push_back(node);
  }
  for (auto c : node->children) {
    build_focusable_list(list, c);
  }
}

view_ptr view_t::find_next_focus(view_ptr root, view_ptr node, int x, int y) {
  view_list focusables;
  build_focusable_list(focusables, root);

  for (auto v : focusables) {
    point_t n = {node->computed.x + node->computed.w / 2,
                 node->computed.y + node->computed.h / 2};
    int l = x != 0 ? root->computed.w : root->computed.h;
    if (x > 0) {
      n.x += node->computed.w;
    }
    if (y > 0) {
      n.y += node->computed.h;
    }
    for (int i = 0; i < l; i++) {
      n.x += x;
      n.y += y;
      if (point_in_rect(n, v->computed)) {
        return v;
      }
    }
  }

  return nullptr;
}

void view_t::layout(rect_t c) {
  constraint = c;

  flexibles.clear();
  rigids.clear();

  int reserved = 0;
  int total_flex = 0;
  for (auto c : children) {
    if (!c->show)
      continue;
    *_h(&c->constraint) = *_h(&constraint);
    if (c->flex > 0) {
      flexibles.push_back(c);
      total_flex += c->flex;
    } else {
      rigids.push_back(c);
      reserved += *_w(&c->frame);
      *_w(&c->constraint) = *_w(&c->frame);
    }
  }

  if (total_flex == 0) {
    total_flex = 1;
  }

  int total = (*_w(&constraint) - reserved);
  int distributed = 0;
  int *last = NULL;
  for (auto c : flexibles) {
    *_w(&c->constraint) = (total * c->flex) / total_flex;
    distributed += *_w(&c->constraint);
    last = _w(&c->constraint);
  }
  if (last) {
    *last = *last + (total - distributed);
  }

  for (auto c : children) {
    if (!c->show)
      continue;
    c->layout(c->constraint);
  }
}

void view_t::finalize() {
  *_x(&computed) = *_x(&constraint);
  *_y(&computed) = *_y(&constraint);
  *_w(&computed) = *_w(&constraint);
  *_h(&computed) = *_h(&constraint);

  // printf("%d %d\n", constraint.w, constraint.h);

  int x = *_x(&computed);
  for (auto c : children) {
    if (!c->show)
      continue;
    *_x(&c->constraint) = x;
    *_y(&c->constraint) = *_y(&computed);
    c->finalize();
    x += *_w(&c->constraint);
  }
}

int *view_t::_w(rect_t *rect) { return &rect->w; }
int *view_t::_x(rect_t *rect) { return &rect->x; }
int *view_t::_h(rect_t *rect) { return &rect->h; }
int *view_t::_y(rect_t *rect) { return &rect->y; }
int *column_t::_w(rect_t *rect) { return &rect->h; }
int *column_t::_h(rect_t *rect) { return &rect->w; }
int *column_t::_x(rect_t *rect) { return &rect->y; }
int *column_t::_y(rect_t *rect) { return &rect->x; }
