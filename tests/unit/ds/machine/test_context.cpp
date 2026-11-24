#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/machine/Context.hpp"
#include "ds/machine/StateRule.hpp"
#include "ds/machine/Subject.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// What each name of a context may be worth, as the rules of a machine ask it (DS-249).
class TestContext : public ::testing::Test {
 protected:
  /// The model of a module `m` of a package `p` that holds the text given.
  void load(const std::string& body) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("a.ds", "package p {\n  module m {\n" + body + "  }\n}\n"));
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

const char* const kOrder =
    "    enum Status { Open, Paid, Shipped }\n"
    "    class Order {\n"
    "      - status: Status = Open\n"
    "      - cancelled: bool = false\n"
    "      + pay() -> none\n"
    "      + ship() -> none\n"
    "    }\n"
    "    state Order {\n"
    "      Open if status = Open\n"
    "      Paid if status = Paid\n"
    "      Open -> Paid on pay [enough]\n"
    "      Paid -> Shipped on ship\n"
    "    }\n";

TEST_F(TestContext, TwoRulesThatAskDifferentValuesCannotHoldTogether) {
  load(kOrder);
  const ds::machine::Subject order(model, "p@m::Order");
  const std::vector<ds::machine::StateRule> rules = ds::machine::StateRule::of(*model.find("p@m::Order::state"));
  ASSERT_EQ(rules.size(), 2u);
  ds::machine::Context open, paid, both;
  EXPECT_TRUE(ds::machine::Context::allowed(order, rules[0], &open));
  EXPECT_TRUE(ds::machine::Context::allowed(order, rules[1], &paid));
  EXPECT_EQ(open.told(), "status = Open");
  EXPECT_FALSE(open.together(paid, &both));
  EXPECT_TRUE(open.together(open, &both));
}

TEST_F(TestContext, ContextsKnownInFullAreWithinARuleWhenEveryNameIsOneItAllows) {
  load(kOrder);
  const ds::machine::Subject order(model, "p@m::Order");
  const std::vector<ds::machine::StateRule> rules = ds::machine::StateRule::of(*model.find("p@m::Order::state"));
  ds::machine::Context open;
  ASSERT_TRUE(ds::machine::Context::allowed(order, rules[0], &open));
  ds::machine::Context defaults;
  defaults.set("status", {"Open"});
  EXPECT_TRUE(defaults.within(open));
  defaults.set("status", {"Paid"});
  EXPECT_FALSE(defaults.within(open));
}

}  // namespace
