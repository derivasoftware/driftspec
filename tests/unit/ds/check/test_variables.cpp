#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "ds/check/Variables.hpp"
#include "ds/model/Model.hpp"

namespace {

using ds::model::LiteralForm;

/// The variables of a formula and their slots (DS-381): every variable once, in the order of the names, and bindings
/// ordered as a map of name to text would order them.
class TestVariables : public ::testing::Test {
 protected:
  static ds::model::Literal literal(LiteralForm form, const std::string& name, std::vector<ds::model::Term> terms) {
    ds::model::Literal l;
    l.form = form;
    l.name = name;
    l.terms = std::move(terms);
    return l;
  }
  ds::model::Formula formula() const {
    ds::model::Formula f;
    f.ranges.push_back({"class", "c", "", {}});
    f.where.push_back(literal(LiteralForm::Relation, "inherits", {{"c", true}, {"b", true}}));
    f.claims.push_back({literal(LiteralForm::In, "", {{"c", true}, {"p", false}})});
    return f;
  }
  const std::string a = "A", z = "Z";
};

TEST_F(TestVariables, EachVariableHasOneSlotInTheOrderOfTheNames) {
  const ds::check::Variables variables(formula());
  ASSERT_EQ(variables.size(), 2u);  // b and c; p is no variable
  EXPECT_EQ(variables.name(0), "b");
  EXPECT_EQ(variables.name(1), "c");
  EXPECT_EQ(variables.slotOf("c"), 1);
  EXPECT_EQ(variables.slotOf("p"), -1);
}

TEST_F(TestVariables, BindingsAreOrderedAsAMapOfNameToTextWouldBe) {
  const ds::check::Variables variables(formula());
  const std::vector<const std::string*> onlyC{nullptr, &a}, bothA{&a, &a}, bZ{&z, &a};
  EXPECT_TRUE(variables.before(bothA, onlyC));  // (b, A) comes before (c, A)
  EXPECT_FALSE(variables.before(onlyC, bothA));
  EXPECT_TRUE(variables.before(bothA, bZ));      // same name, A before Z
  EXPECT_FALSE(variables.before(bothA, bothA));  // equal is not before
  EXPECT_EQ(variables.pairs(bZ), (std::vector<std::pair<std::string, std::string>>{{"b", "Z"}, {"c", "A"}}));
}

}  // namespace
