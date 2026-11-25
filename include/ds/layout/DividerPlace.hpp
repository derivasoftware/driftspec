#ifndef DRIFTSPEC_LAYOUT_DIVIDERPLACE_HPP
#define DRIFTSPEC_LAYOUT_DIVIDERPLACE_HPP

#include <cstddef>

namespace ds {
namespace layout {

/// Where an else of a frame lands (DS-464): which of the frame's elses it is, and the y of its line.
struct DividerPlace {
  std::size_t index = 0;
  int y = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_DIVIDERPLACE_HPP
