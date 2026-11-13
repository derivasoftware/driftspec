#include <gtest/gtest.h>

#include <set>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
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
  ds::model::Model model;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
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
    ds::check::Checker(model, steps).run(binder);
  }
};

class TestClassFilter : public Shop {};

TEST_F(TestClassFilter, ItsBoxesAreThePatternsLessTheExclusionsAndTheModulesAroundThemAreAdded) {
  const ds::query::ClassFilter filter({"shop@store::*"}, {"shop@store::Ledger"}, 0);
  EXPECT_EQ(filter.boxes(model), (Ids{"shop@store::Item", "shop@store::Stock"}));
  EXPECT_EQ(filter.select(model), (Ids{"shop@store", "shop@store::Item", "shop@store::Stock"}));
}

TEST_F(TestClassFilter, AClosedBoxStandsForWhatItHoldsUnlessItIsOpened) {
  const Ids containers{"shop@store", "shop@till"};
  const Ids opened = ds::query::ClassFilter::opened(containers, {"shop@till"});
  EXPECT_EQ(opened, (Ids{"shop", "shop@till"}));  // the box opened and every box on the way to it
  EXPECT_EQ(ds::query::ClassFilter::standsFor("shop@store::Stock", containers, opened), "shop@store");
  EXPECT_EQ(ds::query::ClassFilter::standsFor("shop@till::Till", containers, opened), "shop@till::Till");
}

class TestSequenceFilter : public Shop {};

TEST_F(TestSequenceFilter, ItTakesTheBehavioursItsPatternsNameAndWhoTakesPart) {
  const ds::query::SequenceFilter filter({"shop@till::Till::sell"}, 1, false);
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
