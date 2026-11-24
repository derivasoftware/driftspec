#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "ds/text/Utf8.hpp"

namespace {

/// UTF-8 as the Unicode standard writes it (DS-411): the first byte that is not part of a well-formed character, or
/// npos when every byte is.
constexpr std::size_t kWhole = std::string_view::npos;

std::size_t invalidAt(const std::string& bytes) { return ds::text::Utf8::invalidAt(bytes); }

TEST(TestUtf8, AsciiIsUtf8) {
  EXPECT_EQ(invalidAt(""), kWhole);
  EXPECT_EQ(invalidAt("package p {\n  module m { }\n}\n"), kWhole);
  EXPECT_EQ(invalidAt(std::string("a\0b\x7f", 4)), kWhole);  // NUL and DEL are characters too
}

TEST(TestUtf8, EveryWellFormedLengthIsUtf8) {
  EXPECT_EQ(invalidAt("caf\xc3\xa9"), kWhole);                       // é, two bytes
  EXPECT_EQ(invalidAt("\xe2\x86\x92 \xe2\x82\xac"), kWhole);         // → and €, three
  EXPECT_EQ(invalidAt("\xf0\x9f\x98\x80"), kWhole);                  // 😀, four
  EXPECT_EQ(invalidAt("\xc2\x80\xdf\xbf"), kWhole);                  // U+0080 and U+07FF, the ends of two bytes
  EXPECT_EQ(invalidAt("\xe0\xa0\x80\xef\xbf\xbf"), kWhole);          // U+0800 and U+FFFF, of three
  EXPECT_EQ(invalidAt("\xed\x9f\xbf\xee\x80\x80"), kWhole);          // U+D7FF and U+E000, either side of the surrogates
  EXPECT_EQ(invalidAt("\xf0\x90\x80\x80\xf4\x8f\xbf\xbf"), kWhole);  // U+10000 and U+10FFFF, of four
  EXPECT_EQ(invalidAt("\xef\xbb\xbfpackage p { }\n"), kWhole);       // a byte order mark is a character
}

TEST(TestUtf8, ACharacterCutShortIsRefusedAtItsLead) {
  EXPECT_EQ(invalidAt("caf\xe9 latin1"), 3u);  // a Latin-1 é: a lead of three, followed by a space
  EXPECT_EQ(invalidAt("ab\xc3"), 2u);          // by the end of the text
  EXPECT_EQ(invalidAt("ab\xe2\x86"), 2u);
  EXPECT_EQ(invalidAt("ab\xf0\x9f\x98"), 2u);
  EXPECT_EQ(invalidAt("\xe2\x86x"), 0u);  // its last byte is no continuation
  EXPECT_EQ(invalidAt("\xf0\x9f\x98\xc3\xa9"), 0u);
}

TEST(TestUtf8, AnOverlongFormIsRefused) {
  EXPECT_EQ(invalidAt("\xc0\x80"), 0u);          // NUL in two bytes
  EXPECT_EQ(invalidAt("\xc1\xbf"), 0u);          // U+007F in two
  EXPECT_EQ(invalidAt("\xe0\x80\x80"), 0u);      // in three
  EXPECT_EQ(invalidAt("\xe0\x9f\xbf"), 0u);      // U+07FF in three
  EXPECT_EQ(invalidAt("\xf0\x80\x80\x80"), 0u);  // in four
  EXPECT_EQ(invalidAt("\xf0\x8f\xbf\xbf"), 0u);  // U+FFFF in four
}

TEST(TestUtf8, ASurrogateIsRefused) {
  EXPECT_EQ(invalidAt("x\xed\xa0\x80"), 1u);             // U+D800
  EXPECT_EQ(invalidAt("x\xed\xbf\xbf"), 1u);             // U+DFFF
  EXPECT_EQ(invalidAt("\xed\xa0\xbd\xed\xb8\x80"), 0u);  // a pair, as CESU-8 writes 😀
}

TEST(TestUtf8, ACodePointAboveU10FFFFIsRefused) {
  EXPECT_EQ(invalidAt("\xf4\x90\x80\x80"), 0u);  // U+110000
  EXPECT_EQ(invalidAt("\xf5\x80\x80\x80"), 0u);
  EXPECT_EQ(invalidAt("\xf8\x88\x80\x80\x80"), 0u);  // five bytes, which UTF-8 no longer has
  EXPECT_EQ(invalidAt("ok \xff"), 3u);
  EXPECT_EQ(invalidAt("ok \xfe"), 3u);
}

TEST(TestUtf8, AContinuationByteNothingLeadsIsRefused) {
  EXPECT_EQ(invalidAt("\x80"), 0u);
  EXPECT_EQ(invalidAt("a\xbf"), 1u);
  EXPECT_EQ(invalidAt("\xc3\xa9\xa9"), 2u);  // one more than é takes
}

TEST(TestUtf8, TheOffsetIsTheFirstSuchByte) {
  // past characters of every length, and before a second byte that is not UTF-8 either
  EXPECT_EQ(invalidAt("a\xc3\xa9\xe2\x86\x92\xf0\x9f\x98\x80z\x80\xff"), 11u);
  EXPECT_EQ(invalidAt("line one\nline \xe9two\n"), 14u);
}

}  // namespace
