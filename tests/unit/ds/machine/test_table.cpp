#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/machine/Table.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A machine as a table (DS-251): its events down, its states across, in each cell where the event takes the
/// state, and over it what the behaviours written do.
class TestTable : public ::testing::Test {
 protected:
  /// The table of the order of a module `m` of a package `p`, after the text given.
  ds::machine::Table table(const std::string& behaviours = "") {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("a.ds", std::string("package p {\n  module m {\n") + kShop + behaviours + "  }\n}\n"));
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
    return ds::machine::Table(model, *model.find("p@m::Order::state"));
  }

  /// One row as it reads: the event, then `to` under each state, `-` where the machine has no jump.
  static std::string read(const ds::machine::Row& row) {
    std::string out = row.event + ":";
    for (const auto& cell : row.cells) out += " " + (cell.to.empty() ? std::string("-") : cell.to);
    return out;
  }

  static constexpr const char* kShop =
      "    class Order {\n"
      "      + pay() -> none\n"
      "      + ship() -> none\n"
      "      + cancel() -> none\n"
      "    }\n"
      "    state Order {\n"
      "      initial Open\n"
      "      Open -> Paid on pay [enough]\n"
      "      Open -> Held on pay\n"
      "      Paid -> Shipped on ship\n"
      "      Open -> Cancelled on cancel\n"
      "      Paid -> Cancelled on cancel\n"
      "    }\n"
      "    class Desk {\n"
      "      - owns order: Order\n"
      "      + run() -> none\n"
      "      + rush() -> none\n"
      "    }\n";

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

TEST_F(TestTable, ItsStatesGoAcrossAndItsEventsDownAsTheMachineWritesThem) {
  const auto m = table();
  EXPECT_EQ(m.subject(), "Order");
  EXPECT_EQ(m.states(), (std::vector<std::string>{"Open", "Paid", "Held", "Shipped", "Cancelled"}));
  ASSERT_EQ(m.rows().size(), 3u);
  EXPECT_EQ(m.rows()[0].event, "pay");
  EXPECT_EQ(m.rows()[1].event, "ship");
  EXPECT_EQ(m.rows()[2].event, "cancel");
  for (const auto& row : m.rows()) EXPECT_EQ(row.cells.size(), 5u) << row.event;  // a cell under every state
}

TEST_F(TestTable, ACellSaysWhereTheEventTakesTheStateWithItsGuardAndEveryJumpWhenThereAreSeveral) {
  const auto m = table();
  EXPECT_EQ(read(m.rows()[0]), "pay: Paid [enough] / Held - - - -");
  EXPECT_EQ(read(m.rows()[1]), "ship: - Shipped - - -");
  EXPECT_EQ(read(m.rows()[2]), "cancel: Cancelled Cancelled - - -");
}

TEST_F(TestTable, ARowSaysTheBehavioursThatCallItsEventEachOnce) {
  const auto m = table(
      "    sequence Desk::run {\n"
      "      self -> order.ship()\n"
      "    }\n"
      "    sequence Desk::rush {\n"
      "      self -> order.ship()\n"
      "      opt late {\n"
      "        self -> order.ship()\n"
      "      }\n"
      "    }\n");
  EXPECT_TRUE(m.rows()[0].callers.empty());  // nobody pays
  EXPECT_EQ(m.rows()[1].callers, (std::vector<std::string>{"Desk::run", "Desk::rush"}));
}

TEST_F(TestTable, ACallAgainstTheMachineIsSaidInEveryCellItsReceiverCanBeInThen) {
  const auto m = table(
      "    sequence Desk::rush {\n"
      "      alt paid {\n"
      "        self -> order.ship()\n"
      "      } else {\n"
      "        self -> order.cancel()\n"
      "      }\n"
      "      self -> order.cancel()\n"  // the order is Shipped or Cancelled: no jump on cancel from either
      "    }\n");
  const auto& cancel = m.rows()[2];
  EXPECT_TRUE(cancel.cells[0].against.empty());                                  // Open: the machine has a jump
  EXPECT_TRUE(cancel.cells[2].against.empty());                                  // Held: the order cannot be there then
  EXPECT_EQ(cancel.cells[3].against, (std::vector<std::string>{"Desk::rush"}));  // Shipped
  EXPECT_EQ(cancel.cells[4].against, (std::vector<std::string>{"Desk::rush"}));  // Cancelled
  EXPECT_TRUE(cancel.cells[3].to.empty());
}

}  // namespace
