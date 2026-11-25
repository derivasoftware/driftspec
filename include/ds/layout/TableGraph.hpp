#ifndef DRIFTSPEC_LAYOUT_TABLEGRAPH_HPP
#define DRIFTSPEC_LAYOUT_TABLEGRAPH_HPP

#include <vector>

#include "ds/layout/TableRow.hpp"
#include "ds/layout/TableText.hpp"

namespace ds {
namespace layout {

/// A table to place (DS-464), knowing nothing of what it stands for: its corner, its column headings and its rows.
struct TableGraph {
  TableText corner;
  std::vector<TableText> columns;
  std::vector<TableRow> rows;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_TABLEGRAPH_HPP
