#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Frame.hpp"
#include "ds/scene/Projection.hpp"

namespace {

using ds::scene::Frame;
using ds::scene::Projection;

/// What the scene places, before it is placed (DS-389): the model as nested frames, each sized by what it shows,
/// with what each box drawn says it is.
class TestProjection : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("p.ds",
                                 "package p {\n"
                                 "  interface shell {\n"
                                 "    kind: cli\n"
                                 "    operation greet(name: string) -> string {\n"
                                 "      command: p greet <name>\n"
                                 "    }\n"
                                 "  }\n"
                                 "  module m {\n"
                                 "    class Base {\n"
                                 "      + run() -> void\n"
                                 "    }\n"
                                 "    class Heir : Base {\n"
                                 "      + walk(steps: int) -> void\n"
                                 "      - secret: int\n"
                                 "    }\n"
                                 "    class Alone {\n"
                                 "      + one() -> void\n"
                                 "      + two() -> void\n"
                                 "      + three() -> void\n"
                                 "    }\n"
                                 "  }\n"
                                 "}\n"));
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  static const Frame& only(const std::vector<Frame>& frames, const std::string& id) {
    for (const Frame& f : frames)
      if (f.id == id) return f;
    throw std::runtime_error("no frame " + id);
  }
};

TEST_F(TestProjection, ThePackagesSitUnderOneRootAndAPackageShelvesItsModulesWithItsInterfacesAsPorts) {
  const Projection projected(model, metamodel, false);
  ASSERT_EQ(projected.root().kids.size(), 1u);
  const Frame& p = projected.root().kids.front();
  EXPECT_EQ(p.id, "p");
  EXPECT_EQ(p.arrangement, Frame::Arrangement::Shelves);
  ASSERT_EQ(p.kids.size(), 1u);
  EXPECT_EQ(p.kids.front().id, "p@m");
  ASSERT_EQ(p.ports.size(), 1u);
  EXPECT_EQ(p.ports.front().id, "p@shell");
  EXPECT_EQ(p.ports.front().kids.size(), 1u);  // one row per operation
  EXPECT_GT(p.left, p.right);                  // room on the left for the ports
}

TEST_F(TestProjection, AModuleReadsItsTreesOfInheritanceFirstEachBaseOverWhatInheritsFromIt) {
  const Projection projected(model, metamodel, false);
  const Frame& m = projected.root().kids.front().kids.front();
  ASSERT_EQ(m.kids.size(), 2u);
  const Frame& tree = m.kids.front();
  EXPECT_EQ(tree.arrangement, Frame::Arrangement::Tree);
  ASSERT_EQ(tree.kids.size(), 2u);
  EXPECT_EQ(tree.kids[0].id, "p@m::Base");
  EXPECT_EQ(tree.kids[1].id, "p@m::Heir");
  EXPECT_EQ(m.kids[1].id, "p@m::Alone");
  EXPECT_FALSE(projected.nodes().count(tree.id));  // a tree only groups: it is not drawn
}

TEST_F(TestProjection, AUnitIsAsTallAsItsRowsAndSaysWhatEachRowIs) {
  const Projection projected(model, metamodel, false);
  const Frame alone = only(projected.root().kids.front().kids.front().kids, "p@m::Alone");
  EXPECT_EQ(alone.kids.size(), 3u);
  EXPECT_DOUBLE_EQ(alone.h, 22 + 3 * 11 + 6);  // the head, a row each, and the padding below
  EXPECT_GE(alone.w, 150);
  ASSERT_EQ(alone.at.size(), 3u);
  EXPECT_DOUBLE_EQ(alone.at[1].second, 22 + 11);
  const auto& row = projected.nodes().at(alone.kids[0].id);
  EXPECT_EQ(row.kind, "row");
  EXPECT_EQ(row.parent, "p@m::Alone");
  EXPECT_EQ(projected.nodes().at("p@m::Alone").kind, "unit");
}

TEST_F(TestProjection, TheLeanProjectionLeavesPrivateMembersOutAndShrinksTheirClass) {
  const Projection full(model, metamodel, false), lean(model, metamodel, true);
  const Frame& heirFull = full.root().kids.front().kids.front().kids.front().kids[1];
  const Frame& heirLean = lean.root().kids.front().kids.front().kids.front().kids[1];
  EXPECT_EQ(heirFull.kids.size(), 2u);
  EXPECT_EQ(heirLean.kids.size(), 1u);
  EXPECT_LT(heirLean.h, heirFull.h);
  EXPECT_TRUE(full.nodes().count("p@m::Heir::secret"));
  EXPECT_FALSE(lean.nodes().count("p@m::Heir::secret"));
}

}  // namespace
