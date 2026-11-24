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

/// A specification is a directory of text files read as one (REQ-00008-2, REQ-00051-1, CST-00007-1):
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

TEST_F(TestWorkspaceParsing, AFileThatIsNotUtf8IsReportedOnceAtItsFirstSuchByteAndTheOthersLoad) {
  // DS-411: a Latin-1 é, the byte 0xE9, in a title; the file is reported there and read no further
  const ds::model::Model model = ds::api::Api(metamodel).load(
      {parser.parseText("good.ds", "package a {\n  module m {\n    class A { }\n  }\n}\n"),
       parser.parseText("latin1.ds", "package b {\n  requirement REQ-00001-1 {\n    status: draft\n    title: caf\xe9 latin1\n  }\n}\n")});
  std::vector<ds::model::Finding> encoding;
  for (const auto& f : model.findings())
    if (f.code == "E-ENCODING") encoding.push_back(f);
  ASSERT_EQ(encoding.size(), 1u);
  EXPECT_EQ(encoding[0].severity, ds::model::Severity::error);
  EXPECT_EQ(encoding[0].location.file, "latin1.ds");
  EXPECT_EQ(encoding[0].location.line, 4);
  EXPECT_EQ(encoding[0].location.column, 15);
  EXPECT_NE(encoding[0].message.find("byte 0xE9"), std::string::npos) << encoding[0].message;
  EXPECT_TRUE(syntax(model).empty());             // it is not read as a syntax error
  EXPECT_EQ(model.find("REQ-00001-1"), nullptr);  // nothing of it reaches the model
  EXPECT_NE(model.find("a@m::A"), nullptr);       // the file that is UTF-8 is read all the same
}

TEST_F(TestWorkspaceParsing, EveryFormUtf8RefusesIsReportedAtItsByteAndEveryWellFormedOneIsRead) {
  // DS-411: UTF-8 as the Unicode standard writes it, in a title written after "    title: x" (column 13 on line 4)
  const auto loaded = [this](const std::string& bytes) {
    const std::string text = "package p {\n  requirement REQ-00001-1 {\n    status: draft\n    title: x" + bytes + "\n  }\n}\n";
    return ds::api::Api(metamodel).load({parser.parseText("t.ds", text)});
  };
  for (const std::string& written :
       std::vector<std::string>{"\xc3\xa9", "\xe2\x86\x92", "\xf0\x9f\x98\x80", "\xed\x9f\xbf", "\xf4\x8f\xbf\xbf"}) {
    const ds::model::Model model = loaded(written);
    EXPECT_TRUE(model.findings().empty()) << written;
    ASSERT_NE(model.find("REQ-00001-1"), nullptr) << written;
    EXPECT_EQ(model.find("REQ-00001-1")->field("title"), "x" + written);
  }
  // a character cut short, an overlong form, a surrogate, a code point above U+10FFFF, a byte that leads nothing and a
  // continuation byte nothing leads: each is E-ENCODING at its first byte, after one well-formed character of two bytes
  for (const std::string& refused :
       std::vector<std::string>{"\xe9 ", "\xc0\x80", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xff", "\x80"}) {
    const ds::model::Model model = loaded("\xc3\xa9" + refused);
    ASSERT_EQ(model.findings().size(), 1u) << refused;
    const auto& f = model.findings()[0];
    EXPECT_EQ(f.code, "E-ENCODING");
    EXPECT_EQ(f.location.line, 4);
    EXPECT_EQ(f.location.column, 15);  // the é before it is two bytes, so two columns
    EXPECT_EQ(model.find("REQ-00001-1"), nullptr);
  }
}

}  // namespace
