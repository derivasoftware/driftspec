#include <gtest/gtest.h>

#include "ds/parse/Parser.hpp"

namespace {

/// One file read: the text a node of its tree spans is the text written there.
TEST(TestParsedFile, ANodeSpansTheTextWrittenThere) {
  ds::parse::Parser parser;
  const auto file = parser.parseText("a.ds", "package shop {\n  module till {\n  }\n}\n");
  ASSERT_FALSE(file.hasSyntaxErrors);
  const TSNode root = ts_tree_root_node(file.tree.get());
  EXPECT_EQ(file.textOf(root), file.text);
  const TSNode package = ts_node_named_child(root, 0);
  EXPECT_EQ(file.textOf(package).rfind("package shop {", 0), 0u);
}

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
