#include <gtest/gtest.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/check/Facts.hpp"
#include "ds/check/Strata.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace {

using ds::model::Kind;
using ds::model::LiteralForm;

/// The order the definitions are computed in (DS-380): above what they negate, after what they apply.
class TestStrata : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    declare(Kind::Package, "p", "");
    declare(Kind::Module, "p@m", "p");
    declare(Kind::Class, "p@m::A", "p@m");
    declare(Kind::Class, "p@m::B", "p@m");
    declare(Kind::Rule, "p@m::r", "p@m");
    declare(Kind::Definition, "p@m::d", "p@m");
    model.relate({"inherits", "p@m::A", "p@m::B", {}, ""});
  }

  void declare(Kind kind, const std::string& id, const std::string& scope) {
    ds::model::Entity e;
    e.kind = kind;
    e.id = id;
    e.name = id.substr(id.find_last_of(":@") + 1);
    e.scope = scope;
    model.declare(e);
    if (!scope.empty()) model.relate({"contains", scope, id, {}, ""});
  }

  static ds::model::Literal literal(LiteralForm form, const std::string& name, std::vector<ds::model::Term> terms, bool negated = false) {
    ds::model::Literal l;
    l.form = form;
    l.name = name;
    l.terms = std::move(terms);
    l.negated = negated;
    return l;
  }

  /// A definition `d(x)` whose one case applies `e(x)`, negated when asked.
  void applies(const std::string& d, const std::string& e, bool negated) {
    ds::model::Formula f;
    f.parameters = {"x"};
    f.cases.push_back({literal(LiteralForm::Kind, "class", {{"x", true}}), literal(LiteralForm::Call, e, {{"x", true}}, negated)});
    model.state("p@m::" + d, f);
  }
};

TEST_F(TestStrata, ADefinitionSitsAboveWhatItNegatesAndAfterWhatItApplies) {
  declare(Kind::Definition, "p@m::e", "p@m");
  declare(Kind::Definition, "p@m::g", "p@m");
  applies("d", "e", true);  // d negates e: one stratum above it
  applies("g", "e", false);
  ds::model::Formula base;
  base.parameters = {"x"};
  base.cases.push_back({literal(LiteralForm::Kind, "class", {{"x", true}})});
  model.state("p@m::e", base);
  ds::check::Facts facts(model, metamodel);
  facts.record("p@m::d", "call:e", "p@m::e");
  facts.record("p@m::g", "call:e", "p@m::e");
  ds::check::Strata strata(facts);
  std::set<std::string> invalid;
  EXPECT_TRUE(strata.stratify(invalid).empty());
  const auto levels = strata.levels(invalid);
  ASSERT_EQ(levels.size(), 2u);
  EXPECT_EQ(levels.at(0), (std::vector<std::string>{"p@m::e", "p@m::g"}));
  EXPECT_EQ(levels.at(1), (std::vector<std::string>{"p@m::d"}));
  EXPECT_EQ(strata.dependencyOrder({"p@m::g", "p@m::e"}), (std::vector<std::string>{"p@m::e", "p@m::g"}));
}

TEST_F(TestStrata, ANegationThroughRecursionHasNoStratum) {
  applies("d", "d", true);  // d holds where d does not
  ds::check::Facts facts(model, metamodel);
  facts.record("p@m::d", "call:d", "p@m::d");
  ds::check::Strata strata(facts);
  std::set<std::string> invalid;
  EXPECT_EQ(strata.stratify(invalid), (std::vector<std::string>{"p@m::d"}));
  EXPECT_TRUE(invalid.count("p@m::d"));
  EXPECT_TRUE(strata.levels(invalid).empty() || !strata.levels(invalid).begin()->second.empty());
}

}  // namespace
