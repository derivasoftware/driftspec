#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"

namespace {

/// A type is the code's text (DS-18): its names are read out of it, with the arguments of a bracket
/// written right after a name, and no name of any language is known.

std::vector<std::string> names(const std::string& text) {
  std::vector<std::string> out;
  for (const auto& t : ds::bind::scanTypeTokens(text)) out.push_back(t.name);
  return out;
}

TEST(TestScanTypeTokens, EveryNameOfTheTextIsReadLeftToRight) {
  EXPECT_EQ(names("std::map<std::string, std::unique_ptr<IStep>>"),
            (std::vector<std::string>{"std::map", "std::string", "std::unique_ptr", "IStep"}));
  EXPECT_EQ(names("Pool*"), (std::vector<std::string>{"Pool"}));
  EXPECT_EQ(names("Optional[str]"), (std::vector<std::string>{"Optional", "str"}));
  EXPECT_EQ(names("Callable[[str], int]"), (std::vector<std::string>{"Callable", "str", "int"}));
  EXPECT_EQ(names("long long"), (std::vector<std::string>{"long", "long"}));
  EXPECT_EQ(names("Pool | None"), (std::vector<std::string>{"Pool", "None"}));
  EXPECT_EQ(names("shop@store::Cart"), (std::vector<std::string>{"shop@store::Cart"}));
}

TEST(TestScanTypeTokens, ABracketRightAfterANameGivesItsArguments) {
  const auto tokens = ds::bind::scanTypeTokens("std::map<std::string, std::vector<Item>>");
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens[0].args, (std::vector<std::string>{"std::string", "std::vector<Item>"}));
  const auto python = ds::bind::scanTypeTokens("dict[str, list[Item]]");
  ASSERT_FALSE(python.empty());
  EXPECT_EQ(python[0].args, (std::vector<std::string>{"str", "list[Item]"}));
  EXPECT_TRUE(ds::bind::scanTypeTokens("Item")[0].args.empty());
}

}  // namespace
