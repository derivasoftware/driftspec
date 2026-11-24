#include <gtest/gtest.h>

#include "ds/parse/Parser.hpp"

namespace {

/// A file written on Windows: its CRLF line ends are read as LF, so a text block keeps no carriage return
/// and a node still spans what is written on its line (DS-315).
TEST(TestParsedFile, AFileWrittenOnWindowsReadsAsTheSameText) {
  ds::parse::Parser parser;
  const auto file = parser.parseText("w.ds", "package shop {\r\n  module till {\r\n  }\r\n}\r\n");
  ASSERT_FALSE(file.hasSyntaxErrors);
  EXPECT_EQ(file.text, "package shop {\n  module till {\n  }\n}\n");
  EXPECT_EQ(file.text.find('\r'), std::string::npos);
}

}  // namespace
