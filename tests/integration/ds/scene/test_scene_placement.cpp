#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/layout/EngineFactory.hpp"
#include "ds/layout/Graph.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Layout.hpp"
#include "ds/scene/Projection.hpp"

namespace {

/// The scene places its boxes with the layout's packing engine (REQ-00189-2): every box of the scene's document is
/// where that engine places the projection's frames, over examples/hola.
class TestScenePlacement : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola")) binder.bind(f);
    ds::check::Context context(model, metamodel);
    const auto checkSteps = ds::check::StepFactory::of(context);
    ds::check::Checker(context, checkSteps).run(binder.notes());
  }
};

TEST_F(TestScenePlacement, EveryBoxOfTheSceneIsWhereThePackingEnginePlacesTheProjectionsFrames) {
  const ds::scene::SceneDocument doc = ds::scene::Layout(model, metamodel).place();
  const ds::scene::Projection full(model, metamodel, false);
  ds::layout::Graph graph;
  graph.root = &full.root();
  const ds::layout::Placement placed = ds::layout::EngineFactory::packing()->place(graph);
  ASSERT_FALSE(doc.nodes.empty());
  for (const auto& n : doc.nodes) {
    const auto it = placed.boxes.find(n.id);
    ASSERT_NE(it, placed.boxes.end()) << n.id;
    EXPECT_DOUBLE_EQ(n.full.x, it->second.x) << n.id;
    EXPECT_DOUBLE_EQ(n.full.y, it->second.y) << n.id;
    EXPECT_DOUBLE_EQ(n.full.w, it->second.w) << n.id;
    EXPECT_DOUBLE_EQ(n.full.h, it->second.h) << n.id;
  }
}

}  // namespace
