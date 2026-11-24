#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A chain of refines or inherits that comes back to where it starts, and a requirement that refines one
/// written inside its own box, are contradictions between written things (DS-214).
class TestCycles : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("a.ds", text));
    const auto checkSteps = ds::check::StepFactory::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  std::vector<std::string> messages(const std::string& code) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == code) out.push_back(f.message);
    return out;
  }
};

TEST_F(TestCycles, RequirementsThatRefineEachOtherAreACycleAndTheUpwardOneIsSaid) {
  load(
      "package shop {\n"
      "  requirement SREQ-00001-1 {\n"
      "    title: The shop shall take an order\n"
      "    @refines REQ-00001-1\n"
      "  }\n"
      "  module store {\n"
      "    requirement REQ-00001-1 {\n"
      "      title: The cart shall total its items\n"
      "      @refines SREQ-00001-1\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_EQ(messages("E-REFINES-CYCLE").size(), 1u);
  const auto upward = messages("W-REFINES-UPWARD");
  ASSERT_EQ(upward.size(), 1u);
  EXPECT_EQ(upward[0].rfind("SREQ-00001-1", 0), 0u);  // the one of the system refines the one of the module
}

TEST_F(TestCycles, ClassesThatDeriveFromEachOtherAreACycleAndATreeIsNot) {
  load(
      "package shop {\n"
      "  module store {\n"
      "    class A : B {\n"
      "      + go() -> none\n"
      "    }\n"
      "    class B : A {\n"
      "      + stop() -> none\n"
      "    }\n"
      "    class Base {\n"
      "      + run() -> none\n"
      "    }\n"
      "    class Left : Base { }\n"
      "    class Right : Base { }\n"
      "  }\n"
      "}\n");
  const auto cycles = messages("E-INHERITANCE-CYCLE");
  ASSERT_EQ(cycles.size(), 1u);
  EXPECT_EQ(cycles[0].find("Base"), std::string::npos);
}

TEST_F(TestCycles, ARequirementThatRefinesOneOfTheSystemIsNoFinding) {
  load(
      "package shop {\n"
      "  requirement SREQ-00001-1: The shop shall take an order\n"
      "  module store {\n"
      "    requirement REQ-00001-1 {\n"
      "      title: The cart shall total its items\n"
      "      @refines SREQ-00001-1\n"
      "      requirement REQ-00002-1: The cart shall count its items\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_TRUE(messages("E-REFINES-CYCLE").empty());
  EXPECT_TRUE(messages("W-REFINES-UPWARD").empty());
}

}  // namespace
