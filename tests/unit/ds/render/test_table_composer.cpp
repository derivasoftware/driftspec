#include <gtest/gtest.h>

#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/TableComposer.hpp"
#include "ds/theme/Theme.hpp"

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

// built on first use, not when the executable starts: a test reaches the theme only if it uses it (DS-473)
const ds::theme::Theme& theme() {
  static const ds::theme::Theme built;
  return built;
}

/// A table composed for a sink (DS-464): its columns, rows and lines placed, the svg's in pixels, a terminal's in cells
/// with its marks written.
TEST(TestTableComposer, TheColumnsRowsAndLinesAreComposedForTheSink) {
  const ds::render::IrDocument doc = order();
  const ds::render::Figure svg =
      ds::render::TableComposer(ds::render::Metrics::svg(theme().measures(ds::theme::Sink::svg))).compose(doc, 0);
  ASSERT_TRUE(svg.table.has_value());
  EXPECT_EQ(svg.table->columnWidth.size(), 3u);
  EXPECT_EQ(svg.table->headHeight, 38);
  ASSERT_EQ(svg.table->cells.size(), 2u);
  EXPECT_EQ(svg.table->cells[1][1], (std::vector<std::string>{"Desk::rush"}));
  EXPECT_EQ(&svg.document(), &doc);
  const ds::render::Figure text =
      ds::render::TableComposer(ds::render::Metrics::text(theme().measures(ds::theme::Sink::text))).compose(doc, 0);
  EXPECT_EQ(text.table->cells[1][1], (std::vector<std::string>{"! Desk::rush"}));
  EXPECT_EQ(text.table->cells[0][1], (std::vector<std::string>{"\xC2\xB7"}));
  EXPECT_EQ(text.table->padX, 2);
}

}  // namespace
