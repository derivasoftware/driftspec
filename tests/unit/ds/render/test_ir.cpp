#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/render/Ir.hpp"

namespace {

/// The badge a node carries is the same letter in the SVG and in the text drawing (DS-118).
TEST(TestBadgeOf, EachClassifierHasOneLetterInEveryDrawing) {
  EXPECT_EQ(ds::render::badgeOf("function", false), "f");
  EXPECT_EQ(ds::render::badgeOf("interface", true), "I");
  EXPECT_EQ(ds::render::badgeOf("constraint", false), "K");
  EXPECT_EQ(ds::render::badgeOf("", true), "A");  // an abstract class names no classifier
  EXPECT_EQ(ds::render::badgeOf("", false), "C");
  EXPECT_EQ(ds::render::badgeOf("usecase", false), "U");  // one the table does not name: its first letter
}

TEST(TestHoldsMembers, ASectionHoldsMembersOrWhatAClosedBoxOffers) {
  EXPECT_TRUE(ds::render::holdsMembers("attributes"));
  EXPECT_TRUE(ds::render::holdsMembers("methods"));
  for (const char* offered : {"package", "namespace", "interface", "class", "dataclass", "enum", "function"})
    EXPECT_FALSE(ds::render::holdsMembers(offered)) << offered;  // a compartment of a closed box: names, all of that kind
}

/// A drawing that is a table (DS-251): read from the JSON of the IR as it is written.
TEST(TestIrTable, ADocumentWithATableIsReadWithItsCornerItsColumnsAndTheCellsOfEachRow) {
  const auto doc = ds::render::IrDocument::parse(
      R"({"ir":1,"title":"MX","nodes":[],"edges":[],"table":{"corner":"Order","columns":["Open","Paid"],
          "rows":[{"label":"pay","title":"pay is called in Desk::sell",
                   "cells":[{"text":"Paid","kind":"jump","title":"pay takes Open to Paid"},{"text":"","kind":"none","title":""}]}]}})");
  EXPECT_EQ(doc.table.corner, "Order");
  EXPECT_EQ(doc.table.columns, (std::vector<std::string>{"Open", "Paid"}));
  ASSERT_EQ(doc.table.rows.size(), 1u);
  EXPECT_EQ(doc.table.rows[0].label, "pay");
  EXPECT_EQ(doc.table.rows[0].title, "pay is called in Desk::sell");
  ASSERT_EQ(doc.table.rows[0].cells.size(), 2u);
  EXPECT_EQ(doc.table.rows[0].cells[0].text, "Paid");
  EXPECT_EQ(doc.table.rows[0].cells[0].kind, "jump");
  EXPECT_EQ(doc.table.rows[0].cells[0].title, "pay takes Open to Paid");
  EXPECT_EQ(doc.table.rows[0].cells[1].kind, "none");
}

TEST(TestIrTable, ADocumentWithNoTableHasNoColumns) {
  EXPECT_TRUE(ds::render::IrDocument::parse(R"({"ir":1,"title":"CL","nodes":[],"edges":[]})").table.columns.empty());
}

}  // namespace
