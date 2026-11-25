#include <gtest/gtest.h>

#include "ds/layout/TableText.hpp"

namespace {

/// A text of a table to place (DS-464): its words, each in characters.
TEST(TestTableText, ATextIsItsWordsInCharacters) {
  const ds::layout::TableText t{{2, 7}};
  ASSERT_EQ(t.words.size(), 2u);
  EXPECT_EQ(t.words[1], 7u);
  EXPECT_TRUE(ds::layout::TableText{}.words.empty());
}

}  // namespace
