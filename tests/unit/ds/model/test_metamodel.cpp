#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Metamodel.hpp"

namespace {

using ds::model::Kind;

const Kind kAllKinds[] = {Kind::Package,   Kind::Module,     Kind::Requirement, Kind::Verification, Kind::Interface,  Kind::Operation,
                          Kind::View,      Kind::Constraint, Kind::Class,       Kind::Enum,         Kind::Function,   Kind::Method,
                          Kind::Attribute, Kind::Operator,   Kind::Sequence,    Kind::Message,      Kind::Frame,      Kind::State,
                          Kind::Diagram,   Kind::UseCase,    Kind::Outcome,     Kind::Rule,         Kind::Definition, Kind::Step};

/// One metamodel, made for each test as the engine makes one where it starts (DS-341).
class Metamodelled : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
};

/// The metamodel declared once (REQ-00066-1). One fixture per unit of ds::model it tests:
/// the kinds, the fields, the relations and joins.
class TestKindSpec : public Metamodelled {};
class TestFieldSpec : public Metamodelled {};
class TestRelationSpec : public Metamodelled {};
class TestJoins : public Metamodelled {};

TEST_F(TestKindSpec, EachKindIsPartOfTheCategoryTheOwnerGaveIt) {
  // the Owner's five (DS-427, 2026-11-25): what the design is made of, what it answers to, what it does, what judges
  // it, what shows it
  const std::map<std::string, std::string> owner = {
      {"package", "structure"},   {"module", "structure"},   {"interface", "structure"},  {"operation", "structure"},
      {"class", "structure"},     {"enum", "structure"},     {"function", "structure"},   {"method", "structure"},
      {"attribute", "structure"}, {"operator", "structure"}, {"requirement", "artefact"}, {"verification", "artefact"},
      {"constraint", "artefact"}, {"usecase", "artefact"},   {"outcome", "artefact"},     {"sequence", "behaviour"},
      {"message", "behaviour"},   {"frame", "behaviour"},    {"view", "behaviour"},       {"rule", "logic"},
      {"definition", "logic"},    {"state", "logic"},        {"diagram", "view"},         {"step", "view"}};
  ASSERT_EQ(metamodel.kinds().size(), owner.size());
  for (const auto& k : metamodel.kinds()) {
    const std::string name = metamodel.nameOf(k.kind);
    ASSERT_TRUE(owner.count(name)) << name;
    EXPECT_EQ(ds::model::Metamodel::categoryName(k.category), owner.at(name)) << name;
    EXPECT_EQ(k.category, ds::model::Metamodel::categoryOf(k.kind)) << name;
  }
}

TEST_F(TestKindSpec, EveryKindOfTheModelIsDeclaredOnce) {
  std::set<std::string> seen;
  for (const auto& k : metamodel.kinds()) EXPECT_TRUE(seen.insert(metamodel.nameOf(k.kind)).second) << metamodel.nameOf(k.kind);
  for (Kind k : kAllKinds) EXPECT_EQ(metamodel.specOf(k).kind, k) << metamodel.nameOf(k);
  EXPECT_EQ(seen.size(), sizeof(kAllKinds) / sizeof(kAllKinds[0]));
}

TEST_F(TestFieldSpec, AFieldIsFoundByItsKindAndAFamilyByAnySuffix) {
  const auto* title = metamodel.fieldOf(Kind::Requirement, "title");
  ASSERT_NE(title, nullptr);
  EXPECT_EQ(title->source, ds::model::FieldSource::Written);
  EXPECT_NE(metamodel.fieldOf(Kind::Sequence, "local.parser"), nullptr);
  EXPECT_NE(metamodel.fieldOf(Kind::Operation, "exit 2"), nullptr);
  EXPECT_EQ(metamodel.fieldOf(Kind::Sequence, "local."), nullptr);  // a family needs a suffix
  EXPECT_EQ(metamodel.fieldOf(Kind::Diagram, "layout"), nullptr);
  EXPECT_EQ(metamodel.fieldOf(Kind::Class, "title"), nullptr);  // a field of another kind is not this kind's
}

TEST_F(TestRelationSpec, ARelationIsFoundByItsKindAndAnUnknownOneIsNot) {
  const auto* verifies = metamodel.relationOf("verifies");
  ASSERT_NE(verifies, nullptr);
  EXPECT_EQ(verifies->from.size(), 1u);
  EXPECT_EQ(verifies->to.size(), 2u);
  EXPECT_EQ(metamodel.relationOf("depends-on"), nullptr);
}

TEST_F(TestJoins, ARelationJoinsOnlyTheKindsItDeclares) {
  const auto* verifies = metamodel.relationOf("verifies");
  ASSERT_NE(verifies, nullptr);
  EXPECT_TRUE(metamodel.joins(*verifies, Kind::Verification, Kind::Requirement));
  EXPECT_TRUE(metamodel.joins(*verifies, Kind::Verification, Kind::Constraint));
  EXPECT_FALSE(metamodel.joins(*verifies, Kind::Requirement, Kind::Verification));
}

TEST_F(TestJoins, ContainsJoinsAParentOnlyWithTheChildrenItMayHold) {
  const auto* contains = metamodel.relationOf("contains");
  ASSERT_NE(contains, nullptr);
  EXPECT_TRUE(metamodel.joins(*contains, Kind::Class, Kind::Method));
  EXPECT_TRUE(metamodel.joins(*contains, Kind::Package, Kind::Package));
  EXPECT_FALSE(metamodel.joins(*contains, Kind::Class, Kind::Package));  // both ends are kinds contains knows, not as a pair
  EXPECT_FALSE(metamodel.joins(*contains, Kind::Sequence, Kind::Method));
}

TEST_F(TestJoins, ARelationWithNoEndsJoinsAnyKind) {
  const auto* refers = metamodel.relationOf("refers");
  ASSERT_NE(refers, nullptr);
  EXPECT_TRUE(metamodel.joins(*refers, Kind::Class, Kind::Diagram));
}

TEST_F(TestKindSpec, OnlyPackagesAndDiagramsStandAtTheTop) {
  for (const auto& k : metamodel.kinds()) {
    EXPECT_EQ(k.top, k.kind == Kind::Package || k.kind == Kind::Diagram) << metamodel.nameOf(k.kind);
    if (!k.top) {
      EXPECT_FALSE(k.parents.empty()) << metamodel.nameOf(k.kind);
    }
  }
}

/// The tables the manual reads (REQ-00079-2): a code and an annotation, each found by its name. A command is the
/// manual's (DS-342): tests/unit/ds/model/test_manual_pages.cpp.
class TestCodeSpec : public Metamodelled {};
class TestAnnotationSpec : public Metamodelled {};

TEST_F(TestCodeSpec, ACodeIsFoundWithItsSeverityItsMeaningAndWhatToDo) {
  const auto* code = metamodel.codeOf("E-SYNTAX");
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(code->severity, ds::model::Severity::error);
  EXPECT_FALSE(code->fix.empty());
  EXPECT_EQ(metamodel.codeOf("E-NOTHING"), nullptr);
  EXPECT_EQ(&metamodel.rowOf(ds::model::Code::E_SYNTAX), code);  // by its slot, the same row as by its name (DS-379)
}

/// The catalogue holds one row per Code, in its order, each printed as its value is named (DS-379).
TEST_F(TestCodeSpec, EveryCodeHasItsRowInTheOrderOfCode) {
  ASSERT_EQ(metamodel.codes().size(), static_cast<std::size_t>(ds::model::Code::W_OUTCOME_FAILURE_TWICE) + 1);
  for (std::size_t i = 0; i < metamodel.codes().size(); ++i) {
    const auto& c = metamodel.codes()[i];
    EXPECT_EQ(static_cast<std::size_t>(c.id), i) << c.code;
    EXPECT_EQ(&metamodel.rowOf(c.id), &c) << c.code;
  }
}

/// A finding is made from its code: the printed code and the severity come from the catalogue (DS-379).
TEST_F(TestCodeSpec, AFindingIsMadeFromItsCode) {
  const ds::model::Finding f = metamodel.finding(ds::model::Code::W_REQ_NO_VERIFICATION, "said", {"a.ds", 3, 5});
  EXPECT_EQ(f.code, "W-REQ-NO-VERIFICATION");
  EXPECT_EQ(f.severity, ds::model::Severity::coverage);
  EXPECT_EQ(f.message, "said");
  EXPECT_EQ(f.location.line, 3);
  EXPECT_EQ(metamodel.finding(ds::model::Code::E_SYNTAX, "", {}).severity, ds::model::Severity::error);
}

/// What each severity means is a row, in the order of Severity: its name, whether it fails the check, whether it is
/// kept apart, and what an editor shows it as (DS-379).
TEST_F(TestCodeSpec, EverySeverityIsARowThatSaysWhatItMeans) {
  ASSERT_EQ(metamodel.severities().size(), 3u);
  for (std::size_t i = 0; i < metamodel.severities().size(); ++i) EXPECT_EQ(static_cast<std::size_t>(metamodel.severities()[i].id), i);
  const auto& error = metamodel.severityOf(ds::model::Severity::error);
  const auto& warning = metamodel.severityOf(ds::model::Severity::warning);
  const auto& coverage = metamodel.severityOf(ds::model::Severity::coverage);
  EXPECT_EQ(error.name, "error");
  EXPECT_EQ(warning.name, "warning");
  EXPECT_EQ(coverage.name, "coverage");
  EXPECT_TRUE(error.fails);
  EXPECT_FALSE(warning.fails || coverage.fails);
  EXPECT_TRUE(coverage.keptApart);
  EXPECT_FALSE(error.keptApart || warning.keptApart);
  EXPECT_EQ(error.editor, "error");
  EXPECT_EQ(warning.editor, "warning");
  EXPECT_EQ(coverage.editor, "information");
}

TEST_F(TestAnnotationSpec, EveryAnnotationThatMeansSomethingNamesTheRelationItMakes) {
  for (const auto& a : metamodel.annotations()) {
    if (a.relation == "refers") continue;  // any other word: a link with no meaning
    EXPECT_NE(metamodel.relationOf(a.relation), nullptr) << a.name;
  }
}

/// How a kind is written and its example, the metamodel's for the manual (REQ-00079-2), one fixture each.
class TestSyntaxOf : public Metamodelled {};
class TestExampleOf : public Metamodelled {};

TEST_F(TestSyntaxOf, AKindSaysHowItIsWritten) {
  EXPECT_NE(metamodel.syntaxOf(Kind::Sequence).find("sequence <Class>::<method>"), std::string::npos);
  EXPECT_EQ(metamodel.specOf(Kind::Sequence).syntax, metamodel.syntaxOf(Kind::Sequence));  // the metamodel carries it
}

TEST_F(TestExampleOf, AKindsExampleDeclaresOneOfItsKind) {
  EXPECT_NE(metamodel.exampleOf(Kind::Enum).find("enum "), std::string::npos);
  EXPECT_NE(metamodel.exampleOf(Kind::Diagram).find("diagram "), std::string::npos);
}

/// The version of the tool, written in when ds is built (DS-278).
class TestVersion : public Metamodelled {};

TEST_F(TestVersion, IsWrittenAsMajorMinorPatch) {
  const std::string v = metamodel.version();
  int dots = 0;
  for (char c : v) {
    if (c == '.') {
      ++dots;
    } else {
      EXPECT_TRUE(c >= '0' && c <= '9') << v;
    }
  }
  EXPECT_EQ(dots, 2) << v;
}

class TestSpecNamed : public Metamodelled {};

TEST_F(TestSpecNamed, AWordNamesTheKindNameOfWritesAndNoOtherWordDoes) {
  EXPECT_EQ(metamodel.specNamed("class"), &metamodel.specOf(Kind::Class));
  EXPECT_EQ(metamodel.specNamed("usecase"), &metamodel.specOf(Kind::UseCase));
  EXPECT_EQ(metamodel.specNamed("Class"), nullptr);
  EXPECT_EQ(metamodel.specNamed("nothing"), nullptr);
}

/// The class itself (DS-341): its tables are built once, when it is made, and every question reads them.
class TestMetamodel : public Metamodelled {};

TEST_F(TestMetamodel, BuildsItsTablesOnceAndAnswersEveryQuestionFromThem) {
  EXPECT_EQ(&metamodel.kinds(), &metamodel.kinds());
  EXPECT_EQ(&metamodel.specOf(Kind::Package), &metamodel.kinds().front());
  EXPECT_EQ(metamodel.relationOf("contains"), &metamodel.relations().front());
  EXPECT_EQ(&metamodel.nameOf(Kind::Class), &metamodel.nameOf(Kind::Class));  // a name is the table's, not built per call
  const ds::model::Metamodel other;                                           // another one, made elsewhere, says the same language
  EXPECT_EQ(other.kinds().size(), metamodel.kinds().size());
  EXPECT_EQ(other.codes().size(), metamodel.codes().size());
  EXPECT_EQ(other.nameOf(Kind::UseCase), metamodel.nameOf(Kind::UseCase));
  EXPECT_EQ(other.version(), metamodel.version());
}

/// What the metamodel's builders make (DS-341: its private members), read through what the class answers.
class TestWrittenField : public Metamodelled {};
class TestSyntaxField : public Metamodelled {};
class TestDerivedField : public Metamodelled {};
class TestClosedField : public Metamodelled {};
class TestArtefactFields : public Metamodelled {};
class TestBuild : public Metamodelled {};
class TestBuildRelations : public Metamodelled {};
class TestContainment : public Metamodelled {};
class TestFamilies : public Metamodelled {};
class TestWrittenForms : public Metamodelled {};
class TestBuildCodes : public Metamodelled {};
class TestKnownAnnotations : public Metamodelled {};

TEST_F(TestWrittenField, IsAFieldTheAuthorWrites) {
  const auto* f = metamodel.fieldOf(Kind::Requirement, "title");
  ASSERT_NE(f, nullptr);
  EXPECT_EQ(f->type, ds::model::FieldType::Text);
  EXPECT_EQ(f->source, ds::model::FieldSource::Written);
  EXPECT_FALSE(f->meaning.empty());
  EXPECT_TRUE(f->values.empty());
}

TEST_F(TestSyntaxField, IsAFieldTheBinderTakesFromTheSyntax) {
  ASSERT_NE(metamodel.fieldOf(Kind::Method, "params"), nullptr);
  EXPECT_EQ(metamodel.fieldOf(Kind::Method, "params")->source, ds::model::FieldSource::Syntax);
}

TEST_F(TestDerivedField, IsAFieldTheCheckerComputes) {
  ASSERT_NE(metamodel.fieldOf(Kind::Method, "escapes"), nullptr);
  EXPECT_EQ(metamodel.fieldOf(Kind::Method, "escapes")->source, ds::model::FieldSource::Derived);
}

TEST_F(TestClosedField, IsAWrittenFieldThatTakesOnlyItsValues) {
  const auto* f = metamodel.fieldOf(Kind::Verification, "level");
  ASSERT_NE(f, nullptr);
  EXPECT_EQ(f->source, ds::model::FieldSource::Written);
  EXPECT_EQ(f->values, (std::vector<std::string>{"unit", "integration", "system"}));
}

TEST_F(TestArtefactFields, EveryArtefactCarriesTheSameSixFieldsFirstWithAClosedStatus) {
  for (Kind k : {Kind::Requirement, Kind::Verification, Kind::Constraint, Kind::UseCase}) {
    const auto& fields = metamodel.specOf(k).fields;
    ASSERT_GE(fields.size(), 6u) << metamodel.nameOf(k);
    std::vector<std::string> names;
    for (std::size_t i = 0; i < 6; ++i) names.push_back(fields[i].name);
    EXPECT_EQ(names, (std::vector<std::string>{"title", "text", "rationale", "status", "tags", "owner"})) << metamodel.nameOf(k);
    EXPECT_EQ(fields[3].values, (std::vector<std::string>{"draft", "approved", "obsolete"})) << metamodel.nameOf(k);
  }
  // what a kind adds comes after the six: a requirement's parent
  EXPECT_EQ(metamodel.specOf(Kind::Requirement).fields.back().name, "parent");
}

TEST_F(TestBuild, DeclaresThePackageFirstAndAtTheTopEachKindWithHowItIsWritten) {
  ASSERT_FALSE(metamodel.kinds().empty());
  EXPECT_EQ(metamodel.kinds()[0].kind, Kind::Package);
  EXPECT_TRUE(metamodel.kinds()[0].top);
  for (const auto& k : metamodel.kinds()) {
    EXPECT_EQ(k.syntax, metamodel.syntaxOf(k.kind)) << metamodel.nameOf(k.kind);
    EXPECT_EQ(k.example, metamodel.exampleOf(k.kind)) << metamodel.nameOf(k.kind);
    EXPECT_FALSE(k.syntax.empty()) << metamodel.nameOf(k.kind);
  }
}

TEST_F(TestBuildRelations, ContainmentComesFirstAndEveryOtherRelationHasItsEndsAndNoPairs) {
  ASSERT_FALSE(metamodel.relations().empty());
  EXPECT_EQ(metamodel.relations()[0].kind, "contains");
  for (std::size_t i = 1; i < metamodel.relations().size(); ++i) {
    EXPECT_NE(metamodel.relations()[i].kind, "contains");
    EXPECT_TRUE(metamodel.relations()[i].pairs.empty()) << metamodel.relations()[i].kind;
  }
  const auto* uses = metamodel.relationOf("uses");
  ASSERT_NE(uses, nullptr);
  EXPECT_NE(std::find(uses->to.begin(), uses->to.end(), Kind::Enum), uses->to.end());
}

TEST_F(TestContainment, JoinsAParentOnlyWithTheChildrenDeclaredInIt) {
  const auto* contains = metamodel.relationOf("contains");
  ASSERT_NE(contains, nullptr);
  const std::vector<std::pair<Kind, Kind>>& pairs = contains->pairs;
  EXPECT_NE(std::find(pairs.begin(), pairs.end(), std::make_pair(Kind::Module, Kind::Class)), pairs.end());
  EXPECT_EQ(std::find(pairs.begin(), pairs.end(), std::make_pair(Kind::Class, Kind::Module)), pairs.end());
}

TEST_F(TestFamilies, AFamilyMatchesAnySuffixAndANameOnlyItself) {
  EXPECT_NE(metamodel.fieldOf(Kind::Sequence, "local.x"), nullptr);
  EXPECT_EQ(metamodel.fieldOf(Kind::Sequence, "other.x"), nullptr);
  EXPECT_NE(metamodel.fieldOf(Kind::Requirement, "title"), nullptr);
  EXPECT_EQ(metamodel.fieldOf(Kind::Requirement, "titles"), nullptr);
}

TEST_F(TestWrittenForms, EveryKindSaysHowItIsWrittenAndGivesAnExample) {
  for (const auto& k : metamodel.kinds()) {
    EXPECT_FALSE(metamodel.syntaxOf(k.kind).empty()) << metamodel.nameOf(k.kind);
    EXPECT_FALSE(metamodel.exampleOf(k.kind).empty()) << metamodel.nameOf(k.kind);
  }
}

TEST_F(TestBuildCodes, ListsEveryCodeSyntaxFirst) {
  ASSERT_FALSE(metamodel.codes().empty());
  EXPECT_EQ(metamodel.codes()[0].code, "E-SYNTAX");
}

TEST_F(TestKnownAnnotations, AreTheAnnotationsThatMeanSomethingAndTheManualListsTwoRowsMore) {
  const auto& known = metamodel.knownAnnotations();
  const auto& all = metamodel.annotations();
  ASSERT_EQ(all.size(), known.size() + 2);
  for (std::size_t i = 0; i < known.size(); ++i) {
    EXPECT_EQ(all[i].name, known[i].name);
    EXPECT_EQ(known[i].name.rfind("@", 0), 0u) << known[i].name;
    EXPECT_NE(known[i].relation, "refers") << known[i].name;
    EXPECT_EQ(&metamodel.annotationOf(known[i].name.substr(1)), &known[i]);
  }
  // any other word binds as the row the manual lists for it; the colon after an operation is no annotation
  EXPECT_EQ(metamodel.annotationOf("whatever").name, "@<any other word>");
  EXPECT_EQ(all[known.size()].name, "@<any other word>");
  EXPECT_EQ(all.back().name, ": <UC ID> after an operation");
}

/// name and id are what an entity is, read by a rule as name(x) and id(x) (DS-403): no kind declares a field of either
/// name, and the metamodel refuses one.
class TestReservedFields : public Metamodelled {};

TEST_F(TestReservedFields, NoKindDeclaresAFieldNamedNameOrId) {
  EXPECT_TRUE(ds::model::Metamodel::reservedField("name"));
  EXPECT_TRUE(ds::model::Metamodel::reservedField("id"));
  EXPECT_FALSE(ds::model::Metamodel::reservedField("kind"));  // a diagram writes kind: sequence; kind(x) is read apart
  EXPECT_FALSE(ds::model::Metamodel::reservedField("status"));
  for (const auto& k : metamodel.kinds())
    for (const auto& f : k.fields) EXPECT_FALSE(ds::model::Metamodel::reservedField(f.name)) << metamodel.nameOf(k.kind) << " " << f.name;
}

}  // namespace
