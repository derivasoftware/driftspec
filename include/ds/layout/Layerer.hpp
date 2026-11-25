#ifndef DRIFTSPEC_LAYOUT_LAYERER_HPP
#define DRIFTSPEC_LAYOUT_LAYERER_HPP

#include <vector>

#include "ds/layout/Component.hpp"
#include "ds/layout/Layering.hpp"
#include "ds/layout/Rect.hpp"
#include "ds/layout/Spacing.hpp"

namespace ds {
namespace layout {

/// Assigns the layers of one component (DS-370, DS-392): a cycle is broken by reversing the edge that closes it in
/// a walk over the written order, every node goes as low as its longest path from the sources, and a long edge gets a
/// dummy in every layer it crosses, so it is ordered and placed like a node.
class Layerer {
 public:
  explicit Layerer(const Spacing& spacing);
  /// The component layered, its nodes sized as `rel` holds them.
  Layering of(const Component& c, const std::vector<Rect>& rel) const;

 private:
  Spacing spacing_;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_LAYERER_HPP
