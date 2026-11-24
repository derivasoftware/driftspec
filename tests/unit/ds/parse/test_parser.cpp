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

TEST_F(TestParser, AFileThatIsNotUtf8IsMarkedAtTheLineAndColumnOfItsFirstSuchByte) {
  // DS-411: a Latin-1 é once checked clean and failed every JSON output; it is marked where it is, and read no further
  ds::parse::Parser parser;
  const auto file = parser.parseText("x.ds", "package p {\n  requirement REQ-00001-1 {\n    title: caf\xe9 latin1\n  }\n}\n");
  EXPECT_TRUE(file.hasEncodingError);
  EXPECT_FALSE(file.hasSyntaxErrors);
  EXPECT_EQ(file.errorLine, 3);
  EXPECT_EQ(file.errorColumn, 15);  // "    title: caf" is 14 bytes before it
  EXPECT_EQ(file.errorDetail, "byte 0xE9 is not part of a character");
  EXPECT_TRUE(file.text.empty());  // nothing of it reaches the model
  ASSERT_TRUE(file.tree);
  EXPECT_EQ(ts_node_child_count(ts_tree_root_node(file.tree.get())), 0u);

  // the column counts bytes, as a syntax error's does: a character of two bytes before it on its line is two columns
  const auto wide = parser.parseText("w.ds", "// \xc3\xa9\xff\n");
  EXPECT_EQ(wide.errorLine, 1);
  EXPECT_EQ(wide.errorColumn, 6);
  EXPECT_EQ(wide.errorDetail, "byte 0xFF is not part of a character");
  // a file written on Windows: its carriage returns end no line and move no column
  const auto crlf = parser.parseText("c.ds", "package p {\r\n  \x80\r\n}\r\n");
  EXPECT_EQ(crlf.errorLine, 2);
  EXPECT_EQ(crlf.errorColumn, 3);
  // the first byte of the text
  const auto first = parser.parseText("f.ds", "\xc0\x80package p { }\n");
  EXPECT_EQ(first.errorLine, 1);
  EXPECT_EQ(first.errorColumn, 1);
  // a file that is UTF-8 is not marked
  EXPECT_FALSE(parser.parseText("u.ds", "package p { }  // caf\xc3\xa9\n").hasEncodingError);
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
