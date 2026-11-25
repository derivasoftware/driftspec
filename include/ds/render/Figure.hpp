#ifndef DRIFTSPEC_RENDER_FIGURE_HPP
#define DRIFTSPEC_RENDER_FIGURE_HPP

#include <optional>

#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/SequenceLayout.hpp"
#include "ds/render/TableLayout.hpp"

namespace ds {
namespace render {

/// A diagram composed (DS-400): the document it draws and where each of its nodes and routes lands, or its lanes, rows
/// and frames when it is a sequence, measured with the metrics of the technology that draws it. A drawing draws a
/// figure and places nothing. When composing makes a document of its own (a box diagram bundles what a closed box
/// holds) the figure owns it; otherwise it refers to the document it was composed from, which outlives it, as a
/// description refers to its document. Where a node lands points into that document, so a figure is moved and never
/// copied.
struct Figure {
  IrDocument owned;                        ///< the document composing made, when it made one
  const IrDocument* source = nullptr;      ///< the document composed from, when it is drawn as it is
  LayoutResult layout;                     ///< where each node of the document lands, and each route
  std::optional<SequenceLayout> sequence;  ///< where a sequence's lanes, rows and frames land, when the figure is one
  std::optional<TableLayout> table;        ///< where a table's columns, rows and lines land, when the figure is one

  Figure() = default;
  Figure(const Figure&) = delete;
  Figure(Figure&&) = default;

  /// The document the figure draws: the one it was composed from, or the one composing made.
  const IrDocument& document() const { return source ? *source : owned; }
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_FIGURE_HPP
