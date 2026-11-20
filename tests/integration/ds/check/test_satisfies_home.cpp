#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A piece that answers to a requirement written in a box that does not hold it: the requirement is written at
/// one place and realised outside it (DDEV-1470).
class TestSatisfiesHome : public ::testing::Test {
 protected:
  ds::model::Model model;

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    binder.bind(parser.parseText("a.ds", text));
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  std::vector<std::string> messages(const std::string& code) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == code) out.push_back(f.message);
    return out;
  }
};

TEST_F(TestSatisfiesHome, APieceOutsideTheHomeIsReportedWithTheBoxThatHoldsThemAll) {
  load(
      "package shop {\n"
      "  package core {\n"
      "    module render {\n"
      "      requirement REQ-00001-1 {\n"
      "        title: The renderer shall draw an order\n"
      "      }\n"
      "      class Svg {\n"
      "        @requirement REQ-00001-1\n"
      "      }\n"
      "    }\n"
      "    module format {\n"
      "      class Page {\n"
      "        @requirement REQ-00001-1\n"
      "        + write() -> void\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}\n");
  const auto outside = messages("W-SATISFIES-OUTSIDE-HOME");
  ASSERT_EQ(outside.size(), 1u);
  EXPECT_EQ(outside[0].rfind("shop@core@format::Page satisfies REQ-00001-1, whose home shop@core@render", 0), 0u);
  EXPECT_NE(outside[0].find("shop@core holds it and every piece that realises it"), std::string::npos);
}

TEST_F(TestSatisfiesHome, APieceInsideTheHomeARequirementAtTheRootOrOneAnotherSystemAdoptsIsNoFinding) {
  load(
      "requirement REQ-00002-1 {\n"
      "  title: The shop shall take an order\n"
      "}\n"
      "package library {\n"
      "  requirement LIB-00001-1 {\n"
      "    title: Modules shall not depend on each other both ways\n"
      "  }\n"
      "}\n"
      "package shop {\n"
      "  @requirement LIB-00001-1\n"
      "  requirement REQ-00003-1 {\n"
      "    title: The shop shall keep its orders\n"
      "  }\n"
      "  module store {\n"
      "    class Cart {\n"
      "      @requirement REQ-00002-1\n"
      "      @requirement REQ-00003-1\n"
      "      + total() -> int {\n"
      "        @requirement REQ-00003-1\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_TRUE(messages("W-SATISFIES-OUTSIDE-HOME").empty());
}

}  // namespace
