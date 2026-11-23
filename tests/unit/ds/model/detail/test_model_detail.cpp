#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Detail.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/SequenceTree.hpp"

// The model package's internal helpers (DS-332), each held to what the public functions it serves rely on.

namespace {

using ds::model::Entity;
using ds::model::FieldSource;
using ds::model::FieldSpec;
using ds::model::FieldType;
using ds::model::Kind;
using ds::model::KindSpec;
using ds::model::Model;
namespace detail = ds::model::detail;

const std::string kSeq = "p@m::C::run::sequence";

/// One behaviour: m0, an `opt / stop` holding m1, then m2; each step contained by what encloses it.
Model behaviour() {
  Model model;
  Entity s;
  s.kind = Kind::Sequence;
  s.id = kSeq;
  model.declare(s);
  auto message = [&](int order, const std::string& holder) {
    Entity m;
    m.kind = Kind::Message;
    m.id = kSeq + "::m" + std::to_string(order);
    m.scope = kSeq;
    model.declare(m);
    model.relate({"contains", holder, m.id, {}, ""});
  };
  message(0, kSeq);
  Entity f;
  f.kind = Kind::Frame;
  f.id = kSeq + "::f0";
  f.scope = kSeq;
  f.fields["kind"] = "opt";
  f.fields["scenario"] = "stop";
  model.declare(f);
  model.relate({"contains", kSeq, f.id, {}, ""});
  message(1, f.id);
  message(2, kSeq);
  return model;
}

// ---- Identity.cpp ------------------------------------------------------------------------------

class TestOuterOf : public ::testing::Test {};

TEST_F(TestOuterOf, StepsOutOfAMemberAPackageAndTheSystem) {
  EXPECT_EQ(detail::outerOf("p@m::C"), "p@m");
  EXPECT_EQ(detail::outerOf("p@m"), "p@");
  EXPECT_EQ(detail::outerOf("p@"), "");
  EXPECT_EQ(detail::outerOf("app@core@"), "app@");
}

// ---- Model.cpp ---------------------------------------------------------------------------------

class TestFieldIn : public ::testing::Test {};

TEST_F(TestFieldIn, ReadsAFieldAndNothingForOneNotCarried) {
  Entity e;
  e.fields["kind"] = "loop";
  EXPECT_EQ(detail::fieldIn(e, "kind"), "loop");
  EXPECT_EQ(detail::fieldIn(e, "scenario"), "");
}

class TestBranches : public ::testing::Test {};

TEST_F(TestBranches, AnOptNamesItsScenarioOnOneSideOnly) {
  const Model model = behaviour();
  const ds::model::SequenceTree tree(model, kSeq);
  const detail::Branches branches{tree};
  const Entity& opt = *model.find(kSeq + "::f0");
  EXPECT_EQ(branches.names(opt, false), (std::set<std::string>{"stop"}));
  EXPECT_TRUE(branches.names(opt, true).empty());
}

TEST_F(TestBranches, ThePathThatNamesAnOptTakesItAndTheDefaultSkipsIt) {
  const Model model = behaviour();
  const ds::model::SequenceTree tree(model, kSeq);
  const detail::Branches branches{tree};
  const Entity& opt = *model.find(kSeq + "::f0");
  EXPECT_EQ(branches.decide(opt, "stop"), detail::Branches::Way::Then);
  EXPECT_EQ(branches.decide(opt, ""), detail::Branches::Way::Skip);
}

// ---- Metamodel.cpp -----------------------------------------------------------------------------

class TestWrittenField : public ::testing::Test {};
class TestSyntax : public ::testing::Test {};
class TestDerived : public ::testing::Test {};
class TestClosed : public ::testing::Test {};

TEST_F(TestWrittenField, IsAFieldTheAuthorWrites) {
  const FieldSpec f = detail::writtenField("title", FieldType::Text, "what it is");
  EXPECT_EQ(f.name, "title");
  EXPECT_EQ(f.type, FieldType::Text);
  EXPECT_EQ(f.source, FieldSource::Written);
  EXPECT_EQ(f.meaning, "what it is");
  EXPECT_TRUE(f.values.empty());
}

TEST_F(TestSyntax, IsAFieldTheBinderTakesFromTheSyntax) {
  EXPECT_EQ(detail::syntax("params", FieldType::Params, "").source, FieldSource::Syntax);
}

TEST_F(TestDerived, IsAFieldTheCheckerComputes) {
  EXPECT_EQ(detail::derived("escapes", FieldType::Names, "").source, FieldSource::Derived);
}

TEST_F(TestClosed, IsAWrittenFieldThatTakesOnlyItsValues) {
  const FieldSpec f = detail::closed("level", FieldType::Name, "", {"unit", "system"});
  EXPECT_EQ(f.source, FieldSource::Written);
  EXPECT_EQ(f.values, (std::vector<std::string>{"unit", "system"}));
}

class TestArtefact : public ::testing::Test {};

TEST_F(TestArtefact, EveryArtefactCarriesTheSameSixFieldsWithAClosedStatus) {
  std::vector<std::string> names;
  for (const auto& f : detail::artefact()) names.push_back(f.name);
  EXPECT_EQ(names, (std::vector<std::string>{"title", "text", "rationale", "status", "tags", "owner"}));
  EXPECT_EQ(detail::artefact()[3].values, (std::vector<std::string>{"draft", "approved", "obsolete"}));
}

class TestPlus : public ::testing::Test {};

TEST_F(TestPlus, AddsTheMoreFieldsAfterTheBase) {
  const auto joined = detail::plus({detail::writtenField("a", FieldType::Text, "")}, {detail::writtenField("b", FieldType::Text, "")});
  ASSERT_EQ(joined.size(), 2u);
  EXPECT_EQ(joined[0].name, "a");
  EXPECT_EQ(joined[1].name, "b");
}

class TestDescribe : public ::testing::Test {};

TEST_F(TestDescribe, EachKindTakesHowItIsWrittenFromTheManual) {
  KindSpec k;
  k.kind = Kind::Class;
  const auto described = detail::describe({k});
  ASSERT_EQ(described.size(), 1u);
  EXPECT_EQ(described[0].syntax, ds::model::syntaxOf(Kind::Class));
  EXPECT_EQ(described[0].example, ds::model::exampleOf(Kind::Class));
  EXPECT_FALSE(described[0].syntax.empty());
}

class TestBuild : public ::testing::Test {};

TEST_F(TestBuild, DeclaresEveryKindOnceAndThePackageAtTheTop) {
  const auto built = detail::build();
  EXPECT_EQ(built.size(), ds::model::kinds().size());
  ASSERT_FALSE(built.empty());
  EXPECT_EQ(built[0].kind, Kind::Package);
  EXPECT_TRUE(built[0].top);
}

class TestRel : public ::testing::Test {};

TEST_F(TestRel, IsARelationWithItsEndsAndNoPairs) {
  const auto r = detail::rel("uses", "names a type", {Kind::Class}, {Kind::Enum});
  EXPECT_EQ(r.kind, "uses");
  EXPECT_EQ(r.from, (std::vector<Kind>{Kind::Class}));
  EXPECT_EQ(r.to, (std::vector<Kind>{Kind::Enum}));
  EXPECT_TRUE(r.pairs.empty());
}

class TestBuildRelations : public ::testing::Test {};

TEST_F(TestBuildRelations, DeclaresEveryRelationButContainment) {
  const auto built = detail::buildRelations();
  EXPECT_EQ(built.size() + 1, ds::model::relations().size());
  for (const auto& r : built) EXPECT_NE(r.kind, "contains");
}

class TestAmong : public ::testing::Test {};

TEST_F(TestAmong, NoKindsHoldsAnyKind) {
  EXPECT_TRUE(detail::among({}, Kind::Class));
  EXPECT_TRUE(detail::among({Kind::Module, Kind::Class}, Kind::Class));
  EXPECT_FALSE(detail::among({Kind::Package}, Kind::Class));
}

class TestMatches : public ::testing::Test {};

TEST_F(TestMatches, AFamilyMatchesAnySuffixAndANameOnlyItself) {
  EXPECT_TRUE(detail::matches("local.*", "local.x"));
  EXPECT_FALSE(detail::matches("local.*", "other.x"));
  EXPECT_TRUE(detail::matches("title", "title"));
  EXPECT_FALSE(detail::matches("title", "titles"));
}

class TestContainment : public ::testing::Test {};

TEST_F(TestContainment, JoinsAParentOnlyWithTheChildrenDeclaredInIt) {
  const auto contains = detail::containment();
  EXPECT_EQ(contains.kind, "contains");
  const std::vector<std::pair<Kind, Kind>>& pairs = contains.pairs;
  EXPECT_NE(std::find(pairs.begin(), pairs.end(), std::make_pair(Kind::Module, Kind::Class)), pairs.end());
  EXPECT_EQ(std::find(pairs.begin(), pairs.end(), std::make_pair(Kind::Class, Kind::Module)), pairs.end());
}

// ---- WrittenForms.cpp ----------------------------------------------------------------------------

class TestWrittenForms : public ::testing::Test {};

TEST_F(TestWrittenForms, HoldsHowAKindIsWrittenAndItsExample) {
  const auto& forms = detail::writtenForms();
  ASSERT_TRUE(forms.count(Kind::Package));
  EXPECT_EQ(forms.at(Kind::Package).first, ds::model::syntaxOf(Kind::Package));
  EXPECT_EQ(forms.at(Kind::Package).second, ds::model::exampleOf(Kind::Package));
}

// ---- Codes.cpp -----------------------------------------------------------------------------------

class TestBuildCodes : public ::testing::Test {};

TEST_F(TestBuildCodes, ListsEveryCodeTheEngineReportsSyntaxFirst) {
  const auto built = detail::buildCodes();
  EXPECT_EQ(built.size(), ds::model::codes().size());
  ASSERT_FALSE(built.empty());
  EXPECT_EQ(built[0].code, "E-SYNTAX");
}

// ---- Manual.cpp --------------------------------------------------------------------------------

class TestIndent : public ::testing::Test {};

TEST_F(TestIndent, IndentsEveryLineAndEndsEachWithANewline) {
  EXPECT_EQ(detail::indent("a\nb", "  "), "  a\n  b\n");
  EXPECT_EQ(detail::indent("", "  "), "");
}

class TestPad : public ::testing::Test {};

TEST_F(TestPad, FillsToTheWidthOrLeavesOneSpaceAfterALongerText) {
  EXPECT_EQ(detail::pad("ab", 4), "ab  ");
  EXPECT_EQ(detail::pad("abcd", 3), "abcd ");
}

class TestNames : public ::testing::Test {};

TEST_F(TestNames, NamesTheKindsAsTheLanguageWritesThem) {
  EXPECT_EQ(detail::names({Kind::Package, Kind::Module}), "package, module");
  EXPECT_EQ(detail::names({}), "");
}

class TestKindPage : public ::testing::Test {};

TEST_F(TestKindPage, SaysWhatAKindIsAndEndsWithItsExample) {
  const std::string page = detail::kindPage(ds::model::specOf(Kind::Class));
  EXPECT_EQ(page.rfind("class — ", 0), 0u);
  EXPECT_NE(page.find("Example (a workspace that checks clean)"), std::string::npos);
}

class TestIndexPage : public ::testing::Test {};

TEST_F(TestIndexPage, IsTheManualWithNoTopic) {
  EXPECT_EQ(detail::indexPage().rfind("ds — a software design as text", 0), 0u);
  EXPECT_EQ(detail::indexPage(), ds::model::manualPage(""));
}

class TestGrammarPage : public ::testing::Test {};

TEST_F(TestGrammarPage, IsTheGrammarTopic) {
  EXPECT_EQ(detail::grammarPage().rfind("How a workspace is written", 0), 0u);
  EXPECT_EQ(detail::grammarPage(), ds::model::manualPage("grammar"));
}

}  // namespace
