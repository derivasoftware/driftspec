#include <gtest/gtest.h>

#include "ds/axis/Cut.hpp"
#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Walk.hpp"

namespace {

/// The ways through the behaviours of one model (DS-185): Till::sell calls Stock::take, whose own
/// sequence calls Stock::count.
class TestWalker : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(
        parser.parseText("shop.ds",
                         "package shop {\n"
                         "  module till {\n"
                         "    class Stock {\n"
                         "      + take() -> none\n"
                         "      + count() -> int\n"
                         "    }\n"
                         "    class Till {\n"
                         "      - stock: Stock\n"
                         "      + sell() -> none\n"
                         "    }\n"
                         "    sequence Till::sell {\n"
                         "      self -> stock.take()\n"
                         "    }\n"
                         "    sequence Stock::take {\n"
                         "      -> count() -> n\n"
                         "    }\n"
                         "  }\n"
                         "}\n"
                         "diagram OnACut {\n  kind: sequence\n  include: shop@till::Till::sell\n  close: module\n}\n"
                         "diagram Folded {\n  kind: sequence\n  include: shop@till::Till::sell\n  fold: shop@till::Stock::take\n}\n"));
    const auto checkSteps = ds::check::StepFactory::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

TEST_F(TestWalker, TheStepsOfABehaviourSayWhereEachCallGoesOn) {
  const auto steps = ds::query::Walker(model).steps("shop@till::Till::sell::sequence", "");
  ASSERT_EQ(steps.size(), 1u);
  EXPECT_EQ(steps[0].next, "shop@till::Stock::take::sequence");  // the behaviour the call enters
}

TEST_F(TestWalker, AWalkGoesDownIntoWhatTheCallsEnterAsDeepAsAsked) {
  const ds::query::Walker walker(model);
  const auto shallow = walker.walk("shop@till::Till::sell::sequence", "", 0);
  const auto deep = walker.walk("shop@till::Till::sell::sequence", "", 1);
  EXPECT_EQ(shallow.size(), 1u);  // its own message only
  ASSERT_EQ(deep.size(), 2u);     // and the one of the behaviour it enters
  EXPECT_EQ(deep[1].depth, 2);
}

TEST_F(TestWalker, AWalkWithACutFollowsTheCallsTheCutSays) {
  const ds::axis::Cut onACut(model, metamodel, *model.find("diagram:OnACut"));
  EXPECT_EQ(ds::query::Walker(model).walk("shop@till::Till::sell::sequence", "", 0, &onACut).size(), 2u);  // take, and the count inside it
  const ds::axis::Cut folded(model, metamodel, *model.find("diagram:Folded"));
  EXPECT_EQ(ds::query::Walker(model).walk("shop@till::Till::sell::sequence", "", 0, &folded).size(), 1u);  // take is not followed
}

}  // namespace
