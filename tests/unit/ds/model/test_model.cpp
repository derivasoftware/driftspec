#include <gtest/gtest.h>

#include <stdexcept>

#include "ds/model/Model.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;
using ds::model::Model;
using ds::model::Relation;

Entity entity(Kind kind, const std::string& id, const std::string& scope = "", bool expected = false) {
  Entity e;
  e.kind = kind;
  e.id = id;
  e.name = id.substr(id.find_last_of(":@") == std::string::npos ? 0 : id.find_last_of(":@") + 1);
  e.scope = scope;
  e.expected = expected;
  e.location = {"a.ds", 1, 1};
  return e;
}

/// The resolved workspace: one entity per identity, relations and findings (REQ-00001-1, REQ-00002-1).
class TestModel : public ::testing::Test {
 protected:
  Model model;

  int findings(const std::string& code) const {
    int n = 0;
    for (const auto& f : model.findings()) n += f.code == code;
    return n;
  }
};

TEST_F(TestModel, ADeclarationTakesThePlaceOfTheExpectedNode) {
  model.declare(entity(Kind::Class, "p@m::A", "p@m", true));  // referenced first
  EXPECT_TRUE(model.find("p@m::A")->expected);
  model.declare(entity(Kind::Class, "p@m::A", "p@m", true));  // referenced again: still one node
  Entity& declared = model.declare(entity(Kind::Class, "p@m::A", "p@m"));
  EXPECT_FALSE(declared.expected);
  EXPECT_EQ(model.entities().size(), 1u);
  EXPECT_EQ(findings("E-DUPLICATE-IDENTITY"), 0);
}

TEST_F(TestModel, ASecondDeclarationIsReportedExceptForReopenableScopes) {
  model.declare(entity(Kind::Module, "p@m", "p@"));
  model.declare(entity(Kind::Module, "p@m", "p@"));  // a module reopens
  EXPECT_EQ(findings("E-DUPLICATE-IDENTITY"), 0);
  model.declare(entity(Kind::Class, "p@m::A", "p@m"));
  model.declare(entity(Kind::Class, "p@m::A", "p@m"));  // a class does not
  EXPECT_EQ(findings("E-DUPLICATE-IDENTITY"), 1);
  EXPECT_NE(model.findings().back().message.find("first at a.ds:1"), std::string::npos);
}

TEST_F(TestModel, ARenameCarriesMembersAndRelations) {
  model.declare(entity(Kind::Sequence, "p@m::run::sequence", "p@m"));
  model.declare(entity(Kind::Message, "p@m::run::sequence::m0", "p@m::run::sequence"));
  model.relate({"behaviour-of", "p@m::run::sequence", "p@m::A::run", {}, ""});
  ASSERT_TRUE(model.rename("p@m::run::sequence", "p@m::A::run::sequence"));
  EXPECT_EQ(model.find("p@m::run::sequence"), nullptr);
  ASSERT_NE(model.find("p@m::A::run::sequence::m0"), nullptr);
  EXPECT_EQ(model.find("p@m::A::run::sequence::m0")->scope, "p@m::A::run::sequence");
  EXPECT_EQ(model.index().all().front().from, "p@m::A::run::sequence");
  EXPECT_FALSE(model.rename("nobody", "x"));  // nothing to rename
}

TEST_F(TestModel, ARenameOntoADeclaredIdentityIsRefused) {
  model.declare(entity(Kind::Class, "p@m::A", "p@m"));
  model.declare(entity(Kind::Class, "p@m::B", "p@m"));
  EXPECT_FALSE(model.rename("p@m::A", "p@m::B"));
  EXPECT_EQ(findings("E-DUPLICATE-IDENTITY"), 1);
  model.declare(entity(Kind::Class, "p@m::C", "p@m", true));  // an expected node gives way
  EXPECT_TRUE(model.rename("p@m::A", "p@m::C"));
  EXPECT_FALSE(model.find("p@m::C")->expected);
}

TEST_F(TestModel, ARelationTheMetamodelDoesNotDeclareIsRefused) {
  Model fresh;
  EXPECT_THROW(fresh.relate({"depends-on", "p@m::A", "p@m::B", {}, ""}), std::logic_error);
  EXPECT_EQ(fresh.index().count(), 0u);
  fresh.relate({"uses", "p@m::A", "p@m::B", {}, ""});
  EXPECT_EQ(fresh.index().count(), 1u);
}

/// An entity knows the flags it was declared with.
class TestEntity : public ::testing::Test {};

TEST_F(TestEntity, HasAFlagItWasDeclaredWith) {
  Entity e = entity(Kind::Class, "p@m::A", "p@m");
  e.flags = {"abstract", "dataclass"};
  EXPECT_TRUE(e.has("abstract"));
  EXPECT_FALSE(e.has("extern"));
}

/// A parameter is kept as the binder read it, and a renamed entity carries its parameters and arguments.
class TestParam : public ::testing::Test {};

TEST_F(TestParam, ARenamedEntityCarriesItsParametersAndArguments) {
  Model model;
  Entity e = entity(Kind::Function, "p@m::f", "p@m");
  e.params = {{"name", "std::string"}, {"items", "std::vector<int>"}};
  e.args = {"a", "\"b, c\""};
  model.declare(e);
  ASSERT_TRUE(model.rename("p@m::f", "p@m::g"));
  const Entity* g = model.find("p@m::g");
  ASSERT_NE(g, nullptr);
  ASSERT_EQ(g->params.size(), 2u);
  EXPECT_EQ(g->params[1].name, "items");
  EXPECT_EQ(g->params[1].type, "std::vector<int>");
  ASSERT_EQ(g->args.size(), 2u);
  EXPECT_EQ(g->args[1], "\"b, c\"");  // an argument is one value even when its text holds a comma
}

}  // namespace

namespace {

TEST(TestIsArtefact, TheKindsIdentifiedByTheirRevisionAreTheArtefactsAndTheUseCase) {
  for (Kind k : {Kind::Requirement, Kind::Verification, Kind::Constraint, Kind::UseCase})
    EXPECT_TRUE(ds::model::isArtefact(k)) << ds::model::kindName(k);
  for (Kind k : {Kind::Package, Kind::Module, Kind::Class, Kind::Interface, Kind::Rule, Kind::Outcome})
    EXPECT_FALSE(ds::model::isArtefact(k)) << ds::model::kindName(k);
}

}  // namespace
