#include <gtest/gtest.h>

#include "ds/layout/TableRow.hpp"

namespace {

/// A row of a table to place (DS-464): its label and its cells.
TEST(TestTableRow, ARowHasItsLabelAndItsCells) {
  const ds::layout::TableRow r{{{3}}, {{{4}}, {{1}}}};
  EXPECT_EQ(r.label.words[0], 3u);
  ASSERT_EQ(r.cells.size(), 2u);
  EXPECT_EQ(r.cells[1].words[0], 1u);
}

}  // namespace
