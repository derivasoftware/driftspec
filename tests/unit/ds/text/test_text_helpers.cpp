#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/text/Text.hpp"

namespace {

using Words = std::vector<std::string>;

/// The text helpers of the whole engine, in one place (DS-344).

TEST(TestSplitTrimmed, EachItemWithoutTheSpacesAroundItAndNoEmptyOne) {
  EXPECT_EQ(ds::text::splitTrimmed("a, b,, c ", ','), (Words{"a", "b", "c"}));
  EXPECT_EQ(ds::text::splitTrimmed("one", ','), (Words{"one"}));
  EXPECT_TRUE(ds::text::splitTrimmed("", ',').empty());
  EXPECT_EQ(ds::text::splitTrimmed("x\ny", '\n'), (Words{"x", "y"}));
  EXPECT_EQ(ds::text::splitTrimmed(",a,", ','), (Words{"a"}));             // a separator at either end adds no item
  EXPECT_EQ(ds::text::splitTrimmed(" a\t, b", ','), (Words{"a\t", "b"}));  // only spaces are trimmed
}

TEST(TestJoin, TheItemsWithTheSeparatorBetweenEveryTwo) {
  EXPECT_EQ(ds::text::join({"a", "b", "c"}, ", "), "a, b, c");
  EXPECT_EQ(ds::text::join({"a"}, ", "), "a");
  EXPECT_EQ(ds::text::join({}, ", "), "");
  // DS-344: an empty first item once swallowed the separator after it
  EXPECT_EQ(ds::text::join({"", "a"}, ","), ",a");
  EXPECT_EQ(ds::text::join({"a", "", "b"}, ","), "a,,b");
  EXPECT_EQ(ds::text::join({"", ""}, "|"), "|");
}

TEST(TestLinesOf, ANewLineOrAWrittenBackslashNEndsALine) {
  EXPECT_EQ(ds::text::linesOf("a\nb"), (Words{"a", "b"}));
  EXPECT_EQ(ds::text::linesOf("a\\nb"), (Words{"a", "b"}));
  EXPECT_EQ(ds::text::linesOf("one"), (Words{"one"}));
  EXPECT_EQ(ds::text::linesOf(""), (Words{""}));
  EXPECT_EQ(ds::text::linesOf("a\\"), (Words{"a\\"}));  // a backslash at the end is text
}

TEST(TestTrimWhitespace, SpacesTabsAndNewLinesGoFromBothEnds) {
  EXPECT_EQ(ds::text::trimWhitespace(" \t a b \r\n"), "a b");
  EXPECT_EQ(ds::text::trimWhitespace(" \n "), "");
}

TEST(TestTrimSpaces, OnlySpacesGoFromBothEnds) {
  EXPECT_EQ(ds::text::trimSpaces("  a b  "), "a b");
  EXPECT_EQ(ds::text::trimSpaces("\ta \n"), "\ta \n");  // tabs and new lines stay
  EXPECT_EQ(ds::text::trimSpaces("   "), "");
}

TEST(TestBase64, BytesAreWrittenFourCharactersForThree) {
  EXPECT_EQ(ds::text::base64(reinterpret_cast<const unsigned char*>("Man"), 3), "TWFu");
  EXPECT_EQ(ds::text::base64(reinterpret_cast<const unsigned char*>("Ma"), 2), "TWE=");
  EXPECT_EQ(ds::text::base64(nullptr, 0), "");
}

TEST(TestFromBase64, TheBytesComeBackUpToThePadding) {
  const std::string bytes = std::string("any carnal pleas") + '\0' + "\xff";
  const std::string coded = ds::text::base64(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
  EXPECT_EQ(ds::text::fromBase64(coded), bytes);
  EXPECT_EQ(ds::text::fromBase64("TWE="), "Ma");
  EXPECT_EQ(ds::text::fromBase64("TW!E"), "M");  // what is no base64 ends it
}

TEST(TestEscapeXml, TheCharactersXmlGivesAMeaningBecomeEntities) {
  EXPECT_EQ(ds::text::escapeXml("a<b & \"c\">"), "a&lt;b &amp; &quot;c&quot;&gt;");
  EXPECT_EQ(ds::text::escapeXml("it's"), "it's");  // an apostrophe is written as it is, as the SVGs always had it
}

TEST(TestCharacters, EachCodePointCountsOnceWhateverItsBytes) {
  EXPECT_EQ(ds::text::characters(""), 0u);
  EXPECT_EQ(ds::text::characters("run"), 3u);
  EXPECT_EQ(ds::text::characters("Console \u25C1 Channel"), 17u);  // the triangle takes three bytes, one character
  EXPECT_EQ(ds::text::characters("\xC2\xB7"), 1u);                 // a middle dot: two bytes
  EXPECT_EQ(ds::text::characters("\xE2\x86\x92 int"), 5u);         // an arrow, three bytes, then four more
}

TEST(TestWrap, TheWordsFillLinesNoLongerThanTheWidthAndALongWordTakesALineOfItsOwn) {
  EXPECT_EQ(ds::text::wrap("a bb ccc dddd", 6), (std::vector<std::string>{"a bb", "ccc", "dddd"}));
  EXPECT_EQ(ds::text::wrap("unbreakable word", 4), (std::vector<std::string>{"unbreakable", "word"}));
  EXPECT_EQ(ds::text::wrap("  spaced   out  ", 20), (std::vector<std::string>{"spaced out"}));
  EXPECT_TRUE(ds::text::wrap("", 10).empty());
}

}  // namespace
