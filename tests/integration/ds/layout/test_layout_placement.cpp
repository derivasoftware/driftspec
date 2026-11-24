#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <string>

#include "ds/layout/EngineFactory.hpp"
#include "ds/layout/Frame.hpp"
#include "ds/layout/Graph.hpp"
#include "ds/layout/Placement.hpp"

namespace {

/// The boxes of a placement by the identities of their frames, which the engine numbers in the order the graph is read:
/// the root first, then each frame followed by its kids and its ports.
std::map<std::string, ds::layout::Box> byId(const ds::layout::Frame& root, const ds::layout::Placement& placed) {
  std::map<std::string, ds::layout::Box> out;
  std::size_t n = 0;
  std::function<void(const ds::layout::Frame&)> walk = [&](const ds::layout::Frame& f) {
    if (n < placed.boxes.size()) out[f.id] = placed.boxes[n];
    ++n;
    for (const ds::layout::Frame& k : f.kids) walk(k);
    for (const ds::layout::Frame& p : f.ports) walk(p);
  };
  walk(root);
  return out;
}

using ds::layout::Box;
using ds::layout::Frame;
using ds::layout::Graph;
using ds::layout::Placement;

/// The layout places nested boxes of given sizes, knowing nothing of what they stand for (REQ-00210-1): a graph of
/// frames, packed by the engine the factory hands out, as the scene hands it a package of modules and units.
class TestLayoutPlacement : public ::testing::Test {
 protected:
  static Frame leaf(const std::string& id, double w, double h) {
    Frame f;
    f.id = id;
    f.w = w;
    f.h = h;
    return f;
  }
  /// A package of two modules, one of them with a tree of three units, the other with two units, and a port.
  static Frame package(double unitH) {
    Frame tree;
    tree.id = "base#tree";
    tree.arrangement = Frame::Arrangement::Tree;
    tree.gap = 22;
    tree.drop = 44;
    tree.kids = {leaf("base", 150, unitH), leaf("heir1", 150, unitH), leaf("heir2", 160, unitH)};
    Frame m1;
    m1.id = "p@m1";
    m1.arrangement = Frame::Arrangement::Shelves;
    m1.gap = 22, m1.atLeast = 260, m1.factor = 1.5, m1.left = 24, m1.top = 46, m1.right = 24, m1.bottom = 24;
    m1.kids = {tree};
    Frame m2 = m1;
    m2.id = "p@m2";
    m2.kids = {leaf("a", 200, unitH), leaf("b", 180, unitH)};
    Frame p;
    p.id = "p";
    p.arrangement = Frame::Arrangement::Shelves;
    p.gap = 70, p.factor = 1.6, p.left = 300, p.top = 90, p.right = 60, p.bottom = 60, p.portTop = 130, p.portGap = 20;
    p.kids = {m1, m2};
    p.ports = {leaf("p@shell", 360, 86)};
    Frame root;
    root.arrangement = Frame::Arrangement::Shelves;
    root.gap = 120;
    root.factor = 1.6;
    root.kids = {p};
    return root;
  }
  static Placement place(const Frame& root, const std::map<std::string, double>& kept = {}, bool following = false) {
    Graph graph;
    graph.root = &root;
    graph.widths = kept;
    graph.following = following;
    return ds::layout::EngineFactory::packing()->place(graph);
  }
  static bool inside(const Box& in, const Box& out) {
    return in.x >= out.x && in.y >= out.y && in.x + in.w <= out.x + out.w && in.y + in.h <= out.y + out.h;
  }
  static bool apart(const Box& a, const Box& b) { return a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y; }
};

TEST_F(TestLayoutPlacement, EveryBoxLandsInsideItsParentApartFromItsSiblings) {
  const Frame root = package(50);
  const auto b = byId(root, place(root));
  for (const char* unit : {"base", "heir1", "heir2"}) EXPECT_TRUE(inside(b.at(unit), b.at("p@m1"))) << unit;
  for (const char* unit : {"a", "b"}) EXPECT_TRUE(inside(b.at(unit), b.at("p@m2"))) << unit;
  EXPECT_TRUE(inside(b.at("p@m1"), b.at("p")));
  EXPECT_TRUE(inside(b.at("p@m2"), b.at("p")));
  EXPECT_TRUE(apart(b.at("p@m1"), b.at("p@m2")));
  EXPECT_TRUE(apart(b.at("heir1"), b.at("heir2")));
  EXPECT_GT(b.at("heir1").y, b.at("base").y + b.at("base").h);   // what inherits sits below its base
  EXPECT_DOUBLE_EQ(b.at("p@shell").x, b.at("p").x - 360.0 / 2);  // the port half outside its package
}

TEST_F(TestLayoutPlacement, ASecondPlacementOfShorterBoxesFollowingTheFirstIsNeverWider) {
  const Frame tall = package(80), shorter = package(30);
  const Placement full = place(tall);
  const auto fullBoxes = byId(tall, full);
  const auto leanBoxes = byId(shorter, place(shorter, full.widths, true));
  for (const auto& entry : leanBoxes) {
    const auto f = fullBoxes.find(entry.first);
    ASSERT_NE(f, fullBoxes.end()) << entry.first;
    EXPECT_LE(entry.second.w, f->second.w) << entry.first;
    EXPECT_LE(entry.second.h, f->second.h) << entry.first;
  }
}

}  // namespace
