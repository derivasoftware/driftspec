#ifndef DRIFTSPEC_RENDER_LANE_HPP
#define DRIFTSPEC_RENDER_LANE_HPP

#include "ds/render/Ir.hpp"

namespace ds {
namespace render {

/// A lifeline of a sequence drawn in SVG (DS-371): its node, the x of its centre and the width of its head.
struct Lane {
  const IrNode* node;
  int cx = 0;
  int headW = 0;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_LANE_HPP
