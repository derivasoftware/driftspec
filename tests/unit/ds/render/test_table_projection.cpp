#include <gtest/gtest.h>

#include "ds/render/Ir.hpp"
#include "ds/render/TableProjection.hpp"

namespace {

ds::render::IrDocument order() {
  ds::render::IrDocument doc;
  doc.title = "TB_Order";
  ds::render::IrTable t;
  t.corner = "Order";
  t.columns = {"Open", "Paid"};
  ds::render::IrRow pay;
  pay.label = "pay";
  pay.cells = {{"Paid", "jump", ""}, {"", "none", ""}};
  ds::render::IrRow cancel;
  cancel.label = "cancel";
  cancel.cells = {{"Cancelled", "jump", ""}, {"Desk::rush", "against", ""}};
  t.rows = {pay, cancel};
  doc.table = t;
  return doc;
}

/// A table as the layout's graph (DS-464): each text's words measured in characters, as the sink writes them: a
/// terminal's marks counted, an svg's cells as written.
TEST(TestTableProjection, EachTextIsMeasuredAsTheSinkWritesIt) {
  const ds::render::IrDocument doc = order();
  const ds::render::TableProjection svg(doc, false);
  ASSERT_EQ(svg.graph().rows.size(), 2u);
  EXPECT_TRUE(svg.graph().rows[0].cells[1].words.empty());  // an empty cell
  EXPECT_EQ(svg.graph().rows[1].cells[1].words.size(), 1u);
  const ds::render::TableProjection text(doc, true);
  ASSERT_EQ(text.graph().rows[0].cells[1].words.size(), 1u);  // "·"
  EXPECT_EQ(text.graph().rows[0].cells[1].words[0], 1u);
  ASSERT_EQ(text.graph().rows[1].cells[1].words.size(), 2u);  // "!" and "Desk::rush"
  // the corner, two headings, then each row's label and its cells
  ASSERT_EQ(text.words().size(), 3u + 2u * 3u);
  EXPECT_EQ(text.words()[8][0], "!");
}

}  // namespace
