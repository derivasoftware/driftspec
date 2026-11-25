#ifndef DRIFTSPEC_LAYOUT_GRIDENGINE_HPP
#define DRIFTSPEC_LAYOUT_GRIDENGINE_HPP

#include <cstddef>
#include <vector>

#include "ds/layout/ITableEngine.hpp"
#include "ds/layout/TableGraph.hpp"
#include "ds/layout/TablePlacement.hpp"
#include "ds/layout/TableSpacing.hpp"

namespace ds {
namespace layout {

/// The grid engine (DS-464): each text broken into lines no longer than the wrap, between words; each column as wide
/// as its widest line and its room each side; each row as tall as its tallest cell, or its least.
class GridEngine : public ITableEngine {
 public:
  explicit GridEngine(const TableSpacing& spacing) : spacing_(spacing) {}
  TablePlacement place(const TableGraph& graph) const override;

 private:
  /// The word each line after the first starts with, the text broken no longer than the wrap.
  std::vector<std::size_t> breaks(const TableText& text) const;
  /// The characters of its widest line.
  std::size_t widest(const TableText& text, const std::vector<std::size_t>& breaks) const;

  const TableSpacing spacing_;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_GRIDENGINE_HPP
