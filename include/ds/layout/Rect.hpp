#ifndef DRIFTSPEC_LAYOUT_RECT_HPP
#define DRIFTSPEC_LAYOUT_RECT_HPP

namespace ds {
namespace layout {

/// A rectangle of the layered engine, in whole units (DS-392): its top left corner, its width and its height.
struct Rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_RECT_HPP
