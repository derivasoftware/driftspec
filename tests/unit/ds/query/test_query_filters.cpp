#include <gtest/gtest.h>

#include <set>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Filter.hpp"
#include "ds/query/Query.hpp"

namespace {

using Ids = std::set<std::string>;

/// Each filter built with its criteria and asked directly, and the query on its own (DS-186), over
/// a shop: a store of items, a till that sells from it, a stock with a machine, two requirements.
class Shop : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("shop.ds",
                                       "package shop {\n"
                                       "  requirement REQ-00001-1: The shop shall sell\n"
                                       "  requirement REQ-00002-1 {\n"
                                       "    @refines REQ-00001-1\n"
                                       "    title: The till shall take money\n"
                                       "  }\n"
                                       "  verification VER-00001-1 {\n"
                                       "    @requirement REQ-00002-1\n"
                                       "    title: Tests confirm the till takes money\n"
                                       "    method: inspection\n"
                                       "  }\n"
                                       "  module store {\n"
                                       "    class Item { + price: int }\n"
                                       "    class Stock {\n"
                                       "      + take(n: int) -> Item\n"
                                       "      + refill() -> none\n"
                                       "    }\n"
                                       "    class Ledger { }\n"
                                       "    state Stock {\n"
                                       "      initial Full\n"
                                       "      Full -> Empty on take\n"
                                       "      Empty -> Full on refill\n"
                                       "    }\n"
                                       "  }\n"
                                       "  module till {\n"
                                       "    class Till {\n"
                                       "      @requirement REQ-00002-1\n"
                                       "      - stock: store::Stock\n"
                                       "      + sell(n: int) -> int\n"
                                       "    }\n"
                                       "    sequence Till::sell {\n"
                                       "      self -> stock.take(n) -> item\n"
                                       "      return n\n"
                                       "    }\n"
                                       "  }\n"
                                       "}\n");
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto steps = ds::check::steps();
    ds::check::Checker(model, metamodel, steps).run(binder.notes());
  }
};

class TestClassFilter : public Shop {};

TEST_F(TestClassFilter, ItsBoxesAreThePatternsLessTheExclusionsAndTheModulesAroundThemAreAdded) {
  const ds::query::ClassFilter filter({"shop@store::*"}, {"shop@store::Ledger"}, 0);
  EXPECT_EQ(filter.boxes(model), (Ids{"shop@store::Item", "shop@store::Stock"}));
  EXPECT_EQ(filter.select(model), (Ids{"shop@store", "shop@store::Item", "shop@store::Stock"}));
}

TEST_F(TestClassFilter, WithNoDiagramToTakeACutFromItShowsEveryBox) {
  const ds::query::ClassFilter filter({"shop@*"}, {}, 0, "diagram:Nowhere");  // the cut itself is TestCut's
  EXPECT_EQ(filter.select(model), ds::query::ClassFilter({"shop@*"}, {}, 0).select(model));
}

class TestSequenceFilter : public Shop {};

TEST_F(TestSequenceFilter, ItTakesTheBehavioursItsPatternsNameAndWhoTakesPart) {
  const ds::query::SequenceFilter filter({"shop@till::Till::sell"}, 1);
  const auto taken = filter.behaviours(model);
  ASSERT_EQ(taken.size(), 1u);
  EXPECT_EQ(taken[0].sequence->id, "shop@till::Till::sell::sequence");
  EXPECT_EQ(filter.select(model), (Ids{"shop@till::Till", "shop@store::Stock"}));
}

class TestStateFilter : public Shop {};

TEST_F(TestStateFilter, ItTakesTheMachinesItNames) {
  const ds::query::StateFilter filter({"shop@store::Stock"});
  EXPECT_EQ(filter.machines(model), (std::vector<std::string>{"shop@store::Stock::state"}));
  EXPECT_TRUE(filter.select(model).count("shop@store::Stock::state"));
  EXPECT_TRUE(ds::query::StateFilter({"shop@till::Till"}).machines(model).empty());  // no machine, nothing taken
}

class TestRequirementsFilter : public Shop {};

TEST_F(TestRequirementsFilter, ItTakesItsRootsWhatRefinesThemAndAsAskedWhatRealisesAndVerifiesThem) {
  const Ids all = ds::query::RequirementsFilter({"REQ-00001-1"}, true, true).select(model);
  EXPECT_TRUE(all.count("REQ-00001-1") && all.count("REQ-00002-1"));
  EXPECT_TRUE(all.count("shop@till::Till"));
  EXPECT_TRUE(all.count("VER-00001-1"));
  const Ids bare = ds::query::RequirementsFilter({"REQ-00001-1"}, false, false).select(model);
  EXPECT_EQ(bare, (Ids{"REQ-00001-1", "REQ-00002-1"}));
}

TEST(TestRequirementsFilterUseCase, AUseCaseThatAnswersToARequirementIsDrawnUnderIt) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  const auto file = parser.parseText("shop.ds",
                                     "package shop {\n"
                                     "  requirement SREQ-00001-1: The shop shall sell an item\n"
                                     "  usecase UC-00001-1 buy(item: string) -> int {\n"
                                     "    title: Buy an item\n"
                                     "    @requirement SREQ-00001-1\n"
                                     "    outcome default: the order\n"
                                     "  }\n"
                                     "}\n");
  ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
  binder.bind(file);
  const auto steps = ds::check::steps();
  ds::check::Checker(model, metamodel, steps).run(binder.notes());
  EXPECT_TRUE(ds::query::RequirementsFilter({"shop@*"}, true, true).select(model).count("UC-00001-1"));
  EXPECT_FALSE(ds::query::RequirementsFilter({"shop@*"}, false, true).select(model).count("UC-00001-1"));  // design hidden
}

class TestInPattern : public Shop {};

TEST_F(TestInPattern, AnIdentityAPrefixAndAModuleEachTakeWhatTheyName) {
  const ds::model::Entity& item = *model.find("shop@store::Item");
  const ds::model::Entity& till = *model.find("shop@till::Till");
  EXPECT_TRUE(ds::query::inPattern(model, "shop@store::Item", item));  // its identity
  EXPECT_TRUE(ds::query::inPattern(model, "shop@store::*", item));     // a prefix over the scope
  EXPECT_TRUE(ds::query::inPattern(model, "shop@store", item));        // the module, named as such
  EXPECT_FALSE(ds::query::inPattern(model, "shop@store::*", till));
  EXPECT_FALSE(ds::query::inPattern(model, "shop@store", till));
  EXPECT_FALSE(ds::query::inPattern(model, "shop@store::Stock", item));  // another identity
}

class TestDrawnFrom : public Shop {};

TEST_F(TestDrawnFrom, ABoxIsItselfAMemberItsClassAndABehaviourItsSubject) {
  EXPECT_EQ(ds::query::drawnFrom(model, *model.find("shop@till::Till")), "shop@till::Till");
  EXPECT_EQ(ds::query::drawnFrom(model, *model.find("shop@till")), "shop@till");
  EXPECT_EQ(ds::query::drawnFrom(model, *model.find("shop@till::Till::stock")), "shop@till::Till");
  EXPECT_EQ(ds::query::drawnFrom(model, *model.find("shop@till::Till::sell::sequence")), "shop@till::Till");
  EXPECT_EQ(ds::query::drawnFrom(model, *model.find("shop@store::Stock::state")), "shop@store::Stock");
}

class TestQuery : public Shop {};

TEST_F(TestQuery, ANamedQuestionIsAnsweredByIdentityAndAnUnknownOneIsAnError) {
  const ds::query::Query query(model);
  bool till = false;
  for (const auto& a : query.ask("entities", "shop@till")) till |= a.id == "shop@till::Till";
  EXPECT_TRUE(till);
  const auto wrong = query.ask("colours", "");
  ASSERT_EQ(wrong.size(), 1u);
  EXPECT_EQ(wrong[0].kind, "error");
}

}  // namespace
