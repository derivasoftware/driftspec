#include <gtest/gtest.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/check/Facts.hpp"
#include "ds/check/Resolver.hpp"
#include "ds/check/RuleValidator.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace {

using ds::model::Kind;
using ds::model::LiteralForm;

/// A formula checked before it is evaluated (DS-380): what it names, what it binds, what can never match.
class TestRuleValidator : public ::testing::Test {
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
};

TEST_F(TestRuleValidator, AWellFormedRuleResolvesWhatItNames) {
  const ds::check::Resolver resolver(model);
  const ds::check::Facts facts(model, metamodel);
  ds::check::RuleValidator validator(metamodel, resolver, facts);
  ds::model::Formula f;
  f.ranges.push_back({"class", "c", "", {}});
  f.claims.push_back({literal(LiteralForm::Relation, "inherits", {{"c", true}, {"B", false}})});
  std::vector<std::pair<std::string, std::string>> resolved;
  EXPECT_TRUE(validator.validate("p@m::r", f, resolved));
  EXPECT_EQ(resolved, (std::vector<std::pair<std::string, std::string>>{{"B", "p@m::B"}}));
  EXPECT_TRUE(validator.take().empty());
}

TEST_F(TestRuleValidator, AnUnknownRelationAndAnUnboundVariableAreFindings) {
  const ds::check::Resolver resolver(model);
  const ds::check::Facts facts(model, metamodel);
  ds::check::RuleValidator validator(metamodel, resolver, facts);
  ds::model::Formula f;
  f.ranges.push_back({"class", "c", "", {}});
  f.claims.push_back({literal(LiteralForm::Relation, "befriends", {{"c", true}, {"x", true}}),
                      literal(LiteralForm::Relation, "inherits", {{"y", true}, {"c", true}}, true)});
  std::vector<std::pair<std::string, std::string>> resolved;
  EXPECT_FALSE(validator.validate("p@m::r", f, resolved));
  std::set<std::string> codes;
  for (const auto& finding : validator.take()) codes.insert(finding.code);
  EXPECT_EQ(codes, (std::set<std::string>{"E-RULE-UNKNOWN", "E-RULE-UNSAFE"}));
  EXPECT_TRUE(validator.take().empty());  // taken once
}

}  // namespace
