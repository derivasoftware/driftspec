#include <gtest/gtest.h>

#include <deque>
#include <map>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The static rules R10–R19 over tests/corpus/static/static.ds, whose `!` comments are the oracle.
class TestStatic : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  std::map<std::string, int> byCode;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/static")) {
      ASSERT_FALSE(f.hasSyntaxErrors) << f.path;
      binder.bind(f);
    }
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    for (const auto& f : model.findings()) ++byCode[f.code];
  }

  std::string field(const std::string& id, const std::string& key) const {
    const ds::model::Entity* e = model.find(id);
    if (!e) return "<no entity " + id + ">";
    auto it = e->fields.find(key);
    return it == e->fields.end() ? "" : it->second;
  }

  mutable std::deque<ds::model::Relation> kept;

  // a copy kept by the fixture: the index is a view made for the asking, and what it lists does not outlive it
  const ds::model::Relation* relation(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().all()) {
      if (r.kind == kind && r.from == from && r.to == to) return &kept.emplace_back(r);
    }
    return nullptr;
  }
};

TEST_F(TestStatic, TheOracleFindingsAndNothingElse) {
  EXPECT_EQ(byCode["E-TEMPLATE-BOUND"], 1);
  EXPECT_EQ(byCode["E-MODIFIER-CONFLICT"], 1);
  EXPECT_EQ(byCode["E-OPERATOR-UNDECLARED"], 1);
  EXPECT_EQ(byCode["E-DATACLASS-ABSTRACT"], 1);
  EXPECT_EQ(byCode["E-OWNERSHIP-CYCLE"], 1);
  EXPECT_EQ(model.findings().size(), 5u);  // Cache, Requirement and std:: nobody designs: text, no finding (R11)
}

TEST_F(TestStatic, ANameNobodyDesignsIsTextAndRelatesNothing) {
  // std::string, std::vector, Cache and Requirement are named and declared nowhere: no entity, no relation (DS-18)
  for (const char* id : {"ds@ds::Cache", "ds@std::vector<1>", "ds@ds::Requirement", "ds@std::string"})
    EXPECT_EQ(model.find(id), nullptr) << id;
  for (const auto& r : model.index().from("ds@ds::Model::cache")) ADD_FAILURE() << r.kind << " " << r.to;
  EXPECT_EQ(field("ds@ds::Model::cache", "type"), "Cache");                                    // the text stays as written
  EXPECT_NE(relation("owns", "ds@ds::Index::requirements", "ds@ds::Repository<1>"), nullptr);  // only what is designed relates
}

TEST_F(TestStatic, ConstraintsAreArtefactsInBothForms) {
  // the revision is part of the identity (block 1): DS-C-0001-0, DS-C-0002-1
  EXPECT_EQ(field("DS-C-0001-0", "title"), "The diagnostics core shall not depend on any I/O library");
  EXPECT_EQ(field("DS-C-0002-1", "title"), "Finding codes are stable");
  EXPECT_NE(relation("about", "DS-C-0001-0", "ds"), nullptr);
  // nested in a class or a member body (R12) says what annotating says (DS-217)
  EXPECT_NE(relation("constrained-by", "ds@ds::Vector", "DS-C-0003-0"), nullptr);
  EXPECT_NE(relation("satisfies", "ds@ds::Model::declare", "DS-0003-0"), nullptr);
  EXPECT_NE(relation("constrained-by", "ds@ds::Model", "DS-C-0001-0"), nullptr);  // a pinned link
}

TEST(TestNesting, NestingAnArtefactInADesignEntityIsAnnotatingIt) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package shop {\n"
                               "  module store {\n"
                               "    requirement REQ-00002-1: The store shall keep its items\n"
                               "    class Cart {\n"
                               "      requirement REQ-00001-1: The cart shall total its items\n"
                               "      - cents: int {\n"
                               "        constraint CST-00001-1: Every price shall be kept in cents\n"
                               "      }\n"
                               "    }\n"
                               "    class Basket {\n"
                               "      @requirement REQ-00002-1\n"
                               "      - cents: int {\n"
                               "        @constraint CST-00001-1\n"
                               "      }\n"
                               "    }\n"
                               "  }\n"
                               "}\n"));
  ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  EXPECT_TRUE(model.findings().empty()) << model.findings().front().message;  // an attribute may annotate, as it may nest
  const auto has = [&model](const std::string& kind, const std::string& from, const std::string& to) {
    for (const auto& r : model.index().from(from))
      if (r.kind == kind && r.to == to) return true;
    return false;
  };
  EXPECT_TRUE(has("satisfies", "shop@store::Cart", "REQ-00001-1"));
  EXPECT_TRUE(has("constrained-by", "shop@store::Cart::cents", "CST-00001-1"));
  EXPECT_TRUE(has("satisfies", "shop@store::Basket", "REQ-00002-1"));
  EXPECT_TRUE(has("constrained-by", "shop@store::Basket::cents", "CST-00001-1"));
  EXPECT_TRUE(has("about", "REQ-00002-1", "shop@store"));  // in a box it still gives its level
}

TEST_F(TestStatic, NotesLiveInMemberBodies) {
  EXPECT_EQ(field("ds@ds::ScopedCheck::run", "note"), "walks the scopes and delegates each one to runScope");
  EXPECT_EQ(field("ds@ds::ScopedCheck::run", "contract.0"), "");  // no prose contract: what holds is a rule (DS-94)
}

TEST_F(TestStatic, AbstractAndValueAreDerived) {
  EXPECT_EQ(field("ds@ds::Check", "stereotypes"), "abstract");  // every method abstract: still an abstract class (DS-301)
  EXPECT_EQ(field("ds@ds::ScopedCheck", "stereotypes"), "abstract");
  EXPECT_EQ(field("ds@ds::Model", "stereotypes"), "");
  EXPECT_EQ(field("ds@ds::Location", "stereotypes"), "value, equatable, ordered, printable");
  EXPECT_EQ(field("ds@ds::Finding", "stereotypes"), "value, equatable, hashable");
}

TEST_F(TestStatic, EnumsAndTemplatesHaveIdentities) {
  EXPECT_EQ(field("ds@ds::Severity", "values"), "Error, Warning, Info");
  EXPECT_EQ(field("ds@ds::Repository<1>", "params"), "T");
  EXPECT_EQ(field("ds@ds::Repository<1>", "bound.T"), "Entity");
  EXPECT_NE(relation("bounded-by", "ds@ds::Repository<1>", "ds@ds::Entity"), nullptr);
  EXPECT_NE(relation("owns", "ds@ds::Index::relations", "ds@ds::Repository<1>"), nullptr);
  EXPECT_NE(relation("owns", "ds@ds::Index::relations", "ds@ds::Relation"), nullptr);  // every designed name of the type, owned
}

TEST_F(TestStatic, OwnershipAndMultiplicityComeFromTheAttribute) {
  const auto* location = relation("owns", "ds@ds::Finding::location", "ds@ds::Location");
  ASSERT_NE(location, nullptr);
  EXPECT_EQ(location->multiplicity, "1");
  const auto* scopes = relation("refs", "ds@ds::ScopedCheck::scopes", "ds@ds::Scope");
  ASSERT_NE(scopes, nullptr);
  EXPECT_EQ(scopes->multiplicity, "*");
  const auto* entities = relation("owns", "ds@ds::Model::entities", "ds@ds::Entity");
  ASSERT_NE(entities, nullptr);
  EXPECT_EQ(entities->multiplicity, "*");
  const auto* found = relation("uses", "ds@ds::Model::find", "ds@ds::Entity");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->multiplicity, "");
  EXPECT_NE(relation("uses", "ds@ds::ScopedCheck::runScope", "ds@ds::Model"), nullptr);  // parameters give uses
  EXPECT_TRUE(model.find("ds@ds::ScopedCheck::scopes")->has("many"));                    // written, never read from std::vector
  EXPECT_FALSE(model.find("ds@ds::Finding::location")->has("many"));
}

TEST_F(TestStatic, OperatorsAreDeclaredInTheHeaderAndRefinedInTheBody) {
  EXPECT_EQ(field("ds@ds::Vector", "operators"), "==, +, *, []");
  ASSERT_NE(model.find("ds@ds::Vector::operator+"), nullptr);
  EXPECT_EQ(field("ds@ds::Vector::operator*", "params"), "k: float");
  EXPECT_EQ(field("ds@ds::Vector::operator*", "returns"), "Vector");
  ASSERT_NE(model.find("ds@ds::Vector::Vector"), nullptr);
  EXPECT_TRUE(model.find("ds@ds::Vector::Vector")->has("constructor"));
}

TEST_F(TestStatic, CrossModuleReferencesResolveByTheModuleOrByThePackage) {
  const auto* model_ = relation("refs", "ds@lsp::Publisher::model", "ds@ds::Model");
  ASSERT_NE(model_, nullptr);
  EXPECT_NE(relation("owns", "ds@lsp::Publisher::last", "ds@ds::Finding"), nullptr);
  // a bare Finding no scope of lsp declares is the one Finding of the package, as the code imports it (DS-18)
  EXPECT_NE(relation("uses", "ds@lsp::Publisher::publish", "ds@ds::Finding"), nullptr);
  EXPECT_EQ(model.find("ds@lsp::Finding"), nullptr);
}

TEST_F(TestStatic, DefaultsAndModifiersAreKept) {
  EXPECT_EQ(field("ds@ds::Location::column", "default"), "0");
  EXPECT_TRUE(model.find("ds@ds::Stack<1>::of")->has("static"));
  EXPECT_TRUE(model.find("ds@ds::Stack<1>::size")->has("const"));
  EXPECT_TRUE(model.find("ds@ds::Check::run")->has("abstract"));
  EXPECT_TRUE(model.find("ds@ds::Finding::location")->has("owns"));
}

}  // namespace
