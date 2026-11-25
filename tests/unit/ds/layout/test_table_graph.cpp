#include <gtest/gtest.h>

#include "ds/layout/TableGraph.hpp"

namespace {

/// A table to place (DS-464): empty until its corner, headings and rows are given.
TEST(TestTableGraph, AGraphHoldsItsCornerHeadingsAndRows) {
  ds::layout::TableGraph g;
  EXPECT_TRUE(g.columns.empty());
  g.columns = {{{4}}, {{6}}};
  g.rows = {{{{3}}, {{{4}}, {{1}}}}};
  EXPECT_EQ(g.rows.size(), 1u);
  EXPECT_EQ(g.columns[1].words[0], 6u);
}

}  // namespace
