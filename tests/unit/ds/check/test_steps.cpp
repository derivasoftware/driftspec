#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Context.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A step of a test: it says its name, what it needs, and one finding when it runs.
class FakeStep : public ds::check::IStep {
 public:
  FakeStep(std::string name, std::vector<std::string> needs, std::vector<std::string>& log)
      : name_(std::move(name)), needs_(std::move(needs)), log_(log) {}
  std::string name() const override { return name_; }
  std::vector<std::string> needs() const override { return needs_; }
  std::vector<ds::model::Finding> run(ds::check::Context&) override {
    log_.push_back(name_);
    return {{"W-" + name_, ds::model::Severity::warning, name_ + " ran", {}}};
  }

 private:
  std::string name_;
  std::vector<std::string> needs_;
  std::vector<std::string>& log_;
};

/// The checker is a runner of steps (DS-138): every step is made in one place, keyed by its name,
/// and run after every step it needs.
TEST(TestSteps, EveryStepIsMadeOnceAndKeyedByItsName) {
  const auto all = ds::check::StepFactory::of();
  ASSERT_EQ(all.size(), 15u);  // ContractCheck became five (DS-382)
  for (const auto& s : all) EXPECT_EQ(s.first, s.second->name());
}

TEST(TestOrder, EachStepRunsAfterWhatItNeeds) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  const auto checkSteps = ds::check::StepFactory::of();
  const ds::check::Checker checker(model, metamodel, checkSteps);
  const std::vector<std::string> order = checker.order();
  EXPECT_EQ(order, (std::vector<std::string>{"linker", "behaviour", "dependencies", "derivation", "failures", "diagrams", "failure-check",
                                             "interfaces", "levels", "order", "rules", "scenarios", "states", "structure", "usecases"}));
  const auto all = ds::check::StepFactory::of();
  for (std::size_t i = 0; i < order.size(); ++i)
    for (const auto& need : all.at(order[i])->needs())
      EXPECT_LT(std::find(order.begin(), order.end(), need) - order.begin(), static_cast<long>(i)) << order[i] << " needs " << need;
}

/// A step needs only the steps that write what it reads (DS-138), not the one before it in a chain:
/// behaviour typing and derivation both start from the linker and neither reads the other, and a
/// step that reads the whole model runs after every step that enriches it.
TEST(TestOrder, AStepNeedsTheStepsThatWriteWhatItReads) {
  const auto all = ds::check::StepFactory::of();
  using Needs = std::vector<std::string>;
  EXPECT_EQ(all.at("linker")->needs(), Needs{});
  EXPECT_EQ(all.at("behaviour")->needs(), Needs{"linker"});
  EXPECT_EQ(all.at("derivation")->needs(), Needs{"linker"});
  // what ContractCheck needed, each of the five it became (DS-382): the failure check reads what FailureFlow wrote
  for (const char* contract : {"interfaces", "usecases", "diagrams", "scenarios", "failure-check"})
    EXPECT_EQ(all.at(contract)->needs(), (Needs{"linker", "behaviour", "failures"})) << contract;
  EXPECT_EQ(all.at("dependencies")->needs(), (Needs{"linker", "behaviour"}));
  EXPECT_EQ(all.at("structure")->needs(), (Needs{"linker", "behaviour", "derivation"}));
  EXPECT_EQ(all.at("rules")->needs(), (Needs{"linker", "behaviour", "derivation"}));
  EXPECT_EQ(all.at("failures")->needs(), (Needs{"linker", "behaviour"}));  // the calls resolved (DS-188)
}

/// The keys of the steps made for a selection, in the order of the map.
std::vector<std::string> keysOf(const ds::check::Selection& selection) {
  std::vector<std::string> out;
  for (const auto& s : ds::check::StepFactory::of(selection)) out.push_back(s.first);
  return out;
}

/// A load runs the steps it is given (DS-376): each step a selection names is made under its own name, the fifteen
/// steps of the check are fifteen steps of a selection, and with none named none is made.
TEST(TestSteps, EachStepASelectionNamesIsMadeUnderItsOwnName) {
  using ds::check::Selection;
  using ds::check::Step;
  const std::vector<std::pair<Step, std::string>> named{{Step::Linker, "linker"},
                                                        {Step::Behaviour, "behaviour"},
                                                        {Step::Derivation, "derivation"},
                                                        {Step::Failures, "failures"},
                                                        {Step::Interfaces, "interfaces"},
                                                        {Step::UseCases, "usecases"},
                                                        {Step::Diagrams, "diagrams"},
                                                        {Step::Scenarios, "scenarios"},
                                                        {Step::FailureChecks, "failure-check"},
                                                        {Step::Structure, "structure"},
                                                        {Step::Dependencies, "dependencies"},
                                                        {Step::Rules, "rules"},
                                                        {Step::Levels, "levels"},
                                                        {Step::States, "states"},
                                                        {Step::Order, "order"}};
  std::vector<std::string> names;
  for (const auto& n : named) {
    const auto made = ds::check::StepFactory::of(Selection::of({n.first}));
    ASSERT_TRUE(made.count(n.second)) << n.second;
    EXPECT_EQ(made.at(n.second)->name(), n.second);
    names.push_back(n.second);
  }
  std::sort(names.begin(), names.end());
  EXPECT_EQ(keysOf(Selection::every()), names);  // one step each, and every step of the check
  EXPECT_EQ(keysOf(Selection::every()), keysOf(Selection::every().with(Step::Rules)));
  EXPECT_EQ(ds::check::StepFactory::of().size(), 15u);  // with no selection, every step
  EXPECT_TRUE(ds::check::StepFactory::of(Selection::of({})).empty());
}

/// A step chosen brings every step it needs (DS-376), so none runs without what it reads: the rules bring what
/// writes the finished model, the interfaces and the failure check what escapes each operation, and the checker orders what it is given.
TEST(TestSteps, AStepChosenBringsEveryStepItNeeds) {
  using ds::check::Selection;
  using ds::check::Step;
  using Keys = std::vector<std::string>;
  EXPECT_EQ(keysOf(Selection::of({Step::Linker})), Keys{"linker"});
  EXPECT_EQ(keysOf(Selection::of({Step::Rules})), (Keys{"behaviour", "derivation", "linker", "rules"}));
  EXPECT_EQ(keysOf(Selection::of({Step::Interfaces})), (Keys{"behaviour", "failures", "interfaces", "linker"}));
  EXPECT_EQ(keysOf(Selection::of({Step::FailureChecks})), (Keys{"behaviour", "failure-check", "failures", "linker"}));
  EXPECT_EQ(keysOf(Selection::of({Step::Order})), (Keys{"behaviour", "linker", "order"}));
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  const auto drawing = ds::check::StepFactory::of(Selection::of({Step::Derivation, Step::Behaviour}));
  EXPECT_EQ(ds::check::Checker(model, metamodel, drawing).order(), (Keys{"linker", "behaviour", "derivation"}));
}

/// A step gives back what it found (DS-138): a step that enriches reports to the context, which
/// hands the findings back in the order they were said, once.
TEST(TestFindings, TheContextHandsBackWhatAStepReportedInOrderAndOnce) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::bind::Binder binder(model, metamodel);
  const ds::check::Resolver resolver(model);
  ds::check::Context context(model, metamodel, resolver, binder.notes());
  context.report({"W-A", ds::model::Severity::warning, "first", {}});
  context.report({"W-B", ds::model::Severity::warning, "second", {}});
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
  EXPECT_TRUE((std::is_base_of<ACheck, ds::check::InterfaceContracts>::value));  // what ContractCheck was (DS-382)
  EXPECT_TRUE((std::is_base_of<ACheck, ds::check::UseCaseContracts>::value));
  EXPECT_TRUE((std::is_base_of<ACheck, ds::check::FailureCheck>::value));  // reads what FailureFlow wrote
  EXPECT_TRUE((std::is_base_of<ACheck, ds::check::ScenarioCheck>::value));
  EXPECT_TRUE((std::is_base_of<ACheck, ds::check::DiagramCheck>::value));
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
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  const auto file = parser.parseText("s.ds", "package shop {\n  language: cobol\n  colour: red\n}\n");
  ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
  binder.bind(file);
  const auto checkSteps = ds::check::StepFactory::of();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  std::vector<std::string> codes;
  for (const auto& f : model.findings()) codes.push_back(f.code);
  EXPECT_EQ(codes, (std::vector<std::string>{"W-LANGUAGE-UNKNOWN", "W-FIELD-UNKNOWN"}));
}

/// The checker runs the steps it is given (DS-138): the map is built outside, so a test can hand it
/// steps of its own. It orders them by what they need, not by their keys, and adds each one's
/// findings to the model in that order.
TEST(TestChecker, RunsTheStepsItIsGivenAfterWhatEachNeeds) {
  std::vector<std::string> log;
  std::map<std::string, std::unique_ptr<ds::check::IStep>> given;
  given["a"] = std::unique_ptr<ds::check::IStep>(new FakeStep("a", {"c"}, log));
  given["b"] = std::unique_ptr<ds::check::IStep>(new FakeStep("b", {}, log));
  given["c"] = std::unique_ptr<ds::check::IStep>(new FakeStep("c", {"b"}, log));
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::bind::Binder binder(model, metamodel);
  ds::check::Checker checker(model, metamodel, given);
  ds::check::IChecker& entry = checker;  // what the rest of ds sees
  entry.run(binder.notes());
  EXPECT_EQ(log, (std::vector<std::string>{"b", "c", "a"}));
  std::vector<std::string> codes;
  for (const auto& f : model.findings()) codes.push_back(f.code);
  EXPECT_EQ(codes, (std::vector<std::string>{"W-b", "W-c", "W-a"}));
}

TEST(TestChecker, StepsThatNeedEachOtherAreACycleAndNoOrder) {
  std::vector<std::string> log;
  std::map<std::string, std::unique_ptr<ds::check::IStep>> given;
  given["a"] = std::unique_ptr<ds::check::IStep>(new FakeStep("a", {"b"}, log));
  given["b"] = std::unique_ptr<ds::check::IStep>(new FakeStep("b", {"a"}, log));
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  const ds::check::Checker checker(model, metamodel, given);
  EXPECT_THROW(checker.order(), std::logic_error);
}

/// The checker holds the map by reference, so it refuses a temporary one, which would die first.
TEST(TestChecker, TakesNoTemporaryMap) {
  EXPECT_TRUE((std::is_constructible<ds::check::Checker, ds::model::Model&, const ds::model::Metamodel&,
                                     const std::map<std::string, std::unique_ptr<ds::check::IStep>>&>::value));
  EXPECT_FALSE((std::is_constructible<ds::check::Checker, ds::model::Model&, const ds::model::Metamodel&,
                                      std::map<std::string, std::unique_ptr<ds::check::IStep>>&&>::value));
  EXPECT_TRUE((std::is_base_of<ds::check::IChecker, ds::check::Checker>::value));
}

/// The factory by its own name (DS-404): every step it makes is keyed by the name the step gives, and the checker it
/// makes is handed back as an IChecker that runs them.
TEST(TestStepFactory, EveryStepItMakesIsKeyedByItsOwnName) {
  for (const auto& s : ds::check::StepFactory::of()) EXPECT_EQ(s.first, s.second->name());
}

TEST(TestCheckerFactory, TheCheckerItMakesRunsTheStepsItIsGiven) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  const auto steps = ds::check::StepFactory::of();
  const std::unique_ptr<ds::check::IChecker> checker = ds::check::CheckerFactory::make(model, metamodel, steps);
  ASSERT_NE(checker, nullptr);
  checker->run(ds::bind::Notes{});
  EXPECT_TRUE(model.findings().empty());  // an empty model breaks nothing
}

}  // namespace
