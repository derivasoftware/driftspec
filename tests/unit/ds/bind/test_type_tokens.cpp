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

TEST(TestScanTypeTokens, ADotQualifiesAsTwoColonsDo) {
  EXPECT_EQ(names("lsp.Location"), (std::vector<std::string>{"lsp::Location"}));
  EXPECT_EQ(names("lsp.Hover | None"), (std::vector<std::string>{"lsp::Hover", "None"}));
  EXPECT_EQ(names("typing.Optional[x.Y]"), (std::vector<std::string>{"typing::Optional", "x::Y"}));
  EXPECT_EQ(names("a.b.C"), (std::vector<std::string>{"a::b::C"}));
  EXPECT_EQ(ds::bind::scanTypeTokens("typing.Optional[x.Y]")[0].args, (std::vector<std::string>{"x.Y"}));
}

TEST(TestScanTypeTokens, ADotThatQualifiesNoNameGivesNoToken) {
  EXPECT_EQ(names("Foo."), (std::vector<std::string>{"Foo"}));
  EXPECT_EQ(names(".Foo"), (std::vector<std::string>{"Foo"}));
  EXPECT_EQ(names("Literal[1.5]"), (std::vector<std::string>{"Literal"}));
  EXPECT_EQ(names("Literal[1.e5]"), (std::vector<std::string>{"Literal"}));
  EXPECT_EQ(names("tuple[int, ...]"), (std::vector<std::string>{"tuple", "int"}));
  EXPECT_EQ(names("Args&&..."), (std::vector<std::string>{"Args"}));
  EXPECT_EQ(names("Ts...Rest"), (std::vector<std::string>{"Ts", "Rest"}));
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
