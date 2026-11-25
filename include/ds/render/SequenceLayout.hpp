#ifndef DRIFTSPEC_RENDER_SEQUENCELAYOUT_HPP
#define DRIFTSPEC_RENDER_SEQUENCELAYOUT_HPP

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "ds/render/Ir.hpp"
#include "ds/render/Lane.hpp"
#include "ds/render/SequenceFrame.hpp"
#include "ds/render/SequenceRow.hpp"

namespace ds {
namespace render {

/// Where a sequence lands (DS-400), composed for one technology: its lanes, the messages in order and the row each
/// lands on, its frames, its size, and the proportions its marks are drawn with. Everything points into the figure's
/// document.
struct SequenceLayout {
  std::vector<Lane> lanes;               ///< a lane per lifeline, in the order written
  std::map<std::string, size_t> laneOf;  ///< a lifeline's lane, by its id
  std::vector<const IrEdge*> messages;   ///< by their order
  std::vector<SequenceRow> rows;         ///< by a message's order
  std::vector<SequenceFrame> frames;     ///< the frames drawn
  int width = 0;
  int height = 0;
  int margin = 0;      ///< around the drawing, above the heads
  int headHeight = 0;  ///< a lifeline's head
  int selfWidth = 0;   ///< how far a message to its own lane loops out
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SEQUENCELAYOUT_HPP
