#ifndef DRIFTSPEC_RENDER_FIGURE_HPP
#define DRIFTSPEC_RENDER_FIGURE_HPP

#include <optional>

#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/SequenceLayout.hpp"

namespace ds {
namespace render {

/// A diagram composed (DS-400): its document as it is drawn and where each of its nodes and routes lands, or its lanes,
/// rows and frames when it is a sequence, measured
/// with the metrics of the technology that draws it. A drawing draws a figure and places nothing. Where a node lands
/// points into the figure's own document, so a figure is moved and never copied.
struct Figure {
  IrDocument document;                     ///< the document as it is drawn: what a closed box holds bundled into it
  LayoutResult layout;                     ///< where each node of the document lands, and each route
  std::optional<SequenceLayout> sequence;  ///< where a sequence's lanes, rows and frames land, when the figure is one

  Figure() = default;
  Figure(const Figure&) = delete;
  Figure(Figure&&) = default;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_FIGURE_HPP
