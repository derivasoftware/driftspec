#include <gtest/gtest.h>

#include "ds/parse/Parser.hpp"
#include "ds/parse/TreeReader.hpp"

namespace {

using ds::parse::TreeReader;

/// The syntax tree of one parsed file, read (DS-354): what binding asks of a node, with no model.
class TestTreeReader : public ::testing::Test {
 protected:
  ds::parse::Parser parser;
  ds::parse::ParsedFile file = parser.parseText("a.ds", "package p {\n  module m {\n    class A : B, C {\n    }\n  }\n}\n");
  TreeReader reader{file};

  /// The first node of `type` under `node`, depth first.
  static TSNode find(TSNode node, const char* type) {
    if (TreeReader::is(node, type)) return node;
    for (uint32_t i = 0; i < ts_node_named_child_count(node); ++i) {
      const TSNode found = find(ts_node_named_child(node, i), type);
      if (!ts_node_is_null(found)) return found;
    }
    return TSNode{};
  }
};

TEST_F(TestTreeReader, ANodeIsKnownByItsTypeAndItsFieldsAreRead) {
  const TSNode cls = find(reader.root(), "class");
  ASSERT_FALSE(ts_node_is_null(cls));
  EXPECT_TRUE(TreeReader::is(cls, "class"));
  EXPECT_FALSE(TreeReader::is(cls, "module"));
  EXPECT_EQ(reader.fieldText(cls, "name"), "A");
  EXPECT_EQ(reader.fieldText(cls, "nobody"), "");  // a field the node lacks reads as nothing
  EXPECT_TRUE(ts_node_is_null(TreeReader::child(cls, "nobody")));
}

TEST_F(TestTreeReader, AFieldThatRepeatsGivesEveryChild) {
  const TSNode cls = find(reader.root(), "class");
  std::vector<std::string> bases;
  for (const TSNode base : TreeReader::childrenNamed(cls, "base")) bases.push_back(reader.textOf(base));
  EXPECT_EQ(bases, (std::vector<std::string>{"B", "C"}));
}

TEST_F(TestTreeReader, ANodeSaysWhereItIsWrittenFromOne) {
  const ds::parse::Span span = reader.spanOf(find(reader.root(), "class"));
  EXPECT_EQ(reader.file().path, "a.ds");
  EXPECT_EQ(span.line, 3);
  EXPECT_EQ(span.column, 5);
  EXPECT_EQ(span.endLine, 4);
}

}  // namespace
