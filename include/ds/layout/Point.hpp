#ifndef DRIFTSPEC_LAYOUT_POINT_HPP
#define DRIFTSPEC_LAYOUT_POINT_HPP

namespace ds {
namespace layout {

/// A point of the layered engine, in whole units (DS-392).
struct Point {
  int x = 0;
  int y = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_POINT_HPP
