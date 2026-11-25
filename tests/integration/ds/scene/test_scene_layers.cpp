#include <gtest/gtest.h>

#include <string>

#include "ds/api/Api.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Layers.hpp"
#include "ds/scene/Map.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// The scene of a design (REQ-00069-2): boxes placed once, a map that says where each identity of
/// the model lands on them, and the layers that draw each perspective over the same boxes.
class TestSceneLayersOfADesign : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    model = ds::api::Api(metamodel).load(
        {parser.parseText("p.ds",
                          "package p {\n  requirement REQ-00001-1: The system shall run\n  module m {\n"
                          "    class A {\n      @requirement REQ-00001-1\n      - b: B\n      + run() -> none\n    }\n"
                          "    class B {\n      + go() -> int\n    }\n    sequence A::run {\n      self -> b.go() -> n\n    }\n  }\n}\n")});
  }
};

TEST_F(TestSceneLayersOfADesign, EveryIdentityOfTheDesignLandsOnItselfOrTheNearestBoxThatHoldsIt) {
  const ds::scene::SceneDocument doc = ds::scene::Scene(model, metamodel).build();
  const ds::scene::Map map(doc.nodes);
  EXPECT_EQ(map.land("p@m::A"), "p@m::A");
  EXPECT_EQ(map.land("p@m::A::run"), "p@m::A::run");
  EXPECT_EQ(map.land("p@m::A::run::sequence::m0"), "p@m::A::run");  // a message: the member whose behaviour writes it
  EXPECT_EQ(map.land("q@n::C"), "");                                // nothing on the map stands for it
  EXPECT_EQ(map.parentOf("p@m::A"), "p@m");
  for (const auto& pair : model.entities()) {
    if (pair.second.kind == ds::model::Kind::Class || pair.second.kind == ds::model::Kind::Method) {
      EXPECT_TRUE(map.has(pair.first)) << pair.first;
    }
  }
}

TEST_F(TestSceneLayersOfADesign, TheLayersComeInOrderAndEachAddsItsOwnPartOfTheDesign) {
  const ds::scene::SceneDocument doc = ds::scene::Scene(model, metamodel).build();
  const ds::scene::Map map(doc.nodes);
  const auto all = ds::scene::LayerFactory::of(model, metamodel, map);
  ASSERT_EQ(all.size(), 4u);
  EXPECT_NE(dynamic_cast<ds::scene::StructureLayer*>(all[0].get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::scene::BehaviourLayer*>(all[1].get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::scene::TraceLayer*>(all[2].get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::scene::RulesLayer*>(all[3].get()), nullptr);
  EXPECT_FALSE(doc.links.empty());         // structure: A holds B
  EXPECT_FALSE(doc.behaviours.empty());    // behaviour: A::run
  EXPECT_FALSE(doc.requirements.empty());  // trace: REQ-00001-1
}

TEST_F(TestSceneLayersOfADesign, TheSceneBuildsTheSameDocumentEveryTime) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::scene::Scene scene(model, metamodel);
  const auto described = [&sink](const ds::scene::SceneDocument& d) {  // the scene's description in the sink (DS-398)
    ds::format::DescriptionFactory::scene(d)->describe(sink);
    return sink.result();
  };
  const std::string first = described(scene.build());
  EXPECT_EQ(first, described(scene.build()));
  EXPECT_EQ(first, described(ds::scene::Scene(model, metamodel).build()));  // and for another scene of the same model
}

}  // namespace
