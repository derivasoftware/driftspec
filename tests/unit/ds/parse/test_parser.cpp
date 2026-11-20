#include <gtest/gtest.h>

#include <algorithm>

#include "ds/parse/Parser.hpp"

namespace {

class TestParser : public ::testing::Test {};

TEST_F(TestParser, ParsesAWholeWorkspaceWithoutSyntaxErrors) {
  ds::parse::Parser parser;
  const auto files = parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/diag");
  ASSERT_EQ(files.size(), 13u);
  for (const auto& f : files) EXPECT_FALSE(f.hasSyntaxErrors) << f.path;
}

TEST_F(TestParser, ReportsSyntaxErrors) {
  ds::parse::Parser parser;
  const auto file = parser.parseText("x.ds", "package ds { class {");
  EXPECT_TRUE(file.hasSyntaxErrors);
}

TEST_F(TestParser, ReportsWhereTheFileStopsParsing) {
  ds::parse::Parser parser;
  const auto file = parser.parseText("x.ds",
                                     "package a {\n"
                                     "  module m {\n"
                                     "    class A {\n"
                                     "      + x: int\n"
                                     "      @consumes b.c\n"
                                     "    }\n"
                                     "  }\n"
                                     "}\n");
  ASSERT_TRUE(file.hasSyntaxErrors);
  EXPECT_EQ(file.errorLine, 5);  // the dot a qualified name does not take, not the top of the file
  EXPECT_GT(file.errorColumn, 1);
  EXPECT_FALSE(file.errorDetail.empty());
}

TEST_F(TestParser, TheQuotedTextIsCutOnACodePointBoundary) {
  // DS-334: an arrow (three bytes) across the cut once left half a character, and the JSON of the
  // findings refused the message
  ds::parse::Parser parser;
  const auto file = parser.parseText("x.ds", "package a { }\n&c><le only knows it  \u2192 aggregation\n");
  ASSERT_TRUE(file.hasSyntaxErrors);
  const std::string& detail = file.errorDetail;
  ASSERT_NE(detail.find("..."), std::string::npos);  // the quote was cut
  for (std::size_t i = 0; i < detail.size();) {      // every lead byte has all its continuation bytes
    const auto lead = static_cast<unsigned char>(detail[i]);
    const std::size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
    ASSERT_GT(length, 0u) << "a continuation byte where a character starts, at " << i;
    for (std::size_t k = 1; k < length; ++k)
      ASSERT_EQ(static_cast<unsigned char>(detail[i + k]) & 0xC0, 0x80) << "a character cut at " << i;
    i += length;
  }
}

TEST_F(TestParser, AFileThatParsesHasNoErrorPlace) {
  ds::parse::Parser parser;
  const auto file = parser.parseText("x.ds", "package a { }\n");
  EXPECT_FALSE(file.hasSyntaxErrors);
  EXPECT_EQ(file.errorLine, 0);
}

TEST_F(TestParser, WhatTheGrammarTakesAtAPlaceFollowsTheTokenBeforeIt) {
  ds::parse::Parser parser;
  const auto inModule = parser.expectedAt("package p {\n  module m {\n    \n  }\n}\n", 2, 4);
  EXPECT_NE(std::find(inModule.begin(), inModule.end(), "class"), inModule.end());
  EXPECT_NE(std::find(inModule.begin(), inModule.end(), "@annotation"), inModule.end());
  const auto atTop = parser.expectedAt("", 0, 0);
  EXPECT_NE(std::find(atTop.begin(), atTop.end(), "package"), atTop.end());
}

}  // namespace
