#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A module answers to its requirements as a package does (REQ-00017-2, DS-82): the metamodel
/// let a module require, and the grammar now lets one write it.
class TestModuleAnnotations : public ::testing::Test {};

TEST_F(TestModuleAnnotations, AModuleAnswersToTheRequirementItsBodyNames) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  const auto file = parser.parseText("m.ds",
                                     "package p {\n"
                                     "  requirement REQ-00001-1: The store shall keep the stock\n"
                                     "  module store {\n"
                                     "    @requirement REQ-00001-1\n"
                                     "    class Stock { }\n"
                                     "  }\n"
                                     "}\n");
  ASSERT_FALSE(file.hasSyntaxErrors);
  binder.bind(file);
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder);
  bool required = false;
  for (const auto& r : model.index().all()) required |= r.kind == "satisfies" && r.from == "p@store" && r.to == "REQ-00001-1";
  EXPECT_TRUE(required);
  EXPECT_TRUE(model.findings().empty()) << model.findings().front().code;
}

}  // namespace
