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

}  // namespace
