#include <gtest/gtest.h>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/scene/Layers.hpp"
#include "ds/scene/Map.hpp"

namespace {

/// The one place that names the concrete layers (DS-390): it makes each, in the order they decorate, over the model
/// and the map it is given, and makes them anew each time it is asked.
TEST(TestLayerFactory, ItMakesEveryLayerInTheOrderTheyDecorate) {
  const ds::model::Metamodel metamodel;
  const ds::model::Model model{metamodel};
  const ds::scene::Map map({});
  const auto first = ds::scene::LayerFactory::of(model, metamodel, map);
  ASSERT_EQ(first.size(), 5u);
  EXPECT_NE(dynamic_cast<ds::scene::StructureLayer*>(first[0].get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::scene::BehaviourLayer*>(first[1].get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::scene::TraceLayer*>(first[2].get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::scene::RulesLayer*>(first[3].get()), nullptr);
  EXPECT_NE(dynamic_cast<ds::scene::WalkthroughLayer*>(first[4].get()), nullptr);  // DS-471
  const auto second = ds::scene::LayerFactory::of(model, metamodel, map);
  ASSERT_EQ(second.size(), 5u);
  EXPECT_NE(first[0].get(), second[0].get());  // each asking gets its own layers
}

}  // namespace
