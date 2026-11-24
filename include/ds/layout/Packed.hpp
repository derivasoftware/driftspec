#ifndef DRIFTSPEC_LAYOUT_PACKED_HPP
#define DRIFTSPEC_LAYOUT_PACKED_HPP

#include <utility>
#include <vector>

namespace ds {
namespace layout {

/// A frame packed (DS-389): its size and where each of its children sits, from its top left corner, its ports after
/// its kids. What the packer works out before it says where everything lands.
struct Packed {
  double w = 0;
  double h = 0;
  std::vector<std::pair<double, double>> at;
  std::vector<Packed> kids;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_PACKED_HPP
