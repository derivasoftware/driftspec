#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"
#include "ds/query/Walk.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// Where a failure starts and where it is handled, over tests/corpus/failures (DS-187).
class TestFailures : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/failures")) {
      ASSERT_FALSE(f.hasSyntaxErrors) << f.path << ": " << f.errorDetail;
      binder.bind(f);
    }
    const auto steps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, steps).run(binder.notes());
  }

  /// What a path takes, a call by its operation, an ending by what it gives back or fails with.
  std::vector<std::string> path(const std::string& sequence, const std::string& scenario) const {
    std::vector<std::string> out;
    for (const auto& t : ds::query::Walker(model).steps(sequence, scenario)) {
      const ds::model::Entity& m = *model.find(t.message);
      if (!m.field("fails").empty())
        out.push_back("fail " + m.field("fails"));
      else if (m.field("return") == "true")
        out.push_back("return " + m.field("result"));
      else
        out.push_back(m.field("op"));
    }
    return out;
  }
};

TEST_F(TestFailures, TheExampleReportsNothingButTheFailuresItsSignaturesMisstate) {
  for (const auto& f : model.findings()) EXPECT_EQ(f.code.rfind("W-FAILURE-", 0), 0u) << f.code << " " << f.message;
}

TEST_F(TestFailures, APathThatFailsEndsThereWithTheFailuresName) {
  EXPECT_EQ(path("shelf@books::Parser::parse::sequence", "broken"), (std::vector<std::string>{"fail ParseError"}));
  EXPECT_EQ(path("shelf@books::Parser::parse::sequence", ""), (std::vector<std::string>{"return book"}));
}

TEST_F(TestFailures, AnAttemptRunsOnEveryPathAndAHandlerOnlyOnThePathThatNamesIt) {
  const std::string load = "shelf@books::Loader::load::sequence";
  EXPECT_EQ(path(load, ""), (std::vector<std::string>{"read", "parse", "return book"}));               // the default path does not fail
  EXPECT_EQ(path(load, "unreadable"), (std::vector<std::string>{"read", "parse", "fail LoadError"}));  // translated
  EXPECT_EQ(path(load, "busy"), (std::vector<std::string>{"read", "parse", "last", "return book"}));   // resolved
}

TEST_F(TestFailures, AHandlerKeepsTheFailureItHandles) {
  int handlers = 0;
  for (const auto& pair : model.entities()) {
    const ds::model::Entity& f = pair.second;
    if (f.kind != ds::model::Kind::Frame || f.field("kind") != "handler" || f.scope != "shelf@books::Loader::load::sequence") continue;
    ++handlers;
    EXPECT_TRUE(f.field("failure") == "IoError" || f.field("failure") == "Busy") << f.field("failure");
  }
  EXPECT_EQ(handlers, 2);
}

TEST_F(TestFailures, TheSequenceDiagramDrawsTheFailureAndTheHandlers) {
  const std::string ir = ds::render::IrExport(model, metamodel).render("diagram:SEQ_Load", false);
  EXPECT_NE(ir.find("fail LoadError"), std::string::npos);
  EXPECT_NE(ir.find("on IoError"), std::string::npos);
  EXPECT_NE(ir.find("on Busy"), std::string::npos);
}

/// What escapes each operation (DS-188): a failure goes up to every caller that does not handle it.
TEST_F(TestFailures, AFailureEscapesEveryCallerThatDoesNotHandleIt) {
  EXPECT_EQ(model.find("shelf@books::Parser::parse")->field("escapes"), "ParseError");
  EXPECT_EQ(model.find("shelf@books::Loader::load")->field("escapes"), "LoadError, ParseError");  // IoError is handled
  EXPECT_EQ(model.find("shelf@books::Loader::open")->field("escapes"), "LoadError, ParseError");
  EXPECT_EQ(model.find("shelf@books::Shelf::show")->field("escapes"), "");  // both handled
  EXPECT_EQ(model.find("shelf@books::Cache::last")->field("escapes"), "");  // no behaviour, no signature: taken not to fail
}

TEST_F(TestFailures, EachEscapeKeepsThePathItComesUpByAndTheScenarioItStartsIn) {
  EXPECT_EQ(model.find("shelf@books::Loader::open")->field("escape.ParseError"),
            "shelf@books::Loader::open -> shelf@books::Loader::load -> shelf@books::Parser::parse"
            " from shelf@books::Parser::parse::sequence/broken");
  EXPECT_EQ(model.find("shelf@books::Loader::load")->field("escape.LoadError"),
            "shelf@books::Loader::load from shelf@books::Loader::load::sequence/unreadable");  // a handler's own failure
}

TEST_F(TestFailures, TheQueryAnswersWhatEscapesAnOperationByWhereEachStarts) {
  const auto answers = ds::query::Query(model, metamodel).ask("failures", "shelf@books::Loader::open");
  ASSERT_EQ(answers.size(), 2u);
  EXPECT_EQ(answers[0].kind, "failure");
  EXPECT_EQ(answers[0].id, "shelf@books::Loader::load");
  EXPECT_EQ(answers[0].detail.rfind("LoadError: ", 0), 0u);
  EXPECT_EQ(answers[1].id, "shelf@books::Parser::parse");
  EXPECT_TRUE(ds::query::Query(model, metamodel).ask("failures", "shelf@books::Shelf::show").empty());
}

/// What a signature promises (DS-189): fails A, B or fails nothing, held to what escapes.
class TestFailurePromises : public TestFailures {
 protected:
  std::vector<std::string> said(const std::string& code, const std::string& about) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == code && f.message.rfind(about + " ", 0) == 0) out.push_back(f.message);
    return out;
  }
};

TEST_F(TestFailurePromises, WhatEscapesAndTheSignatureDoesNotSayIsAFinding) {
  const auto open = said("W-FAILURE-UNDECLARED", "shelf@books::Loader::open");
  ASSERT_EQ(open.size(), 1u);
  EXPECT_NE(open[0].find("lets ParseError escape"), std::string::npos);
  EXPECT_TRUE(said("W-FAILURE-UNDECLARED", "shelf@books::Loader::load").empty());  // it says both
  EXPECT_TRUE(said("W-FAILURE-UNDECLARED", "shelf@books::Shelf::show").empty());   // fails nothing, and nothing escapes
}

TEST_F(TestFailurePromises, WhatTheSignatureSaysAndNoPathFailsWithIsAFinding) {
  const auto parse = said("W-FAILURE-UNREACHED", "shelf@books::Parser::parse");
  ASSERT_EQ(parse.size(), 1u);
  EXPECT_NE(parse[0].find("Timeout"), std::string::npos);
}

TEST_F(TestFailurePromises, AnOperationWithNoBehaviourFailsAsItsSignatureSays) {
  EXPECT_EQ(model.find("shelf@books::Files::read")->field("escapes"), "Busy, IoError");
  EXPECT_TRUE(said("W-FAILURE-UNREACHED", "shelf@books::Files::read").empty());                   // nothing to hold it to
  EXPECT_EQ(model.find("shelf@books::Loader::load")->field("escapes"), "LoadError, ParseError");  // IoError handled
}

TEST_F(TestFailurePromises, ABoundarySaysWhatEscapesItOrNothingMay) {
  EXPECT_EQ(said("W-FAILURE-UNDECLARED", "shelf@library::open").size(), 2u);  // LoadError and ParseError, said by no one
  EXPECT_TRUE(said("W-FAILURE-UNDECLARED", "shelf@library::show").empty());   // nothing escapes it
}

}  // namespace
