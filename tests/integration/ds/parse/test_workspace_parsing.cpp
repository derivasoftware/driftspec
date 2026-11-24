#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ds/api/Api.hpp"
#include "ds/check/Report.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A specification is a directory of text files read as one (REQ-00008-1, REQ-00051-1, CST-00007-1):
/// the grammar reads every file of an example, and a file it cannot read is reported where it stops.
class TestWorkspaceParsing : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::parse::Parser parser;

  static std::vector<ds::model::Finding> syntax(const ds::model::Model& model) {
    std::vector<ds::model::Finding> out;
    for (const auto& f : model.findings())
      if (f.code == "E-SYNTAX") out.push_back(f);
    return out;
  }
};

TEST_F(TestWorkspaceParsing, AWholeWorkspaceIsReadWithoutSyntaxErrors) {
  const auto files = parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/diag");
  ASSERT_EQ(files.size(), 13u);
  for (const auto& f : files) EXPECT_FALSE(f.hasSyntaxErrors) << f.path;
  const ds::model::Model model = ds::api::Api(metamodel).load(files);
  EXPECT_TRUE(syntax(model).empty());
  EXPECT_GT(ds::check::Report::of(model, metamodel).entities, 13);  // every file gave what it declares
}

TEST_F(TestWorkspaceParsing, TheFilesOfADirectoryAreReadAsOneInAnyOrder) {
  auto files = parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/diag");
  const ds::check::Report forwards = ds::check::Report::of(ds::api::Api(metamodel).load(files), metamodel);
  std::reverse(files.begin(), files.end());
  const ds::check::Report backwards = ds::check::Report::of(ds::api::Api(metamodel).load(files), metamodel);
  EXPECT_EQ(forwards.entities, backwards.entities);
  EXPECT_EQ(forwards.expected, backwards.expected);
  EXPECT_EQ(forwards.relations, backwards.relations);
  EXPECT_EQ(forwards.findings.size(), backwards.findings.size());
}

TEST_F(TestWorkspaceParsing, ASyntaxErrorIsReportedWhereTheFileStopsParsing) {
  const ds::model::Model model = ds::api::Api(metamodel).load({parser.parseText("x.ds",
                                                                                "package a {\n"
                                                                                "  module m {\n"
                                                                                "    class A {\n"
                                                                                "      + x: int\n"
                                                                                "      @consumes b.c\n"
                                                                                "    }\n"
                                                                                "  }\n"
                                                                                "}\n")});
  const auto errors = syntax(model);
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_EQ(errors[0].severity, ds::model::Severity::error);
  EXPECT_EQ(errors[0].location.file, "x.ds");
  EXPECT_EQ(errors[0].location.line, 5);  // the dot a qualified name does not take, not the top of the file
  EXPECT_GT(errors[0].location.column, 1);
}

TEST_F(TestWorkspaceParsing, AFileThatParsesReportsNoSyntaxErrorBesideOneThatDoesNot) {
  const ds::model::Model model =
      ds::api::Api(metamodel).load({parser.parseText("good.ds", "package a {\n  module m {\n    class A { }\n  }\n}\n"),
                                    parser.parseText("bad.ds", "package b { class {")});
  const auto errors = syntax(model);
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_EQ(errors[0].location.file, "bad.ds");
  EXPECT_NE(model.find("a@m::A"), nullptr);  // the file that parses is read all the same
}

}  // namespace
