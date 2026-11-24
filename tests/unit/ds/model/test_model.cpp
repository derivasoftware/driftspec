#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "ds/model/Metamodel.hpp"
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
  const ds::model::Metamodel metamodel;
  Model model{metamodel};

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
  const Entity& declared = model.declare(entity(Kind::Class, "p@m::A", "p@m"));
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
  Model fresh{metamodel};
  EXPECT_THROW(fresh.relate({"depends-on", "p@m::A", "p@m::B", {}, ""}), std::logic_error);
  EXPECT_EQ(fresh.index().count(), 0u);
  fresh.relate({"uses", "p@m::A", "p@m::B", {}, ""});
  EXPECT_EQ(fresh.index().count(), 1u);
}

TEST_F(TestModel, WhatIsNotYetWrittenLeavesTheFindingsForTheCoverage) {
  model.report({"W-REQ-NO-VERIFICATION", "warning", "first", {"a.ds", 3, 1}});
  model.report({"E-DUPLICATE-IDENTITY", "error", "kept", {"a.ds", 2, 1}});
  model.report({"W-MESSAGE-NO-OPERATION", "coverage", "second", {"a.ds", 1, 1}});
  model.cover();
  ASSERT_EQ(model.findings().size(), 1u);
  EXPECT_EQ(model.findings().front().message, "kept");
  ASSERT_EQ(model.coverage().size(), 2u);  // in the order reported, each marked as coverage (DS-216)
  EXPECT_EQ(model.coverage()[0].message, "first");
  EXPECT_EQ(model.coverage()[0].severity, "coverage");
  EXPECT_EQ(model.coverage()[1].message, "second");
}

TEST_F(TestModel, AFileThatDoesNotParseTakesTheFindingsThatOnlyEchoIt) {
  model.report({"E-SYNTAX", "error", "the file does not parse", {"cut.ds", 4, 1}});
  model.report({"W-FIELD-UNKNOWN", "warning", "in the cut file", {"cut.ds", 9, 1}});
  model.report({"W-REF-UNDECLARED", "warning", "declared nowhere now", {"whole.ds", 2, 1}});
  model.report({"W-FIELD-UNKNOWN", "warning", "in a whole file", {"whole.ds", 1, 1}});
  model.withdrawEchoes({});  // every file parses: nothing echoes
  EXPECT_EQ(model.findings().size(), 4u);
  model.withdrawEchoes({"cut.ds"});
  ASSERT_EQ(model.findings().size(), 2u);
  EXPECT_EQ(model.findings()[0].code, "E-SYNTAX");
  EXPECT_EQ(model.findings()[1].message, "in a whole file");
}

TEST_F(TestModel, TheFindingsAreSortedByFileThenLineAndKeepTheirOrderOnALine) {
  model.report({"W-B", "warning", "b2", {"b.ds", 2, 1}});
  model.report({"W-A", "warning", "a5 first", {"a.ds", 5, 9}});
  model.report({"W-A", "warning", "a1", {"a.ds", 1, 1}});
  model.report({"W-A", "warning", "a5 second", {"a.ds", 5, 1}});
  model.sortFindings();
  std::vector<std::string> order;
  for (const auto& f : model.findings()) order.push_back(f.message);
  EXPECT_EQ(order, (std::vector<std::string>{"a1", "a5 first", "a5 second", "b2"}));
}

TEST_F(TestModel, ATableIsKeptByItsDiagramInPlaceOfTheOneBefore) {
  ds::model::Tabulated before;
  before.columns = {"old"};
  model.tabulate("diagram:T", before);
  ds::model::Tabulated table;
  table.columns = {"name"};
  table.rows = {{"A"}};
  table.kinds = {{""}};
  model.tabulate("diagram:T", table);
  ASSERT_EQ(model.tables().size(), 1u);
  EXPECT_EQ(model.tables().at("diagram:T").columns, (std::vector<std::string>{"name"}));
  EXPECT_EQ(model.tables().at("diagram:T").rows.front().front(), "A");
}

TEST_F(TestModel, AnAnnotationWritesAFieldAndNothingAnEntityIsIndexedBy) {
  model.declare(entity(Kind::Class, "p@m::A", "p@m"));
  model.annotate("p@m::A", "stereotypes", "value");
  model.annotate("p@m::A", "stereotypes", "value, equatable");  // written again: the last value stays
  const Entity* a = model.find("p@m::A");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->field("stereotypes"), "value, equatable");
  EXPECT_EQ(model.index().members("p@m"), (std::vector<std::string>{"p@m::A"}));
  EXPECT_THROW(model.annotate("p@m::Nobody", "type", "x"), std::logic_error);  // a defect of ds, not of the design
}

TEST_F(TestModel, ARefinementJoinsFieldsAndFlagsAndMovesWhereTheEntityIsWritten) {
  Entity header = entity(Kind::Operator, "p@m::A::operator==", "p@m::A");
  header.fields["symbol"] = "==";
  model.declare(header);
  Entity body = header;
  body.fields["returns"] = "bool";
  body.flags = {"const"};
  body.params = {{"other", "A"}};
  body.location = {"a.ds", 7, 3};
  model.refine(body);
  const Entity* op = model.find("p@m::A::operator==");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->field("symbol"), "==");
  EXPECT_EQ(op->field("returns"), "bool");
  EXPECT_TRUE(op->has("const"));
  ASSERT_EQ(op->params.size(), 1u);
  EXPECT_EQ(op->location.line, 7);
  body.params.clear();
  model.refine(body);  // a refinement with no parameters leaves those held
  EXPECT_EQ(model.find("p@m::A::operator==")->params.size(), 1u);
}

TEST_F(TestModel, ARenameCarriesTheFieldsTheMetamodelSaysHoldAnIdentity) {
  model.declare(entity(Kind::Class, "p@m::A", "p@m"));
  model.declare(entity(Kind::Sequence, "p@m::A::run::sequence", "p@m::A"));
  Entity m = entity(Kind::Message, "p@m::B::go::sequence::m0", "p@m::B::go::sequence");
  m.fields["expands"] = "p@m::A::run::sequence";
  m.fields["subject"] = "p@m::A::run::sequence";  // a name as written: no identity the rename carries
  model.declare(m);
  ASSERT_TRUE(model.rename("p@m::A", "p@m::Z"));
  EXPECT_EQ(model.find("p@m::B::go::sequence::m0")->field("expands"), "p@m::Z::run::sequence");
  EXPECT_EQ(model.find("p@m::B::go::sequence::m0")->field("subject"), "p@m::A::run::sequence");
  EXPECT_EQ(metamodel.identityFields(), (std::vector<std::string>{"expands"}));
}

/// An entity knows the flags it was declared with, and reads a field written as a number.
class TestEntity : public ::testing::Test {};

TEST_F(TestEntity, HasAFlagItWasDeclaredWith) {
  Entity e = entity(Kind::Class, "p@m::A", "p@m");
  e.flags = {"abstract", "dataclass"};
  EXPECT_TRUE(e.has("abstract"));
  EXPECT_FALSE(e.has("many"));
}

TEST_F(TestEntity, ANumberFieldIsReadWholeUpToTheLargestInt) {
  Entity e = entity(Kind::Diagram, "diagram:SEQ_A");
  int n = -1;
  e.fields["depth"] = "2147483647";
  EXPECT_TRUE(e.number("depth", n));
  EXPECT_EQ(n, 2147483647);
  e.fields["depth"] = "007";  // a leading zero is still the number
  EXPECT_TRUE(e.number("depth", n));
  EXPECT_EQ(n, 7);
  e.fields["depth"] = "0";
  EXPECT_TRUE(e.number("depth", n));
  EXPECT_EQ(n, 0);
}

TEST_F(TestEntity, ANumberTooLargeForAnIntIsNoNumberAndNeverAnAbort) {  // DS-337: std::stoi threw out_of_range
  Entity e = entity(Kind::Diagram, "diagram:SEQ_A");
  int n = -1;
  for (const char* written : {"2147483648", "99999999999", "000000000000000000002147483648"}) {
    e.fields["depth"] = written;
    EXPECT_FALSE(e.number("depth", n)) << written;
    EXPECT_EQ(n, -1) << written;  // left as it was
  }
}

TEST_F(TestEntity, AnEmptyUnwrittenOrSignedFieldIsNoNumber) {
  Entity e = entity(Kind::Diagram, "diagram:SEQ_A");
  int n = -1;
  EXPECT_FALSE(e.number("depth", n));  // not written
  for (const char* written : {"", "lots", "*", "-1", "+1", " 3", "3 ", "1.5"}) {
    e.fields["depth"] = written;
    EXPECT_FALSE(e.number("depth", n)) << "'" << written << "'";
  }
  EXPECT_EQ(n, -1);
}

/// A parameter is kept as the binder read it, and a renamed entity carries its parameters and arguments.
class TestParam : public ::testing::Test {};

TEST_F(TestParam, ARenamedEntityCarriesItsParametersAndArguments) {
  const ds::model::Metamodel metamodel;
  Model model{metamodel};
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
  const ds::model::Metamodel metamodel;
  for (Kind k : {Kind::Requirement, Kind::Verification, Kind::Constraint, Kind::UseCase})
    EXPECT_TRUE(metamodel.isArtefact(k)) << metamodel.nameOf(k);
  for (Kind k : {Kind::Package, Kind::Module, Kind::Class, Kind::Interface, Kind::Rule, Kind::Outcome})
    EXPECT_FALSE(metamodel.isArtefact(k)) << metamodel.nameOf(k);
}

}  // namespace
