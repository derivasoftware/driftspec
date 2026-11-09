#include <gtest/gtest.h>

#include <map>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

class TestChecker : public ::testing::Test {
 protected:
  ds::model::Model model;

  std::map<std::string, int> loadExamples() {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/diag")) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
    std::map<std::string, int> byCode;
    for (const auto& f : model.findings()) ++byCode[f.code];
    return byCode;
  }
};

TEST_F(TestChecker, UndeclaredReferencesBecomeExpectedNodesReportedOnce) {
  const auto byCode = loadExamples();
  const ds::model::Entity* base = model.find("ds@lsp::AbstractClass");
  ASSERT_NE(base, nullptr);
  EXPECT_TRUE(base->expected);
  EXPECT_GE(byCode.at("W-REF-UNDECLARED"), 3);  // AbstractClass, Vector, LspServer (and boundary return types)
}

TEST_F(TestChecker, AStateEventMustBeAnOperationOfItsEntity) {
  const auto byCode = loadExamples();
  EXPECT_EQ(byCode.at("W-EVENT-UNDECLARED"), 1);  // `open` is not an operation of lsp::LspClient
}

TEST_F(TestChecker, MessagesToExpectedTypesAreNotReportedTwice) {
  const auto byCode = loadExamples();
  EXPECT_EQ(byCode.count("W-MSG-UNDECLARED"), 0u);  // LspServer::receive waits for LspServer
}

TEST_F(TestChecker, ResolvesInnermostScopeFirst) {
  ds::parse::Parser parser;
  ds::bind::Binder binder(model);
  binder.bind(parser.parseText("t.ds", "package p { module a { class X { } module b { class Y : X { } } } }"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker checker(model, checkSteps);
  checker.run(binder);
  EXPECT_EQ(checker.resolver().resolve("X", "p@a::b"), "p@a::X");
  EXPECT_EQ(model.findings().size(), 0u);
}

}  // namespace
