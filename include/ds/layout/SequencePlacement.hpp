#ifndef DRIFTSPEC_LAYOUT_SEQUENCEPLACEMENT_HPP
#define DRIFTSPEC_LAYOUT_SEQUENCEPLACEMENT_HPP

#include <vector>

#include "ds/layout/BlockPlace.hpp"
#include "ds/layout/LanePlace.hpp"
#include "ds/layout/RowPlace.hpp"

namespace ds {
namespace layout {

/// Where a sequence lands (DS-464), in the sink's units: a place per lane, a row per row number up to the last
/// message's, a place per frame, its size, and the proportions its marks keep.
struct SequencePlacement {
  std::vector<LanePlace> lanes;
  std::vector<RowPlace> rows;
  std::vector<BlockPlace> blocks;
  int width = 0;
  int height = 0;
  int headTop = 0;     ///< where the heads stand, below the margin
  int headHeight = 0;  ///< a head's height
  int selfWidth = 0;   ///< how far a message to its own lane loops out
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_SEQUENCEPLACEMENT_HPP
