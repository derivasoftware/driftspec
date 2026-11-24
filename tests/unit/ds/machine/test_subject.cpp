#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/machine/Subject.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The class a machine is of: its attributes and the values each may take (DS-249).
class TestSubject : public ::testing::Test {
 protected:
  /// The model of a module `m` of a package `p` that holds the text given.
  void load(const std::string& body) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("a.ds", "package p {\n  module m {\n" + body + "  }\n}\n"));
    const auto checkSteps = ds::check::StepFactory::of();
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

TEST_F(TestSubject, AnAttributeOfAnEnumTakesItsValuesAndABooleanTrueOrFalse) {
  load(kOrder);
  const ds::machine::Subject order(model, "p@m::Order");
  ASSERT_NE(order.attribute("status"), nullptr);
  EXPECT_EQ(order.attribute("nothing"), nullptr);
  EXPECT_EQ(order.values("status"), (std::vector<std::string>{"Open", "Paid", "Shipped"}));
  EXPECT_EQ(order.values("cancelled"), (std::vector<std::string>{"false", "true"}));
  EXPECT_EQ(order.values("ready"), (std::vector<std::string>{"false", "true"}));  // a condition with a name
}

}  // namespace
