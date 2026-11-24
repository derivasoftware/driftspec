#include <gtest/gtest.h>

#include <string>

#include "ds/parse/Parser.hpp"
#include "ds/parse/SyntaxError.hpp"

namespace {

using ds::parse::ParsedFile;
using ds::parse::SyntaxError;

/// The first place a tree does not parse (DS-374): placed in the file, with what is wrong there and where it sits.
class TestSyntaxError : public ::testing::Test {
 protected:
  ds::parse::Parser parser;

  /// `text` parsed, then described again by a SyntaxError into a copy whose error is cleared.
  struct Placed {
    bool found;
    ParsedFile file;
  };
  Placed place(const std::string& text) {
    ParsedFile file = parser.parseText("x.ds", text);
    file.errorLine = file.errorColumn = 0;
    file.errorDetail.clear();
    const bool found = SyntaxError(file.text).place(ts_tree_root_node(file.tree.get()), file);
    return {found, file};
  }
};

TEST_F(TestSyntaxError, ATreeThatParsesHasNoErrorToPlace) {
  const Placed placed = place("package a {\n  module m {\n  }\n}\n");
  EXPECT_FALSE(placed.found);
  EXPECT_EQ(placed.file.errorLine, 0);
  EXPECT_EQ(placed.file.errorDetail, "");
}

TEST_F(TestSyntaxError, AWordTheGrammarDoesNotTakeIsQuotedWhereItIsWithWhatTheDeclarationTakes) {
  const Placed placed = place("package a {\n  module m {\n    bogus here\n  }\n}\n");
  ASSERT_TRUE(placed.found);
  EXPECT_EQ(placed.file.errorLine, 3);
  EXPECT_EQ(placed.file.errorDetail.rfind("unexpected `", 0), 0u) << placed.file.errorDetail;
  EXPECT_NE(placed.file.errorDetail.find(" in a module"), std::string::npos) << placed.file.errorDetail;
}

TEST_F(TestSyntaxError, ATokenTheParserHadToInventIsMissing) {
  const Placed placed = place("package a {\n  module m {\n  }\n");
  ASSERT_TRUE(placed.found);
  EXPECT_EQ(placed.file.errorLine, 3);
  EXPECT_EQ(placed.file.errorDetail.rfind("missing `}` in a package", 0), 0u) << placed.file.errorDetail;
}

TEST_F(TestSyntaxError, ItDescribesAFileAsTheParserDoes) {
  const std::string text = "package a {\n  module m {\n    class A {\n      + x int\n    }\n  }\n}\n";
  const ParsedFile parsed = parser.parseText("x.ds", text);
  const Placed placed = place(text);
  EXPECT_EQ(placed.file.errorLine, parsed.errorLine);
  EXPECT_EQ(placed.file.errorColumn, parsed.errorColumn);
  EXPECT_EQ(placed.file.errorDetail, parsed.errorDetail);
}

}  // namespace
