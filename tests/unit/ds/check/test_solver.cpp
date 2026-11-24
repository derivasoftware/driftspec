#include <gtest/gtest.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/check/Facts.hpp"
#include "ds/check/Solver.hpp"
#include "ds/check/Variables.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace {

using ds::model::Kind;
using ds::model::LiteralForm;

/// A body answered over the facts alone (DS-380): every binding that makes each literal true.
class TestSolver : public ::testing::Test {
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

/// The answers of `body`, each as (name, text) pairs: the body solved with the slots of a formula that holds it.
std::vector<std::vector<std::pair<std::string, std::string>>> named(const ds::check::Solver& solver,
                                                                    const std::vector<ds::model::Literal>& body) {
  ds::model::Formula formula;
  formula.where = body;
  const ds::check::Variables variables(formula);
  std::vector<std::vector<std::pair<std::string, std::string>>> out;
  for (const auto& answer : solver.solve("p@m::r", body, variables, std::vector<const std::string*>(variables.size(), nullptr), false))
    out.push_back(variables.pairs(answer));
  return out;
}

TEST_F(TestSolver, ARelationBindsWhatItJoins) {
  ds::check::Facts facts(model, metamodel);
  facts.index();
  const ds::check::Solver solver(facts);
  const auto answers = named(solver, {literal(LiteralForm::Relation, "inherits", {{"x", true}, {"y", true}})});
  ASSERT_EQ(answers.size(), 1u);
  EXPECT_EQ(answers[0], (std::vector<std::pair<std::string, std::string>>{{"x", "p@m::A"}, {"y", "p@m::B"}}));
}

TEST_F(TestSolver, ANegationTestsWhatAPositiveLiteralBound) {
  ds::check::Facts facts(model, metamodel);
  facts.index();
  facts.record("p@m::r", "B", "p@m::B");  // the name the rule writes, as the validator resolved it
  const ds::check::Solver solver(facts);
  // the classes that do not inherit B: B itself
  const auto answers = named(solver, {literal(LiteralForm::Kind, "class", {{"c", true}}),
                                      literal(LiteralForm::Relation, "inherits", {{"c", true}, {"B", false}}, true)});
  ASSERT_EQ(answers.size(), 1u);
  EXPECT_EQ(answers[0], (std::vector<std::pair<std::string, std::string>>{{"c", "p@m::B"}}));
}

TEST_F(TestSolver, ADefinitionAnswersWhatIsDerivedForIt) {
  ds::check::Facts facts(model, metamodel);
  facts.index();
  facts.record("p@m::r", "call:d", "p@m::d");
  facts.derive("p@m::d", {"p@m::A"});
  const ds::check::Solver solver(facts);
  const auto answers = named(solver, {literal(LiteralForm::Call, "d", {{"x", true}})});
  ASSERT_EQ(answers.size(), 1u);
  EXPECT_EQ(answers[0], (std::vector<std::pair<std::string, std::string>>{{"x", "p@m::A"}}));
  EXPECT_TRUE(ds::check::Solver::compares(LiteralForm::Less));
  EXPECT_FALSE(ds::check::Solver::compares(LiteralForm::Equal));
}

}  // namespace
