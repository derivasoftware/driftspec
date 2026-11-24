#include <gtest/gtest.h>

#include <map>
#include <string>

#include "ds/layout/Box.hpp"
#include "ds/layout/Frame.hpp"
#include "ds/layout/Graph.hpp"
#include "ds/layout/PackingEngine.hpp"
#include "ds/layout/Placement.hpp"

namespace {

using ds::layout::Box;
using ds::layout::Frame;
using ds::layout::Graph;
using ds::layout::PackingEngine;
using ds::layout::Placement;

/// Where nested boxes go, knowing nothing of what they stand for (DS-389, DS-392): sized frames in, a box for each out.
class TestPackingEngine : public ::testing::Test {
 protected:
  static Frame leaf(const std::string& id, double w, double h) {
    Frame f;
    f.id = id;
    f.w = w;
    f.h = h;
    return f;
  }
  static Frame shelves(const std::string& id, double gap, double atLeast) {
    Frame f;
    f.id = id;
    f.arrangement = Frame::Arrangement::Shelves;
    f.gap = gap;
    f.atLeast = atLeast;
    return f;
  }
  static void expectBox(const Box& b, double x, double y, double w, double h) {
    EXPECT_DOUBLE_EQ(b.x, x);
    EXPECT_DOUBLE_EQ(b.y, y);
    EXPECT_DOUBLE_EQ(b.w, w);
    EXPECT_DOUBLE_EQ(b.h, h);
  }
  /// Packs `root`; with `following`, its shelves are held to the widths `kept` holds.
  static Placement pack(const Frame& root, const std::map<std::string, double>& kept = {}, bool following = false) {
    Graph graph;
    graph.root = &root;
    graph.widths = kept;
    graph.following = following;
    return PackingEngine().place(graph);
  }
};

TEST_F(TestPackingEngine, AFixedBoxKeepsItsSizeAndPutsEachChildWhereItIsTold) {
  Frame unit = leaf("u", 150, 50);
  unit.kids = {leaf("u::a", 150, 11), leaf("u::b", 150, 11)};
  unit.at = {{0, 22}, {0, 33}};
  const auto boxes = pack(unit).boxes;
  expectBox(boxes.at("u"), 0, 0, 150, 50);
  expectBox(boxes.at("u::a"), 0, 22, 150, 11);
  expectBox(boxes.at("u::b"), 0, 33, 150, 11);
}

TEST_F(TestPackingEngine, ShelvesFillLeftToRightNoWiderThanTheirTargetWithRoomAround) {
  Frame m = shelves("m", 10, 200);  // a target of 200: two boxes of 100 do not fit side by side with the gap
  m.left = 5, m.top = 20, m.right = 5, m.bottom = 7;
  m.kids = {leaf("a", 100, 40), leaf("b", 100, 30), leaf("c", 80, 10)};
  const Placement placed = pack(m);
  const auto& boxes = placed.boxes;
  expectBox(boxes.at("a"), 5, 20, 100, 40);
  expectBox(boxes.at("b"), 5, 70, 100, 30);      // a shelf down: below a's 40 and the gap
  expectBox(boxes.at("c"), 115, 70, 80, 10);     // beside b: 100 + 10 + 80 fits in 200
  expectBox(boxes.at("m"), 0, 0, 200, 107);      // 190 used + 5 + 5; 40 + 10 + 30 used + 20 + 7
  EXPECT_DOUBLE_EQ(placed.widths.at("m"), 190);  // the width its shelves came out at, kept
}

TEST_F(TestPackingEngine, PortsSitOnTheLeftBorderHalfOutsideOneUnderAnother) {
  Frame p = shelves("p", 10, 0);
  p.left = 300, p.top = 90, p.right = 60, p.bottom = 60;
  p.portTop = 130, p.portGap = 20;
  p.kids = {leaf("m", 400, 100)};
  p.ports = {leaf("i", 360, 80), leaf("j", 360, 50)};
  const auto boxes = pack(p).boxes;
  expectBox(boxes.at("m"), 300, 90, 400, 100);
  expectBox(boxes.at("i"), -180, 130, 360, 80);
  expectBox(boxes.at("j"), -180, 230, 360, 50);
  expectBox(boxes.at("p"), 0, 0, 760, 360);  // the ports reach lower than the shelves: 130 + 80 + 20 + 50 + 20 + 60
}

TEST_F(TestPackingEngine, ATreeCentresItsFirstBoxOverTheRowOfTheOthers) {
  Frame t;
  t.id = "t";
  t.arrangement = Frame::Arrangement::Tree;
  t.gap = 20;
  t.drop = 44;
  t.kids = {leaf("base", 100, 30), leaf("x", 150, 40), leaf("y", 150, 60)};
  const auto boxes = pack(t).boxes;
  expectBox(boxes.at("base"), 110, 0, 100, 30);  // centred over a row of 150 + 20 + 150
  expectBox(boxes.at("x"), 0, 74, 150, 40);
  expectBox(boxes.at("y"), 170, 74, 150, 60);
  expectBox(boxes.at("t"), 0, 0, 320, 134);
}

TEST_F(TestPackingEngine, ASecondPackingFollowingTheFirstFillsItsShelvesAsWideAsTheFirstCameOut) {
  Frame tall = shelves("m", 10, 0);
  tall.factor = 1.1;
  tall.kids = {leaf("a", 100, 100), leaf("b", 100, 100), leaf("c", 100, 100), leaf("d", 100, 100)};
  const Placement first = pack(tall);  // the target is the side of the area by 1.1, 220: two to a shelf
  ASSERT_DOUBLE_EQ(first.widths.at("m"), 210);
  Frame shorter = tall;  // the same boxes, shorter: on their own they aim at the widest, 100, one to a shelf
  for (Frame& k : shorter.kids) k.h = 10;
  EXPECT_DOUBLE_EQ(pack(shorter).boxes.at("m").w, 100);
  const Placement second = pack(shorter, first.widths, true);
  EXPECT_DOUBLE_EQ(second.boxes.at("m").w, 210);  // following the first: two to a shelf, as it came out
  expectBox(second.boxes.at("d"), 110, 20, 100, 10);
  EXPECT_TRUE(second.widths.empty());  // a placement that follows keeps no widths of its own
}

TEST_F(TestPackingEngine, AGraphWithoutBoxesPlacesNothingAndEdgesAreNotRouted) {
  EXPECT_TRUE(PackingEngine().place(Graph{}).boxes.empty());
  const Frame unit = leaf("u", 10, 10);
  Graph graph;
  graph.root = &unit;
  graph.edges = {{"u", "u", "uses"}};
  EXPECT_TRUE(PackingEngine().place(graph).routes.empty());
}

}  // namespace
