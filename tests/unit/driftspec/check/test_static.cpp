#include <gtest/gtest.h>

#include <map>

#include "driftspec/bind/Binder.hpp"
#include "driftspec/check/Checker.hpp"
#include "driftspec/model/Model.hpp"
#include "driftspec/parse/Parser.hpp"

namespace {

/// The static rules R10–R19 over examples/static/static.ds, whose `!` comments are the oracle.
class TestStatic : public ::testing::Test {
 protected:
  driftspec::model::Model model;
  std::map<std::string, int> byCode;

  void SetUp() override {
    driftspec::parse::Parser parser;
    driftspec::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/static")) {
      ASSERT_FALSE(f.hasSyntaxErrors) << f.path;
      binder.bind(f);
    }
    driftspec::check::Checker(model).run(binder);
    for (const auto& f : model.findings()) ++byCode[f.code];
  }

  std::string field(const std::string& id, const std::string& key) const {
    const driftspec::model::Entity* e = model.find(id);
    if (!e) return "<no entity " + id + ">";
    auto it = e->fields.find(key);
    return it == e->fields.end() ? "" : it->second;
  }

  const driftspec::model::Relation* relation(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.relations()) {
      if (r.kind == kind && r.from == from && r.to == to) return &r;
    }
    return nullptr;
  }
};

TEST_F(TestStatic, TheOracleFindingsAndNothingElse) {
  EXPECT_EQ(byCode["W-REF-UNDECLARED"], 3);  // Cache, Requirement, bare Finding from lsp
  EXPECT_EQ(byCode["E-TEMPLATE-BOUND"], 1);
  EXPECT_EQ(byCode["W-MODIFIER-ON-VALUE"], 1);
  EXPECT_EQ(byCode["E-OPERATOR-UNDECLARED"], 1);
  EXPECT_EQ(byCode["E-DATACLASS-ABSTRACT"], 1);
  EXPECT_EQ(byCode["E-OWNERSHIP-CYCLE"], 1);
  EXPECT_EQ(model.findings().size(), 8u);
}

TEST_F(TestStatic, ExternDeclaresIdentitiesWithoutObligations) {
  const driftspec::model::Entity* vec = model.find("std::vector<1>");
  ASSERT_NE(vec, nullptr);
  EXPECT_TRUE(vec->has("extern"));
  EXPECT_EQ(field("std::vector<1>", "stereotypes"), "extern");
}

TEST_F(TestStatic, ConstraintsAreArtefactsInBothForms) {
  // the revision is part of the identity (block 1): DS-C-0001-0, DS-C-0002-1
  EXPECT_EQ(field("DS-C-0001-0", "title"), "The diagnostics core shall not depend on any I/O library");
  EXPECT_EQ(field("DS-C-0002-1", "title"), "Finding codes are stable");
  EXPECT_NE(relation("about", "DS-C-0001-0", "package:ds"), nullptr);
  EXPECT_NE(relation("about", "DS-C-0003-0", "ds::Vector"), nullptr);         // nested in a class
  EXPECT_NE(relation("about", "DS-0003-0", "ds::Model::declare"), nullptr);   // requirement nested in a member body (R12)
  EXPECT_NE(relation("constrained-by", "ds::Model", "DS-C-0001-0"), nullptr);  // a pinned link
}

TEST_F(TestStatic, NotesAndContractsLiveInMemberBodies) {
  EXPECT_EQ(field("ds::ScopedCheck::run", "note"), "walks the scopes and delegates each one to runScope");
  EXPECT_EQ(field("ds::ScopedCheck::run", "contract.0"), "ensures: result sorted by location");
  EXPECT_EQ(field("ds::Model::declare", "contract.1"), "ensures: find(entity.id) == entity");
}

TEST_F(TestStatic, InterfaceAndAbstractAreDerived) {
  EXPECT_EQ(field("ds::Check", "stereotypes"), "interface");
  EXPECT_EQ(field("ds::ScopedCheck", "stereotypes"), "abstract");
  EXPECT_EQ(field("ds::Model", "stereotypes"), "");
  EXPECT_EQ(field("ds::Location", "stereotypes"), "value, equatable, ordered, printable");
  EXPECT_EQ(field("ds::Finding", "stereotypes"), "value, equatable, hashable");
}

TEST_F(TestStatic, EnumsAndTemplatesHaveIdentities) {
  EXPECT_EQ(field("ds::Severity", "values"), "Error, Warning, Info");
  EXPECT_EQ(field("ds::Repository<1>", "params"), "T");
  EXPECT_EQ(field("ds::Repository<1>", "bound.T"), "Entity");
  EXPECT_NE(relation("bounded-by", "ds::Repository<1>", "ds::Entity"), nullptr);
  EXPECT_NE(relation("owns", "ds::Index::relations", "ds::Repository<1>"), nullptr);
  EXPECT_NE(relation("uses", "ds::Index::relations", "ds::Relation"), nullptr);
}

TEST_F(TestStatic, OwnershipAndMultiplicityComeFromTheAttribute) {
  const auto* location = relation("owns", "ds::Finding::location", "ds::Location");
  ASSERT_NE(location, nullptr);
  EXPECT_EQ(location->multiplicity, "1");
  const auto* scopes = relation("refs", "ds::ScopedCheck::scopes", "ds::Scope");
  ASSERT_NE(scopes, nullptr);
  EXPECT_EQ(scopes->multiplicity, "*");
  const auto* entities = relation("owns", "ds::Model::entities", "ds::Entity");
  ASSERT_NE(entities, nullptr);
  EXPECT_EQ(entities->multiplicity, "*");
  const auto* found = relation("uses", "ds::Model::find", "ds::Entity");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->multiplicity, "");
  EXPECT_NE(relation("shares", "ds::Model::cache", "ds::Cache"), nullptr);
  EXPECT_NE(relation("uses", "ds::ScopedCheck::runScope", "ds::Model"), nullptr);  // parameters give uses
}

TEST_F(TestStatic, OperatorsAreDeclaredInTheHeaderAndRefinedInTheBody) {
  EXPECT_EQ(field("ds::Vector", "operators"), "==, +, *, []");
  ASSERT_NE(model.find("ds::Vector::operator+"), nullptr);
  EXPECT_EQ(field("ds::Vector::operator*", "params"), "k: float");
  EXPECT_EQ(field("ds::Vector::operator*", "returns"), "Vector");
  ASSERT_NE(model.find("ds::Vector::Vector"), nullptr);
  EXPECT_TRUE(model.find("ds::Vector::Vector")->has("constructor"));
}

TEST_F(TestStatic, CrossModuleReferencesNeedTheModuleNotAnInclude) {
  const auto* model_ = relation("refs", "lsp::Publisher::model", "ds::Model");
  ASSERT_NE(model_, nullptr);
  EXPECT_NE(relation("owns", "lsp::Publisher::last", "ds::Finding"), nullptr);
  const driftspec::model::Entity* bare = model.find("lsp::Finding");
  ASSERT_NE(bare, nullptr);
  EXPECT_TRUE(bare->expected);
}

TEST_F(TestStatic, DefaultsAndModifiersAreKept) {
  EXPECT_EQ(field("ds::Location::column", "default"), "0");
  EXPECT_TRUE(model.find("ds::Stack<1>::of")->has("static"));
  EXPECT_TRUE(model.find("ds::Stack<1>::size")->has("const"));
  EXPECT_TRUE(model.find("ds::Check::run")->has("abstract"));
  EXPECT_TRUE(model.find("ds::Finding::location")->has("owns"));
}

}  // namespace
