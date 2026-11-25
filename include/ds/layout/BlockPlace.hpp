#ifndef DRIFTSPEC_LAYOUT_BLOCKPLACE_HPP
#define DRIFTSPEC_LAYOUT_BLOCKPLACE_HPP

#include <vector>

#include "ds/layout/DividerPlace.hpp"

namespace ds {
namespace layout {

/// Where a frame lands (DS-464): its four sides, and its elses that fall within the rows drawn.
struct BlockPlace {
  int top = 0;
  int bottom = 0;
  int left = 0;
  int right = 0;
  std::vector<DividerPlace> dividers;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_BLOCKPLACE_HPP
