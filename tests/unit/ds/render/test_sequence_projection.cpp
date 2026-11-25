#include <gtest/gtest.h>

#include <string>

#include "ds/render/Ir.hpp"
#include "ds/render/SequenceProjection.hpp"

namespace {

ds::render::IrNode node(const std::string& id, const std::string& kind, const std::string& label, const std::string& classifier) {
  ds::render::IrNode n;
  n.id = id;
  n.kind = kind;
  n.label = label;
  n.classifier = classifier;
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

/// A sequence as the layout's graph (DS-464): its lifelines as lanes, its messages in the order of their rows, its
/// frames as blocks, and what each stands for kept beside it in the same order.
TEST(TestSequenceProjection, TheLifelinesMessagesAndFramesBecomeTheGraphInOrder) {
  ds::render::IrDocument doc;
  ds::render::IrNode alt = node("f0", "frame", "alt ready", "alt");
  alt.span = {0, 1};
  alt.dividers = {{1, "else"}};
  doc.nodes = {node("a", "lifeline", "User", "actor"), alt, node("b", "lifeline", "Shop", "participant")};
  doc.edges = {message("b", "b", "check()", 1), message("a", "b", "buy()", 0), message("a", "nobody", "lost()", 2)};
  const ds::render::SequenceProjection p(doc);
  const ds::layout::SequenceGraph& g = p.graph();
  ASSERT_EQ(g.lanes.size(), 2u);
  EXPECT_TRUE(g.lanes[0].actor);
  EXPECT_EQ(g.lanes[1].labelLength, 4u);
  EXPECT_EQ(p.lanes()[1]->id, "b");
  ASSERT_EQ(g.messages.size(), 2u);  // a message to no lifeline is not drawn
  EXPECT_EQ(g.messages[0].row, 0);   // by row
  EXPECT_EQ(g.messages[0].from, 0u);
  EXPECT_EQ(g.messages[1].from, g.messages[1].to);
  EXPECT_EQ(p.messages()[1]->label, "check()");
  ASSERT_EQ(g.blocks.size(), 1u);
  EXPECT_EQ(g.blocks[0].classifierLength, 3u);
  ASSERT_EQ(g.blocks[0].dividers.size(), 1u);
  EXPECT_EQ(g.blocks[0].dividers[0], 1);
  EXPECT_EQ(p.blocks()[0]->id, "f0");
}

}  // namespace
