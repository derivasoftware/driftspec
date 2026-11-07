#include <gtest/gtest.h>

#include <algorithm>

#include "ds/parse/Parser.hpp"

namespace {

class TestParser : public ::testing::Test {};

TEST_F(TestParser, ParsesAWholeWorkspaceWithoutSyntaxErrors) {
  ds::parse::Parser parser;
  const auto files = parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/diag");
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
                                     "      @uses b.c\n"
                                     "    }\n"
                                     "  }\n"
                                     "}\n");
  ASSERT_TRUE(file.hasSyntaxErrors);
  EXPECT_EQ(file.errorLine, 5);  // the dot a qualified name does not take, not the top of the file
  EXPECT_GT(file.errorColumn, 1);
  EXPECT_FALSE(file.errorDetail.empty());
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
