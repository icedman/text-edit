#ifndef TE_VIEW_H
#define TE_VIEW_H

#include <memory>
#include <vector>

struct point_t {
  int x;
  int y;
};

struct rect_t {
  int x;
  int y;
  int w;
  int h;
};

bool rects_overlap(rect_t a, rect_t b);
bool rects_equal(rect_t a, rect_t b);
bool point_in_rect(point_t p, rect_t r);
rect_t intersect_rects(rect_t a, rect_t b);
rect_t merge_rects(rect_t a, rect_t b);

struct view_t;
typedef std::shared_ptr<view_t> view_ptr;
typedef std::vector<view_ptr> view_list;

struct view_t {
  view_t()
      : frame({0, 0, 0, 0}), constraint({0, 0, 0, 0}), computed({0, 0, 0, 0}),
        flex(0), direction(0), show(true) {}

  rect_t frame;      // preferred
  rect_t constraint; // set by parent
  rect_t computed;   // screen

  int flex;
  int direction;
  bool show;

  view_list children;

  void layout(rect_t constraint);
  void render();

  view_list flexibles;
  view_list rigids;

  virtual int *_w(rect_t *rect);
  virtual int *_x(rect_t *rect);
  virtual int *_h(rect_t *rect);
  virtual int *_y(rect_t *rect);
};

struct column_t : view_t {
  int *_w(rect_t *rect);
  int *_x(rect_t *rect);
  int *_h(rect_t *rect);
  int *_y(rect_t *rect);
};

struct row_t : view_t {};

#endif // TE_VIEW_H