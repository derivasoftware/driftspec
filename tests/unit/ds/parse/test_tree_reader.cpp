#include <gtest/gtest.h>

#include <cstring>

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
  EXPECT_EQ(reader.fieldText(cls, ds::parse::Field::name), "A");
  EXPECT_EQ(reader.fieldText(cls, ds::parse::Field::guard), "");  // a field the node lacks reads as nothing
  EXPECT_TRUE(ts_node_is_null(reader.child(cls, ds::parse::Field::guard)));
}

/// One file read: the text a node of its tree spans is the text written there; the file is data, the reader reads it
/// (DS-374).
TEST(TestTreeReaderText, ANodeSpansTheTextWrittenThere) {
  ds::parse::Parser parser;
  const auto file = parser.parseText("a.ds", "package shop {\n  module till {\n  }\n}\n");
  ASSERT_FALSE(file.hasSyntaxErrors);
  const TreeReader reader(file);
  EXPECT_EQ(reader.textOf(reader.root()), file.text);
  const TSNode package = ts_node_named_child(reader.root(), 0);
  EXPECT_EQ(reader.textOf(package).rfind("package shop {", 0), 0u);
}

/// The reader reads with the grammar its file was parsed with, which the tree carries (DS-374).
TEST_F(TestTreeReader, TheGrammarIsTheOneTheFileWasParsedWith) { EXPECT_EQ(&reader.language(), &parser.language()); }

TEST_F(TestTreeReader, EveryFieldIsAFieldOfTheGrammarByItsName) {
  const TSLanguage* language = &parser.language();
  const uint32_t count = ts_language_field_count(language);
  EXPECT_EQ(static_cast<uint32_t>(ds::parse::Field::visibility) + 1, count);  // every field of the grammar, none more
  for (uint32_t i = 0; i < count; ++i) {
    const char* name = TreeReader::nameOf(static_cast<ds::parse::Field>(i));
    EXPECT_NE(ts_language_field_id_for_name(language, name, static_cast<uint32_t>(std::strlen(name))), 0) << name;
  }
  EXPECT_STREQ(TreeReader::nameOf(ds::parse::Field::operator_), "operator");
  EXPECT_STREQ(TreeReader::nameOf(ds::parse::Field::else_scenario), "else_scenario");
}

TEST_F(TestTreeReader, AFieldThatRepeatsGivesEveryChild) {
  const TSNode cls = find(reader.root(), "class");
  std::vector<std::string> bases;
  for (const TSNode base : reader.childrenNamed(cls, ds::parse::Field::base)) bases.push_back(reader.textOf(base));
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
