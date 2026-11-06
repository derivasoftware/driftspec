#include <gtest/gtest.h>

#include <string>

#include "ds/parse/Parser.hpp"

namespace {

/// A syntax error says where it sits and what the place takes (REQ-00080-1), so a newcomer can
/// correct it without guessing the grammar.
class TestSyntaxErrors : public ::testing::Test {
 protected:
  static std::string detailOf(const std::string& text) {
    ds::parse::Parser parser;
    const auto file = parser.parseText("x.ds", text);
    EXPECT_TRUE(file.hasSyntaxErrors);
    return file.errorDetail;
  }
};

TEST_F(TestSyntaxErrors, AWordTheGrammarDoesNotTakeNamesWhatTheModuleTakes) {
  const std::string detail = detailOf("package p {\n  module m {\n    clas A { }\n  }\n}\n");
  EXPECT_NE(detail.find("unexpected `clas A {` in a module"), std::string::npos) << detail;
  EXPECT_NE(detail.find("it takes class, constraint, dataclass"), std::string::npos) << detail;
}

TEST_F(TestSyntaxErrors, ABlockClosedEarlyNamesWhatItWouldHaveTaken) {
  // an annotation a module does not take: the parser closes the module, and says what it takes
  const std::string detail = detailOf("package p {\n  module m {\n    @requirement REQ-1\n    class A { }\n  }\n}\n");
  EXPECT_NE(detail.find("missing `}` in a module, which takes"), std::string::npos) << detail;
  EXPECT_EQ(detail.find("@annotation"), std::string::npos) << detail;
}

}  // namespace
