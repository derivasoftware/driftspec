#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/check/Evaluator.hpp"
#include "ds/model/Model.hpp"

namespace {

using ds::model::Kind;
using ds::model::LiteralForm;

/// The evaluator alone (REQ-00096-1): a model built by hand, a formula stated on it, the rule decided.
class TestEvaluator : public ::testing::Test {
 protected:
  ds::model::Model model;

  void SetUp() override {
    declare(Kind::Package, "p", "");
    declare(Kind::Module, "p@m", "p");
    declare(Kind::Class, "p@m::A", "p@m");
    declare(Kind::Class, "p@m::B", "p@m");
    declare(Kind::Rule, "p@m::r", "p@m");
    model.relate({"inherits", "p@m::A", "p@m::B", {}, ""});
  }

  void declare(Kind kind, const std::string& id, const std::string& scope) {
    ds::model::Entity e;
    e.kind = kind;
    e.id = id;
    e.scope = scope;
    model.declare(e);
    if (!scope.empty()) model.relate({"contains", scope, id, {}, ""});
  }

  static ds::model::Literal literal(LiteralForm form, const std::string& name, std::vector<ds::model::Term> terms) {
    ds::model::Literal l;
    l.form = form;
    l.name = name;
    l.terms = std::move(terms);
    return l;
  }

  std::vector<std::string> broken() {
    const ds::check::Resolver resolver(model);  // the evaluator needs names resolved, not a checker (DS-125)
    std::vector<std::string> out;
    for (const auto& f : ds::check::Evaluator(model, resolver).run())
      if (f.code == "E-RULE-BROKEN") out.push_back(f.message);
    return out;
  }
};

TEST_F(TestEvaluator, ANeverIsBrokenByEachBindingWithAnAnswer) {
  ds::model::Formula f;
  f.ranges.push_back({"class", "c", "p@m", {}});
  f.never = true;
  f.claims.push_back({literal(LiteralForm::Relation, "inherits", {{"c", true}, {"x", true}})});
  model.state("p@m::r", f);
  EXPECT_EQ(broken(), (std::vector<std::string>{"p@m::r is broken for c = p@m::A: x = p@m::B"}));
}

TEST_F(TestEvaluator, AHoldsIsBrokenByEachBindingWithNoAnswer) {
  ds::model::Formula f;
  f.ranges.push_back({"class", "c", "p@m", {}});
  f.claims.push_back({literal(LiteralForm::In, "", {{"c", true}, {"p", false}})});  // every class of m is in p
  model.state("p@m::r", f);
  EXPECT_TRUE(broken().empty());
}

}  // namespace
