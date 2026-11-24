#include <gtest/gtest.h>

#include <vector>

#include "ds/check/Selection.hpp"

namespace {

using ds::check::Selection;
using ds::check::Step;

const std::vector<Step> kEvery{Step::Linker,       Step::Behaviour, Step::Derivation, Step::Failures,      Step::Interfaces,
                               Step::UseCases,     Step::Diagrams,  Step::Scenarios,  Step::FailureChecks, Step::Structure,
                               Step::Dependencies, Step::Rules,     Step::Levels,     Step::States,        Step::Order};

/// Which steps of the check a load runs (DS-376): a set of steps, every one unless it is said otherwise.
TEST(TestSelection, EveryStepIsInTheWholeSelection) {
  const Selection every = Selection::every();
  for (const Step step : kEvery) EXPECT_TRUE(every.has(step));
}

TEST(TestSelection, ASelectionOfStepsHasThoseAndNoOther) {
  const Selection drawing = Selection::of({Step::Linker, Step::Behaviour, Step::Derivation});
  for (const Step step : kEvery) EXPECT_EQ(drawing.has(step), step == Step::Linker || step == Step::Behaviour || step == Step::Derivation);
  EXPECT_FALSE(Selection::of({}).has(Step::Linker));
}

TEST(TestSelection, AStepAddedIsInTheNewSelectionAndTheOldOneStaysAsItWas) {
  const Selection drawing = Selection::of({Step::Linker});
  const Selection table = drawing.with(Step::Rules);
  EXPECT_TRUE(table.has(Step::Rules));
  EXPECT_TRUE(table.has(Step::Linker));
  EXPECT_FALSE(drawing.has(Step::Rules));
  EXPECT_TRUE(table.with(Step::Rules).has(Step::Rules));  // twice is once
}

}  // namespace
