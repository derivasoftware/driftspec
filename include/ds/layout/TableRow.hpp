#ifndef DRIFTSPEC_LAYOUT_TABLEROW_HPP
#define DRIFTSPEC_LAYOUT_TABLEROW_HPP

#include <vector>

#include "ds/layout/TableText.hpp"

namespace ds {
namespace layout {

/// A row of a table to place (DS-464): its label, and its cells column by column.
struct TableRow {
  TableText label;
  std::vector<TableText> cells;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_TABLEROW_HPP
