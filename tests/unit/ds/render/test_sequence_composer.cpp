#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/SequenceComposer.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
const ds::theme::Theme kTheme;

ds::render::IrNode lifeline(const std::string& id, const std::string& label) {
  ds::render::IrNode n;
  n.id = id;
  n.kind = "lifeline";
  n.label = label;
  n.classifier = "participant";
  return n;
}

ds::render::IrEdge message(const std::string& from, const std::string& to, const std::string& label, int order) {
  ds::render::IrEdge e;
  e.from = from;
  e.to = to;
  e.kind = "message";
  e.label = label;
  e.order = order;
  return e;
}

/// Two lifelines, a call from the first to the second and a call of the second to itself, inside an alt frame with an
/// else before the second message.
ds::render::IrDocument twoLanes() {
  ds::render::IrDocument doc;
  doc.title = "SEQ_Two";
  ds::render::IrNode frame;
  frame.id = "f0";
  frame.kind = "frame";
  frame.classifier = "alt";
  frame.label = "alt ready";
  frame.span = {0, 1};
  frame.dividers = {{1, "else"}};
  doc.nodes = {lifeline("a", "Caller"), lifeline("b", "Callee"), frame};
  doc.edges = {message("a", "b", "run()", 0), message("b", "b", "check()", 1)};
  return doc;
}

/// A sequence composed for SVG (DS-400, DS-464): its lanes, rows and frames in pixels, pointing into the document.
TEST(TestSequenceComposer, InSvgTheLanesRowsAndFramesAreComposedInPixels) {
  const ds::render::Metrics m = ds::render::Metrics::svg(kTheme.measures(ds::theme::Sink::svg));
  const ds::render::Figure figure = ds::render::SequenceComposer(m).compose(twoLanes(), 0);
  ASSERT_TRUE(figure.sequence.has_value());
  const ds::render::SequenceLayout& s = *figure.sequence;
  ASSERT_EQ(s.lanes.size(), 2u);
  EXPECT_EQ(s.lanes[0].node, &figure.document().nodes[0]);  // it points into the document composed from
  EXPECT_LT(s.lanes[0].cx, s.lanes[1].cx);
  EXPECT_EQ(s.laneOf.at("b"), 1u);
  ASSERT_EQ(s.messages.size(), 2u);
  EXPECT_EQ(s.messages[1]->label, "check()");
  ASSERT_EQ(s.rows.size(), 2u);
  EXPECT_LT(s.rows[0].line, s.rows[1].line);  // the second message runs below the first
  ASSERT_EQ(s.frames.size(), 1u);
  EXPECT_EQ(s.frames[0].node->id, "f0");
  EXPECT_LT(s.frames[0].left, s.lanes[0].cx);  // the frame wraps both lanes
  EXPECT_GT(s.frames[0].right, s.lanes[1].cx);
  ASSERT_EQ(s.frames[0].dividers.size(), 1u);
  EXPECT_EQ(s.frames[0].dividers[0].label, "else");
  EXPECT_GT(s.width, s.lanes[1].cx);
  EXPECT_GT(s.height, s.rows[1].bottom);
  EXPECT_GT(s.headHeight, 0);  // the proportions the drawing's marks keep
  EXPECT_GT(s.selfWidth, 0);
}

/// A sequence with no lifeline is composed empty, and the drawing says what it has.
TEST(TestSequenceComposer, NoLifelineComposesNoLane) {
  const ds::render::Metrics m = ds::render::Metrics::svg(kTheme.measures(ds::theme::Sink::svg));
  const ds::render::Figure figure = ds::render::SequenceComposer(m).compose(ds::render::IrDocument(), 0);
  ASSERT_TRUE(figure.sequence.has_value());
  EXPECT_TRUE(figure.sequence->lanes.empty());
}

/// A sequence composed for a terminal (DS-464): the svg's proportions in cells, a head and a message to its own lane
/// given the three rows a terminal draws them in.
TEST(TestSequenceComposer, InATerminalTheSameProportionsAreCellsAndTheMarksGetTheirLeastRoom) {
  const ds::render::Metrics m = ds::render::Metrics::text(kTheme.measures(ds::theme::Sink::text));
  const ds::render::Figure figure = ds::render::SequenceComposer(m).compose(twoLanes(), 0);
  ASSERT_TRUE(figure.sequence.has_value());
  const ds::render::SequenceLayout& s = *figure.sequence;
  ASSERT_EQ(s.lanes.size(), 2u);
  EXPECT_EQ(s.lanes[0].node, &figure.document().nodes[0]);  // it points into the document composed from
  EXPECT_LT(s.lanes[0].cx, s.lanes[1].cx);
  EXPECT_EQ(s.laneOf.at("b"), 1u);
  ASSERT_EQ(s.messages.size(), 2u);
  EXPECT_EQ(s.messages[1]->label, "check()");
  ASSERT_EQ(s.rows.size(), 2u);
  EXPECT_LT(s.rows[0].line, s.rows[1].line);  // the second message runs below the first
  ASSERT_EQ(s.frames.size(), 1u);
  EXPECT_EQ(s.frames[0].node->id, "f0");
  EXPECT_LT(s.frames[0].left, s.lanes[0].cx);  // the frame wraps both lanes
  EXPECT_GT(s.frames[0].right, s.lanes[1].cx);
  ASSERT_EQ(s.frames[0].dividers.size(), 1u);
  EXPECT_EQ(s.frames[0].dividers[0].label, "else");
  EXPECT_GT(s.width, s.lanes[1].cx);
  EXPECT_GT(s.height, s.rows[1].bottom);
  EXPECT_GE(s.rows[0].line - s.rows[0].top, 1);                                     // a call: its label above its arrow
  EXPECT_GE(s.rows[1].bottom - s.rows[1].line, 2);                                  // a call to its own lane loops back on the row below
  EXPECT_EQ(s.lanes[0].headW, static_cast<int>(std::string("Caller").size()) + 4);  // its name, a space and a border each side
  EXPECT_EQ(s.headHeight, 3);                                                       // a head's border, its name, its border
}

}  // namespace
