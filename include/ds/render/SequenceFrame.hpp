#ifndef DRIFTSPEC_RENDER_SEQUENCEFRAME_HPP
#define DRIFTSPEC_RENDER_SEQUENCEFRAME_HPP

#include <vector>

#include "ds/render/Ir.hpp"
#include "ds/render/SequenceDivider.hpp"

namespace ds {
namespace render {

/// Where a frame of a sequence lands (DS-400): the messages it spans, its four sides and its elses.
struct SequenceFrame {
  const IrNode* node = nullptr;  ///< the frame, in the figure's document
  int first = 0;
  int last = 0;
  int top = 0;
  int bottom = 0;
  int left = 0;
  int right = 0;
  std::vector<SequenceDivider> dividers;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SEQUENCEFRAME_HPP
