#include <gtest/gtest.h>

#include <map>

#include "driftspec/bind/Binder.hpp"
#include "driftspec/check/Checker.hpp"
#include "driftspec/model/Model.hpp"
#include "driftspec/parse/Parser.hpp"

namespace {

class TestChecker : public ::testing::Test {
 protected:
  driftspec::model::Model model;

  std::map<std::string, int> loadExamples() {
    driftspec::parse::Parser parser;
    driftspec::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/diag")) binder.bind(f);
    driftspec::check::Checker(model).run(binder);
    std::map<std::string, int> byCode;
    for (const auto& f : model.findings()) ++byCode[f.code];
    return byCode;
  }
};

TEST_F(TestChecker, UndeclaredReferencesBecomeExpectedNodesReportedOnce) {
  const auto byCode = loadExamples();
  const driftspec::model::Entity* base = model.find("lsp::AbstractClass");
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
  driftspec::parse::Parser parser;
  driftspec::bind::Binder binder(model);
  binder.bind(parser.parseText("t.ds", "package p { module a { class X { } module b { class Y : X { } } } }"));
  driftspec::check::Checker checker(model);
  checker.run(binder);
  EXPECT_EQ(checker.resolve("X", "a::b"), "a::X");
  EXPECT_EQ(model.findings().size(), 0u);
}

}  // namespace
