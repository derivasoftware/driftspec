#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/machine/WrittenFields.hpp"
#include "ds/model/Model.hpp"

namespace {

/// The lists the binder writes into a field, kept as joined text (DS-367).
TEST(TestWrittenFields, PiecesAreWhatIsWrittenBetweenTheSeparatorsUntrimmed) {
  using ds::machine::WrittenFields;
  EXPECT_EQ(WrittenFields::pieces("a|b||c", '|'), (std::vector<std::string>{"a", "b", "", "c"}));
  EXPECT_EQ(WrittenFields::pieces("Open, Paid", ','), (std::vector<std::string>{"Open", " Paid"}));
  EXPECT_TRUE(WrittenFields::pieces("", '\n').empty());
}

TEST(TestWrittenFields, JoinedWritesTheSeparatorOnceSomethingIsWritten) {
  using ds::machine::WrittenFields;
  EXPECT_EQ(WrittenFields::joined({"a", "b"}, ", "), "a, b");
  EXPECT_EQ(WrittenFields::joined({"", "a"}, ", "), "a");  // nothing written yet: no separator
  EXPECT_EQ(WrittenFields::joined({"a", "", "b"}, ", "), "a, , b");
  EXPECT_EQ(WrittenFields::joined({}, ", "), "");
}

}  // namespace
