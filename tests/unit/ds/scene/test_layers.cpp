#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Layers.hpp"
#include "ds/scene/Map.hpp"
#include "ds/scene/Scene.hpp"

namespace {

ds::scene::Node node(const std::string& id, const std::string& parent) {
  ds::scene::Node n;
  n.id = id;
  n.parent = parent;
  return n;
}

/// The map (DS-126): built once from the boxes placed, it says where each identity of the model lands.
TEST(TestMap, AnIdentityLandsOnItselfOrTheNearestBoxThatHoldsIt) {
  const ds::scene::Map map({node("p@m", ""), node("p@m::A", "p@m"), node("p@m::A::run", "p@m::A")});
  EXPECT_EQ(map.land("p@m::A"), "p@m::A");
  EXPECT_EQ(map.land("p@m::A::x"), "p@m::A");                       // a member with no row of its own: its class
  EXPECT_EQ(map.land("p@m::A::run::sequence::m0"), "p@m::A::run");  // a message: the member whose behaviour writes it
  EXPECT_EQ(map.land("q@n::B"), "");                                // nothing on the map stands for it
  EXPECT_TRUE(map.has("p@m"));
  EXPECT_EQ(map.parentOf("p@m::A"), "p@m");
  EXPECT_EQ(map.parentOf("p@m"), "");
}

class TestLayers : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(
        parser.parseText("p.ds",
                         "package p {\n  requirement REQ-00001-1: The system shall run\n  module m {\n"
                         "    class A {\n      @requirement REQ-00001-1\n      - b: B\n      + run() -> none\n    }\n"
                         "    class B {\n      + go() -> int\n    }\n    sequence A::run {\n      self -> b.go() -> n\n    }\n  }\n}\n"));
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  }
};

/// Each perspective draws over the same boxes (DS-126): the layers come in the order they decorate,
/// and each adds its own part of the document.
TEST_F(TestLayers, TheLayersComeInOrderAndEachAddsItsOwnPart) {
  const ds::scene::Map map({});
  const auto all = ds::scene::layers(model, metamodel, map);
  ASSERT_EQ(all.size(), 4u);
  EXPECT_NE(dynamic_cast<ds::scene::StructureLayer*>(all[0].get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::scene::BehaviourLayer*>(all[1].get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::scene::TraceLayer*>(all[2].get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::scene::RulesLayer*>(all[3].get()), nullptr);

  const ds::scene::Scene scene(model, metamodel);
  const ds::scene::SceneDocument doc = scene.build();
  EXPECT_FALSE(doc.links.empty());         // structure: A holds B
  EXPECT_FALSE(doc.behaviours.empty());    // behaviour: A::run
  EXPECT_FALSE(doc.requirements.empty());  // trace: REQ-00001-1
}

/// The scene is an object that lasts (DS-126): placed and layered once, it builds the same document
/// every time it is asked.
TEST_F(TestLayers, TheSceneBuildsTheSameDocumentEveryTime) {
  const ds::scene::Scene scene(model, metamodel);
  EXPECT_EQ(ds::format::SceneJson().write(scene.build(), {}), ds::format::SceneJson().write(scene.build(), {}));
}

}  // namespace
