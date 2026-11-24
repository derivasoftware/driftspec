#ifndef DRIFTSPEC_LAYOUT_BOX_HPP
#define DRIFTSPEC_LAYOUT_BOX_HPP

namespace ds {
namespace layout {

/// A rectangle where a box lands (DS-392): its top left corner, its width and its height. The scene reads one unit as
/// a pixel at zoom 1.
struct Box {
  double x = 0;
  double y = 0;
  double w = 0;
  double h = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_BOX_HPP
