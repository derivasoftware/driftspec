#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/machine/Calls.hpp"
#include "ds/machine/Jump.hpp"
#include "ds/machine/Machine.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// One walk of the behaviours written, for the calls of one machine's events (DS-250).
class TestCallsWalk : public ::testing::Test {
 protected:
  /// The model of a module `m` of a package `p` that holds the text given.
  void load(const std::string& body) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("a.ds", "package p {\n  module m {\n" + body + "  }\n}\n"));
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
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

TEST_F(TestCallsWalk, ACallOfAnEventIsFoundWithTheStatesItsReceiverMayBeIn) {
  load(std::string(kOrder) +
       "    class Desk {\n"
       "      - owns order: Order\n"
       "      + run() -> none\n"
       "    }\n"
       "    sequence Desk::run {\n"
       "      self -> order.ship()\n"
       "    }\n");
  const ds::model::Entity& machine = *model.find("p@m::Order::state");
  const std::vector<ds::machine::Jump> jumps = ds::machine::Jump::of(machine);
  const std::vector<std::string> states = ds::machine::Machine(model, machine).states();
  const std::string subject = "p@m::Order";
  const ds::machine::Calls walk(model, subject, jumps, states);
  std::vector<ds::machine::Call> found;
  walk.of(*model.find("p@m::Desk::run::sequence"), found);
  ASSERT_EQ(found.size(), 1u);
  EXPECT_EQ(found[0].behaviour, "Desk::run");
  EXPECT_EQ(found[0].event, "ship");
  EXPECT_EQ(found[0].from, (std::vector<std::string>{"Open", "Paid", "Shipped"}));  // the first call: any state
  EXPECT_TRUE(found[0].fits);
}

}  // namespace
