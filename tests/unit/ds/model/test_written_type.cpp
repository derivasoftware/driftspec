#include <gtest/gtest.h>

#include <string>

#include "ds/model/WrittenType.hpp"

namespace {

std::string unqualified(const std::string& text) { return ds::model::WrittenType(text).unqualified(); }

/// A type as the design writes it (DS-390): read without the names that qualify it.
TEST(TestWrittenType, ATypeReadsWithoutTheNamesThatQualifyIt) {
  EXPECT_EQ(unqualified("std::vector<model::Entity>"), "vector<Entity>");
  EXPECT_EQ(unqualified("std::map<std::string, ds::model::Entity*>"), "map<string, Entity*>");
  EXPECT_EQ(unqualified("int"), "int");
  EXPECT_EQ(unqualified(""), "");
}

TEST(TestWrittenType, OnlyAWholeNameQualifiesNeverAPartOfALongerWord) {
  EXPECT_EQ(unqualified("9a::b"), "9a::b");                              // a word that starts with a digit is no name
  EXPECT_EQ(unqualified("a::::b"), "::b");                               // a name and its ::, then what is left as written
  EXPECT_EQ(unqualified("::x"), "::x");                                  // no name before the ::
  EXPECT_EQ(unqualified("a :: b"), "a :: b");                            // the :: must follow the name
  EXPECT_EQ(unqualified("Map<K, V>::iterator"), "Map<K, V>::iterator");  // > is no name
}

}  // namespace
