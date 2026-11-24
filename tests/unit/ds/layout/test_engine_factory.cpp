#include <gtest/gtest.h>

#include "ds/layout/EngineFactory.hpp"
#include "ds/layout/Frame.hpp"
#include "ds/layout/Graph.hpp"
#include "ds/layout/PackingEngine.hpp"

namespace {

/// The one place that names the layout's engines (DS-392): whoever places is handed an IEngine.
TEST(TestEngineFactory, ThePackingEngineIsHandedOutAsAnEngine) {
  const auto engine = ds::layout::EngineFactory::packing();
  ASSERT_NE(engine, nullptr);
  EXPECT_NE(dynamic_cast<const ds::layout::PackingEngine*>(engine.get()), nullptr);
  ds::layout::Frame unit;
  unit.id = "u";
  unit.w = 120;
  unit.h = 40;
  ds::layout::Graph graph;
  graph.root = &unit;
  EXPECT_DOUBLE_EQ(engine->place(graph).boxes.at("u").w, 120);
}

}  // namespace
