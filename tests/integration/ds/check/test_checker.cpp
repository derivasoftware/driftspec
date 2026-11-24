#include <gtest/gtest.h>

#include <map>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

class TestChecker : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  std::map<std::string, int> loadExamples() {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/diag")) binder.bind(f);
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
    std::map<std::string, int> byCode;
    for (const auto& f : model.findings()) ++byCode[f.code];
    return byCode;
  }
};

TEST_F(TestChecker, UndeclaredReferencesBecomeExpectedNodesReportedOnce) {
  const auto byCode = loadExamples();
  // the subject of a behaviour nobody declares is a link that waits: an expected node, said once (R6)
  const ds::model::Entity* subject = model.find("ds@lsp::SendOnce");
  ASSERT_NE(subject, nullptr);
  EXPECT_TRUE(subject->expected);
  EXPECT_EQ(byCode.at("W-REF-UNDECLARED"), 1);
  // a base, a type or a participant nobody designs is text: no expected node, no finding (DS-18)
  for (const char* id : {"ds@lsp::AbstractClass", "ds@lsp::Vector", "ds@lsp::LspServer"}) EXPECT_EQ(model.find(id), nullptr) << id;
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
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("t.ds", "package p { module a { class X { } module b { class Y : X { } } } }"));
  const auto checkSteps = ds::check::Steps::of();
  ds::check::Checker checker(model, metamodel, checkSteps);
  checker.run(binder.notes());
  EXPECT_EQ(checker.resolver().resolve("X", "p@a::b"), "p@a::X");
  EXPECT_EQ(model.findings().size(), 0u);
}

}  // namespace
