#ifndef DRIFTSPEC_LAYOUT_LAYEREDENGINE_HPP
#define DRIFTSPEC_LAYOUT_LAYEREDENGINE_HPP

#include <memory>

#include "ds/layout/IEngine.hpp"
#include "ds/layout/IRouter.hpp"
#include "ds/layout/Spacing.hpp"

namespace ds {
namespace layout {

/// The engine that layers boxes along their edges (Sugiyama), render's (DS-185, DS-392): in every layered frame,
/// bases above what inherits and wholes above parts, components packed towards a readable aspect, the edges routed
/// by the router it is handed. A frame wider than the graph's width wraps its layers onto further lines. It computes in
/// whole units: what it is given is whole and what it gives is whole, so the doubles of a graph and a placement carry
/// them exactly. Its phases are the layerer, the orderer, the positioner and the framer (DS-370).
class LayeredEngine : public IEngine {
 public:
  LayeredEngine(const Spacing& spacing, std::unique_ptr<IRouter> router);
  Placement place(const Graph& graph) const override;

 private:
  Spacing spacing_;
  std::unique_ptr<IRouter> router_;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_LAYEREDENGINE_HPP
