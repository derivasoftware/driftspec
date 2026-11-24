#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ds/parse/Lookahead.hpp"
#include "ds/parse/Parser.hpp"

namespace {

using ds::parse::Lookahead;

/// What the grammar takes in one parse state (DS-374): its keywords, each once and in order, then an annotation and
/// a field when it takes those.
class TestLookahead : public ::testing::Test {
 protected:
  ds::parse::Parser parser;
};

TEST_F(TestLookahead, AtTheTopOfAFileTheGrammarTakesItsDeclarations) {
  const Lookahead top(parser.language(), 1);
  const std::vector<std::string>& keywords = top.keywords();
  EXPECT_NE(std::find(keywords.begin(), keywords.end(), "package"), keywords.end());
  EXPECT_TRUE(std::is_sorted(keywords.begin(), keywords.end()));
  EXPECT_EQ(std::adjacent_find(keywords.begin(), keywords.end()), keywords.end());
}

TEST_F(TestLookahead, TheWordsAreTheKeywordsThenAnAnnotationAndAField) {
  for (const TSStateId state : {TSStateId{1}, TSStateId{2}, TSStateId{10}, TSStateId{100}}) {
    const Lookahead takes(parser.language(), state);
    const std::vector<std::string> words = takes.words();
    ASSERT_GE(words.size(), takes.keywords().size());
    EXPECT_TRUE(std::equal(takes.keywords().begin(), takes.keywords().end(), words.begin()));
    EXPECT_LE(words.size() - takes.keywords().size(), 2u);
    for (std::size_t i = takes.keywords().size(); i < words.size(); ++i)
      EXPECT_TRUE(words[i] == "@annotation" || words[i] == "key: value") << words[i];
  }
}

TEST_F(TestLookahead, WhatTheParserCompletesWithIsWhatTheStateAfterTheLastTokenTakes) {
  // an empty file: the last thing before the place is the empty root, and the state after it is what comes next
  const ds::parse::ParsedFile empty = parser.parseText("x.ds", "");
  const TSStateId after = ts_node_next_parse_state(ts_tree_root_node(empty.tree.get()));
  EXPECT_EQ(parser.expectedAt("", 0, 0), Lookahead(parser.language(), after).words());
}

TEST_F(TestLookahead, AStateTheGrammarDoesNotHaveTakesNothing) {
  const Lookahead none(parser.language(), static_cast<TSStateId>(ts_language_state_count(&parser.language())));
  EXPECT_TRUE(none.keywords().empty());
  EXPECT_TRUE(none.words().empty());
}

}  // namespace
