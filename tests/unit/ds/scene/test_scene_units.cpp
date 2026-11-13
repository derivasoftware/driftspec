#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Layers.hpp"
#include "ds/scene/Layout.hpp"
#include "ds/scene/Map.hpp"

namespace {

/// Each layer of the scene on its own, over boxes the layout placed (DS-186): a layer adds its part
/// of the document and nothing of another's.
class SceneOf : public ::testing::Test {
 protected:
  ds::model::Model model;
  ds::scene::SceneDocument placed;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    binder.bind(parser.parseText("p.ds",
                                 "package p {\n"
                                 "  requirement REQ-00001-1: The system shall run\n"
                                 "  verification VER-00001-1 {\n"
                                 "    @requirement REQ-00001-1\n"
                                 "    title: Inspection confirms it runs\n"
                                 "    method: inspection\n"
                                 "  }\n"
                                 "  rule a-draws-nothing {\n"
                                 "    title: A never draws\n"
                                 "    @requirement REQ-00001-1\n"
                                 "    for class c in p@m\n"
                                 "    never m in c, s behaviour-of m, s uses x, x in p@render\n"
                                 "  }\n"
                                 "  module m {\n"
                                 "    class A {\n"
                                 "      @requirement REQ-00001-1\n"
                                 "      - b: B\n"
                                 "      + run() -> none\n"
                                 "    }\n"
                                 "    class B {\n"
                                 "      + go() -> int\n"
                                 "    }\n"
                                 "    sequence A::run {\n"
                                 "      self -> b.go() -> n\n"
                                 "    }\n"
                                 "  }\n"
                                 "}\n"));
    const auto steps = ds::check::steps();
    ds::check::Checker(model, steps).run(binder);
    placed = ds::scene::Layout(model).place();
  }
};

class TestLayout : public SceneOf {};

TEST_F(TestLayout, EveryBoxIsPlacedInsideItsParentAndTheSameModelPlacesTheSameWay) {
  std::map<std::string, ds::scene::Node> by;
  for (const auto& n : placed.nodes) by[n.id] = n;
  ASSERT_TRUE(by.count("p@m::A") && by.count("p@m"));
  EXPECT_EQ(by["p@m::A"].parent, "p@m");
  const auto& a = by["p@m::A"].full;
  const auto& m = by["p@m"].full;
  EXPECT_TRUE(a.x >= m.x && a.y >= m.y && a.x + a.w <= m.x + m.w && a.y + a.h <= m.y + m.h);
  const auto again = ds::scene::Layout(model).place();
  ASSERT_EQ(again.nodes.size(), placed.nodes.size());
  for (size_t i = 0; i < placed.nodes.size(); ++i) EXPECT_EQ(again.nodes[i].full.x, placed.nodes[i].full.x);
}

class TestStructureLayer : public SceneOf {};

TEST_F(TestStructureLayer, ItAddsTheLinksBetweenBoxesAndNoBehaviour) {
  ds::scene::SceneDocument doc = placed;
  const ds::scene::Map map(doc.nodes);
  ds::scene::StructureLayer(model, map).decorate(doc);
  EXPECT_FALSE(doc.links.empty());  // A holds B
  EXPECT_TRUE(doc.behaviours.empty());
}

class TestBehaviourLayer : public SceneOf {};

TEST_F(TestBehaviourLayer, ItAddsEachPublicBehaviourOnceAndNoLink) {
  ds::scene::SceneDocument doc = placed;
  const ds::scene::Map map(doc.nodes);
  ds::scene::BehaviourLayer(model, map).decorate(doc);
  ASSERT_EQ(doc.behaviours.size(), 1u);
  EXPECT_EQ(doc.behaviours[0].subject, "p@m::A::run");
  EXPECT_TRUE(doc.links.empty());
}

class TestTraceLayer : public SceneOf {};

TEST_F(TestTraceLayer, ItAddsTheRequirementsAndWhatVerifiesThem) {
  ds::scene::SceneDocument doc = placed;
  const ds::scene::Map map(doc.nodes);
  ds::scene::TraceLayer(model, map).decorate(doc);
  ASSERT_EQ(doc.requirements.size(), 1u);
  EXPECT_EQ(doc.requirements[0].id, "REQ-00001-1");
  ASSERT_EQ(doc.verifications.size(), 1u);
  EXPECT_EQ(doc.verifications[0].id, "VER-00001-1");
}

class TestRulesLayer : public SceneOf {};

TEST_F(TestRulesLayer, ItAddsEachRuleWhereItIsWritten) {
  ds::scene::SceneDocument doc = placed;
  const ds::scene::Map map(doc.nodes);
  ds::scene::RulesLayer(model, map).decorate(doc);
  ASSERT_EQ(doc.rules.size(), 1u);
  EXPECT_EQ(doc.rules[0].title, "A never draws");
}

}  // namespace
