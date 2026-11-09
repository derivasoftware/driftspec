#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"

namespace {

/// The checker is a runner of steps (DS-138): every step is made in one place, keyed by its name,
/// and run after every step it needs.
TEST(TestSteps, EveryStepIsMadeOnceAndKeyedByItsName) {
  const auto all = ds::check::steps();
  ASSERT_EQ(all.size(), 7u);
  for (const auto& s : all) EXPECT_EQ(s.first, s.second->name());
}

TEST(TestOrder, EachStepRunsAfterWhatItNeeds) {
  ds::model::Model model;
  const ds::check::Checker checker(model);
  const std::vector<std::string> order = checker.order();
  EXPECT_EQ(order, (std::vector<std::string>{"linker", "behaviour", "contracts", "derivation", "structure", "dependencies", "rules"}));
  const auto all = ds::check::steps();
  for (std::size_t i = 0; i < order.size(); ++i)
    for (const auto& need : all.at(order[i])->needs())
      EXPECT_LT(std::find(order.begin(), order.end(), need) - order.begin(), static_cast<long>(i)) << order[i] << " needs " << need;
}

}  // namespace
