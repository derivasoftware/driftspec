#include <gtest/gtest.h>

#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ds/check/Facts.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace {

using ds::model::Kind;
using ds::model::LiteralForm;

/// The model as facts (DS-380): what a rule is evaluated over, read once from the model.
class TestFacts : public ::testing::Test {
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

TEST_F(TestFacts, TheEntitiesOfAKindAndARelationBothWaysAreRead) {
  ds::check::Facts facts(model, metamodel);
  facts.index();
  ASSERT_NE(facts.ofKind("class"), nullptr);
  EXPECT_EQ(*facts.ofKind("class"), (std::vector<std::string>{"p@m::A", "p@m::B"}));
  EXPECT_EQ(facts.ofKind("interface"), nullptr);
  ASSERT_NE(facts.forward("inherits"), nullptr);
  EXPECT_EQ(facts.forward("inherits")->find("p@m::A")->second, "p@m::B");
  EXPECT_EQ(facts.backward("inherits").find("p@m::B")->second, "p@m::A");
  EXPECT_TRUE(facts.isOfKind("p@m::A", "class"));
  EXPECT_FALSE(facts.isOfKind("p@m::A", "module"));
}

TEST_F(TestFacts, WhatHoldsWhatAndTheFieldsOfAnEntityAreAnswered) {
  ds::check::Facts facts(model, metamodel);
  facts.index();  // what holds what is copied from the index, once (DS-381)
  EXPECT_TRUE(facts.contains("p", "p@m::A"));
  EXPECT_FALSE(facts.contains("p@m::A", "p"));
  ASSERT_NE(facts.parentOf("p@m::A"), nullptr);
  EXPECT_EQ(*facts.parentOf("p@m::A"), "p@m");
  EXPECT_EQ(facts.parentOf("p"), nullptr);
  ASSERT_NE(facts.childrenOf("p@m"), nullptr);
  EXPECT_EQ(*facts.childrenOf("p@m"), (std::vector<std::string>{"p@m::A", "p@m::B", "p@m::r", "p@m::d"}));
  EXPECT_EQ(facts.fieldOf("p@m::A", "name"), "A");
  EXPECT_EQ(facts.fieldOf("p@m::A", "id"), "p@m::A");
  EXPECT_EQ(facts.fieldOf("nothing", "name"), "");
  EXPECT_TRUE(facts.kindIs("p@m::r", Kind::Rule));
  EXPECT_EQ(facts.scopeOf("p@m::A"), "p@m");
  EXPECT_TRUE(facts.holds("p@m::r"));
}

TEST_F(TestFacts, WhatTheEvaluatorRecordsIsReadBack) {
  ds::check::Facts facts(model, metamodel);
  std::string identity;
  EXPECT_FALSE(facts.constant("p@m::r", "A", identity));
  facts.record("p@m::r", "A", "p@m::A");
  facts.record("p@m::r", "call:d", "p@m::d");
  ASSERT_TRUE(facts.constant("p@m::r", "A", identity));
  EXPECT_EQ(identity, "p@m::A");
  EXPECT_EQ(facts.definitionOf("p@m::r", "d"), "p@m::d");
  EXPECT_EQ(facts.derived("p@m::d"), nullptr);
  EXPECT_TRUE(facts.derive("p@m::d", {"p@m::A"}));
  EXPECT_FALSE(facts.derive("p@m::d", {"p@m::A"}));  // already held
  EXPECT_EQ(facts.derived("p@m::d")->size(), 1u);
}

/// While a solve reads the facts they refuse to grow: a binding points into them (DS-381).
TEST_F(TestFacts, WhileASolveReadsTheFactsRefuseToGrow) {
  ds::check::Facts facts(model, metamodel);
  facts.reading();
  EXPECT_THROW(facts.record("p@m::r", "A", "p@m::A"), std::logic_error);
  EXPECT_THROW(facts.derive("p@m::d", {"p@m::A"}), std::logic_error);
  facts.done();
  facts.record("p@m::r", "A", "p@m::A");
  EXPECT_TRUE(facts.derive("p@m::d", {"p@m::A"}));
}

TEST_F(TestFacts, AScopedRangeReadsWhatItsScopeHoldsInTheOrderAndStorageOfItsKind) {
  declare(Kind::Module, "p@n", "p");
  declare(Kind::Class, "p@n::C", "p@n");
  ds::check::Facts facts(model, metamodel);
  facts.index();
  const std::vector<std::string>& all = *facts.ofKind("class");
  ASSERT_EQ(all, (std::vector<std::string>{"p@m::A", "p@m::B", "p@n::C"}));
  const auto& inM = facts.ofKindIn("class", "p@m");
  ASSERT_EQ(inM.size(), 2u);
  EXPECT_EQ(inM[0], &all[0]);  // the very strings ofKind keeps: a binding points where it did (DS-503)
  EXPECT_EQ(inM[1], &all[1]);
  ASSERT_EQ(facts.ofKindIn("class", "p").size(), 3u);  // a package holds its modules' classes
  EXPECT_EQ(&facts.ofKindIn("class", "p@m"), &inM);    // read once, kept
  EXPECT_TRUE(facts.ofKindIn("interface", "p@m").empty());
}

TEST_F(TestFacts, AnArtefactIsInTheBoxItWasWrittenInForAScopedRange) {
  ds::model::Entity req;  // no box holds it by contains: it is in the one it was written in (DS-268)
  req.kind = Kind::Requirement;
  req.id = "REQ-00001-1";
  req.name = "REQ-00001-1";
  req.scope = "p@m";
  model.declare(req);
  ds::check::Facts facts(model, metamodel);
  facts.index();
  const auto& inM = facts.ofKindIn("requirement", "p@m");
  ASSERT_EQ(inM.size(), 1u);
  EXPECT_EQ(*inM[0], "REQ-00001-1");
  EXPECT_EQ(facts.ofKindIn("requirement", "p").size(), 1u);
}

}  // namespace
