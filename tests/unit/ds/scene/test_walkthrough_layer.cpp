#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/scene/Document.hpp"
#include "ds/scene/Layers.hpp"
#include "ds/scene/Map.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;

/// The walkthroughs of the scene (DS-471): each step names its diagram as the scene's diagrams do, lands what it lights
/// on the nodes drawn, and a step that shows a sequence carries the behaviour it replays along its scenario.
TEST(TestWalkthroughLayer, EachStepIsLandedOnTheNodesDrawnAndASequenceStepNamesItsBehaviour) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  const auto declare = [&model](Kind kind, const std::string& id, std::map<std::string, std::string> fields) {
    Entity e;
    e.kind = kind;
    e.id = id;
    e.name = id;
    e.fields = std::move(fields);
    model.declare(e);
  };
  declare(Kind::Diagram, "diagram:WALK_A", {{"kind", "walkthrough"}, {"title", "A walk"}});
  declare(Kind::Diagram, "diagram:SEQ_Run", {{"kind", "sequence"}, {"include", "p@m::A::run"}});
  declare(Kind::Class, "p@m::A", {});
  declare(Kind::Step, "diagram:WALK_A::1", {{"show", "SEQ_Run"}, {"scenario", "late"}, {"text", "It runs."}});
  model.relate({"contains", "diagram:WALK_A", "diagram:WALK_A::1", {}, ""});
  model.relate({"shows", "diagram:WALK_A::1", "diagram:SEQ_Run", {}, ""});
  model.relate({"focuses", "diagram:WALK_A::1", "p@m::A", {}, ""});
  model.relate({"points-at", "diagram:WALK_A::1", "p@m::A", {}, ""});
  ds::scene::Node node;
  node.id = "p@m::A";
  node.kind = "unit";
  const ds::scene::Map map({node});
  ds::scene::SceneDocument doc;
  ds::scene::WalkthroughLayer(model, map).decorate(doc);
  ASSERT_EQ(doc.walkthroughs.size(), 1u);
  EXPECT_EQ(doc.walkthroughs[0].name, "diagram:WALK_A");
  EXPECT_EQ(doc.walkthroughs[0].title, "A walk");
  ASSERT_EQ(doc.walkthroughs[0].steps.size(), 1u);
  const auto& step = doc.walkthroughs[0].steps[0];
  EXPECT_EQ(step.diagram, "SEQ_Run");
  EXPECT_EQ(step.behaviour, "p@m::A::run::sequence/late");
  EXPECT_EQ(step.focus, std::vector<std::string>{"p@m::A"});
  EXPECT_EQ(step.at, "p@m::A");
  EXPECT_EQ(step.text, "It runs.");
}

}  // namespace
