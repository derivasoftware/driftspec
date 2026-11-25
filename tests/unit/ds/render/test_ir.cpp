#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/render/Ir.hpp"

namespace {

/// A drawing that is a table (DS-251): read from the JSON of the IR as it is written.
/// An edge between two boxes of a kind, saying nothing else yet (DS-418): no label, no order, no dash, no references.
TEST(TestIrEdge, AnEdgeBetweenTwoBoxesSaysItsEndsAndItsKindAndNothingElse) {
  const ds::render::IrEdge e = ds::render::IrEdge::between("a", "b", "dependency");
  EXPECT_EQ(e.from, "a");
  EXPECT_EQ(e.to, "b");
  EXPECT_EQ(e.kind, "dependency");
  EXPECT_TRUE(e.label.empty());
  EXPECT_EQ(e.order, -1);
  EXPECT_FALSE(e.dashed);
  EXPECT_TRUE(e.refs.empty());
}

TEST(TestIrTable, ADocumentWithATableIsReadWithItsCornerItsColumnsAndTheCellsOfEachRow) {
  const auto doc = ds::render::IrDocument::parse(
      R"({"ir":1,"title":"MX","nodes":[],"edges":[],"table":{"corner":"Order","columns":["Open","Paid"],
          "rows":[{"label":"pay","title":"pay is called in Desk::sell",
                   "cells":[{"text":"Paid","kind":"jump","title":"pay takes Open to Paid"},{"text":"","kind":"none","title":""}]}]}})");
  ASSERT_TRUE(doc.table.has_value());
  EXPECT_EQ(doc.table->corner, "Order");
  EXPECT_EQ(doc.table->columns, (std::vector<std::string>{"Open", "Paid"}));
  ASSERT_EQ(doc.table->rows.size(), 1u);
  EXPECT_EQ(doc.table->rows[0].label, "pay");
  EXPECT_EQ(doc.table->rows[0].title, "pay is called in Desk::sell");
  ASSERT_EQ(doc.table->rows[0].cells.size(), 2u);
  EXPECT_EQ(doc.table->rows[0].cells[0].text, "Paid");
  EXPECT_EQ(doc.table->rows[0].cells[0].kind, "jump");
  EXPECT_EQ(doc.table->rows[0].cells[0].title, "pay takes Open to Paid");
  EXPECT_EQ(doc.table->rows[0].cells[1].kind, "none");
}

TEST(TestIrTable, ADocumentWithNoTableHasNoTable) {
  EXPECT_FALSE(ds::render::IrDocument::parse(R"({"ir":1,"title":"CL","nodes":[],"edges":[]})").table.has_value());
}

}  // namespace
