#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// What a rule reads of an entity (REQ-00196-1, DS-403): name(x), id(x) and kind(x), a value matched against a
/// pattern compiled once, and a member's flag read with is.
class TestRuleProperties : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("a.ds", text);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  std::vector<std::string> messages(const std::string& code) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == code) out.push_back(f.message);
    return out;
  }

  /// The rules broken, by their identity, once per binding.
  std::vector<std::string> broken() const {
    std::vector<std::string> out;
    for (const auto& m : messages("E-RULE-BROKEN")) out.push_back(m.substr(0, m.find(' ')));
    return out;
  }

  static std::size_t count(const std::vector<std::string>& in, const std::string& what) {
    std::size_t n = 0;
    for (const auto& s : in) n += s == what;
    return n;
  }
};

const char* const kShapes =
    "package p {\n"
    "  requirement REQ-00001-1: The shapes shall be named by the Owner's rules\n"
    "  module shapes {\n"
    "    interface IShape {\n"
    "      operation area() -> int\n"
    "    }\n"
    "    interface Drawable {\n"
    "      operation draw() -> none\n"
    "    }\n"
    "    class Square {\n"
    "      + area() -> int\n"
    "    }\n"
    "    class AShape {\n"
    "      + abstract area() -> int\n"
    "      + name() -> std::string\n"
    "    }\n";

TEST_F(TestRuleProperties, ARuleReadsTheNameTheIdentityAndTheKindOfAnEntity) {
  load(std::string(kShapes) +
       "    rule square-by-name {\n"
       "      @requirement REQ-00001-1\n"
       "      for class c in p@shapes\n"
       "      never name(c) = \"Square\"\n"
       "    }\n"
       "    rule square-by-id {\n"
       "      @requirement REQ-00001-1\n"
       "      for class c in p@shapes\n"
       "      never id(c) = \"p@shapes::Square\"\n"
       "    }\n"
       "    rule interfaces-by-kind {\n"
       "      @requirement REQ-00001-1\n"
       "      for interface i in p@shapes\n"
       "      never kind(i) = \"interface\"\n"
       "    }\n"
       "  }\n"
       "}\n");
  const auto rules = broken();
  EXPECT_EQ(count(rules, "p@shapes::square-by-name"), 1u);  // Square, and not AShape
  EXPECT_EQ(count(rules, "p@shapes::square-by-id"), 1u);
  EXPECT_EQ(count(rules, "p@shapes::interfaces-by-kind"), 2u);  // IShape and Drawable
}

TEST_F(TestRuleProperties, ANameMatchesAPattern) {
  load(std::string(kShapes) +
       "    rule interfaces-are-named-i {\n"
       "      @requirement REQ-00001-1\n"
       "      for interface i in p@shapes\n"
       "      holds name(i) matches \"^I[A-Z]\"\n"
       "    }\n"
       "  }\n"
       "}\n");
  const auto said = messages("E-RULE-BROKEN");
  ASSERT_EQ(said.size(), 1u);  // Drawable breaks it; IShape keeps it
  EXPECT_NE(said[0].find("p@shapes::Drawable"), std::string::npos);
  EXPECT_TRUE(messages("E-RULE-PATTERN").empty());
}

TEST_F(TestRuleProperties, AMembersFlagIsReadWithIs) {
  load(std::string(kShapes) +
       "    rule nothing-abstract {\n"
       "      @requirement REQ-00001-1\n"
       "      for method m in p@shapes\n"
       "      never m is abstract\n"
       "    }\n"
       "  }\n"
       "}\n");
  const auto said = messages("E-RULE-BROKEN");
  ASSERT_EQ(said.size(), 1u);  // AShape::area, and neither name nor Square::area
  EXPECT_NE(said[0].find("p@shapes::AShape::area"), std::string::npos);
  EXPECT_TRUE(messages("E-RULE-UNKNOWN").empty());  // abstract is a flag, not a kind it does not know
}

TEST_F(TestRuleProperties, APatternThatIsNoStringOrDoesNotCompileIsReported) {
  load(std::string(kShapes) +
       "    rule unclosed {\n"
       "      @requirement REQ-00001-1\n"
       "      for class c in p@shapes\n"
       "      holds name(c) matches \"[A-Z\"\n"
       "    }\n"
       "    rule unquoted {\n"
       "      @requirement REQ-00001-1\n"
       "      for class c in p@shapes\n"
       "      holds name(c) matches 3\n"
       "    }\n"
       "  }\n"
       "}\n");
  const auto said = messages("E-RULE-PATTERN");
  ASSERT_EQ(said.size(), 2u);
  EXPECT_NE(said[0].find("does not compile"), std::string::npos);
  EXPECT_NE(said[1].find("no string"), std::string::npos);
  EXPECT_TRUE(broken().empty());  // neither is evaluated
}

TEST_F(TestRuleProperties, ADefinitionThatTakesAReservedNameIsReported) {
  load(std::string(kShapes) +
       "    definition kind(x) {\n"
       "      if x is class\n"
       "    }\n"
       "  }\n"
       "}\n");
  const auto said = messages("E-RULE-RESERVED");
  ASSERT_EQ(said.size(), 1u);
  EXPECT_NE(said[0].find("'kind'"), std::string::npos);
}

TEST_F(TestRuleProperties, APropertyNeverBindsItsVariable) {
  load(std::string(kShapes) +
       "    rule from-a-name {\n"
       "      @requirement REQ-00001-1\n"
       "      for class c in p@shapes\n"
       "      never name(x) = \"Square\"\n"
       "    }\n"
       "  }\n"
       "}\n");
  EXPECT_EQ(messages("E-RULE-UNSAFE").size(), 1u);  // x comes from nothing but its name
  EXPECT_TRUE(broken().empty());
}

}  // namespace
