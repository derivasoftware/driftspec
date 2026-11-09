#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Context.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

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

/// A step gives back what it found (DS-138): a step that enriches reports to the context, which
/// hands the findings back in the order they were said, once.
TEST(TestFindings, TheContextHandsBackWhatAStepReportedInOrderAndOnce) {
  ds::model::Model model;
  ds::bind::Binder binder(model);
  const ds::check::Resolver resolver(model);
  ds::check::Context context(model, resolver, binder);
  context.report({"W-A", "warning", "first", {}});
  context.report({"W-B", "warning", "second", {}});
  const auto taken = context.take();
  ASSERT_EQ(taken.size(), 2u);
  EXPECT_EQ(taken[0].code, "W-A");
  EXPECT_EQ(taken[1].code, "W-B");
  EXPECT_TRUE(context.take().empty());
  EXPECT_TRUE(model.findings().empty());  // the context keeps them; only the checker adds them to the model
}

/// A step that only checks is an ACheck: it is handed the context to read, and a context read
/// gives the model const, so the compiler stops a check that tries to change it.
TEST(TestFindings, AStepThatOnlyChecksReadsTheModelAndCannotChangeIt) {
  using ds::check::ACheck;
  EXPECT_TRUE((std::is_base_of<ACheck, ds::check::ContractCheck>::value));
  EXPECT_TRUE((std::is_base_of<ACheck, ds::check::StructureCheck>::value));
  EXPECT_TRUE((std::is_base_of<ACheck, ds::check::DependencyCheck>::value));
  EXPECT_FALSE((std::is_base_of<ACheck, ds::check::Linker>::value));  // these enrich the model
  EXPECT_FALSE((std::is_base_of<ACheck, ds::check::Derivation>::value));
  EXPECT_FALSE((std::is_base_of<ACheck, ds::check::BehaviourTyping>::value));
  EXPECT_FALSE((std::is_base_of<ACheck, ds::check::RuleCheck>::value));
  EXPECT_TRUE((std::is_same<decltype(std::declval<const ds::check::Context&>().model()), const ds::model::Model&>::value));
}

/// The checker adds each step's findings to the model when the step ends, in the order the steps
/// run: the language of a box is derived before its fields are checked, so on one line the
/// derivation's finding comes first.
TEST(TestFindings, TheCheckerAddsTheFindingsOfEachStepInTheOrderTheStepsRun) {
  ds::model::Model model;
  ds::parse::Parser parser;
  ds::bind::Binder binder(model);
  const auto file = parser.parseText("s.ds", "package shop {\n  language: cobol\n  colour: red\n}\n");
  ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
  binder.bind(file);
  ds::check::Checker(model).run(binder);
  std::vector<std::string> codes;
  for (const auto& f : model.findings()) codes.push_back(f.code);
  EXPECT_EQ(codes, (std::vector<std::string>{"W-LANGUAGE-UNKNOWN", "W-FIELD-UNKNOWN"}));
}

}  // namespace
