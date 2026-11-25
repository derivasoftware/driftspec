#ifndef DRIFTSPEC_LAYOUT_PROJECTED_HPP
#define DRIFTSPEC_LAYOUT_PROJECTED_HPP

#include <cstddef>

namespace ds {
namespace layout {

/// An edge projected onto the kids of a frame (DS-392): which kid is drawn above which.
struct Projected {
  int upper = 0;
  int lower = 0;
  std::size_t edge = 0;  ///< index in the document, or SIZE_MAX for a pseudo edge
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_PROJECTED_HPP
