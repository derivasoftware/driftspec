#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "ds/api/Api.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/service/UseCases.hpp"

namespace {

using ds::service::Request;

/// A workspace of one class and one class diagram, loaded as the engine loads texts (DS-407).
const char* const kDesign =
    "package p {\n"
    "  module m {\n"
    "    class C {\n"
    "      + n: int\n"
    "    }\n"
    "  }\n"
    "}\n\n"
    "diagram CL_P {\n"
    "  kind: class\n"
    "  include: p@m::*\n"
    "}\n";

class Fixture : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model = load();

  ds::model::Model load() const {
    ds::parse::Parser parser;
    return ds::api::Api(metamodel).load({parser.parseText("p.ds", kDesign)});
  }

  /// What a use case describes into a compact JSON sink, and how it ended.
  std::pair<std::string, std::string> run(const ds::service::IUseCase& use, const Request& request) const {
    const auto sink = ds::emit::SinkFactory::json(false);
    const ds::service::Outcome outcome = use.run(model, request, *sink);
    return {sink->result(), outcome.name};
  }
};

class TestRequest : public Fixture {};
class TestBadRequest : public Fixture {};
class TestUseCaseFactory : public Fixture {};
class TestCheckDesign : public Fixture {};
class TestSeeDesign : public Fixture {};
class TestAskModel : public Fixture {};
class TestHandModel : public Fixture {};
class TestLearnLanguage : public Fixture {};
class TestEditDesign : public Fixture {};

TEST_F(TestRequest, AnArgumentIsReadByItsNameAndAMissingOneIsABadRequestOrItsFallback) {
  const Request request{{{"question", "diagrams"}}};
  EXPECT_EQ(request.get("question"), "diagrams");
  EXPECT_EQ(request.getOr("subject", "none"), "none");
  EXPECT_THROW(request.get("subject"), ds::service::BadRequest);
}

TEST_F(TestBadRequest, ABadRequestIsAnInvalidArgumentThatSaysWhatIsMissing) {
  try {
    Request{}.get("diagram");
    FAIL();
  } catch (const std::invalid_argument& e) {
    EXPECT_NE(std::string(e.what()).find("diagram"), std::string::npos);
  }
}

TEST_F(TestUseCaseFactory, ItMakesEachUseCaseByItsNameAndNoOther) {
  for (const auto& name : ds::service::UseCaseFactory::names()) EXPECT_EQ(ds::service::UseCaseFactory::of(name, metamodel)->name(), name);
  EXPECT_THROW(ds::service::UseCaseFactory::of("teleport", metamodel), ds::emit::NotImplemented);
}

TEST_F(TestCheckDesign, AWorkspaceThatChecksCleanEndsInDefaultWithItsReport) {
  const ds::service::CheckDesign check(metamodel);
  EXPECT_EQ(check.report(model).errors, 0);
  const auto [json, end] = run(check, {});
  EXPECT_EQ(end, "default");
  EXPECT_NE(json.find("\"findings\""), std::string::npos);
}

TEST_F(TestSeeDesign, ADiagramIsDrawnAsItsLayoutAndOneTheWorkspaceLacksIsNoDiagram) {
  const ds::service::SeeDesign see(metamodel);
  EXPECT_FALSE(see.diagram(model, "CL_P").json.empty());
  EXPECT_EQ(run(see, {{{"format", "layout"}, {"diagram", "CL_P"}}}).second, "default");
  EXPECT_EQ(run(see, {{{"format", "layout"}, {"diagram", "CL_Nope"}}}).second, "no-diagram");
  EXPECT_THROW(run(see, {{{"format", "svg"}, {"diagram", "CL_P"}}}), ds::emit::NotImplemented);               // a writer until DS-399
  EXPECT_FALSE(see.needs(model, {{{"format", "layout"}, {"diagram", "CL_P"}}}).has(ds::check::Step::Rules));  // a class diagram
  EXPECT_TRUE(see.needs(model, {{{"format", "json"}}}).has(ds::check::Step::Rules));                          // the viewer reads every step
}

TEST_F(TestAskModel, AQuestionIsAnsweredAndOneTheModelDoesNotAnswerIsAnUnknownQuestion) {
  const ds::service::AskModel ask(metamodel);
  EXPECT_FALSE(ask.ask(model, "diagrams", "").empty());
  EXPECT_EQ(run(ask, {{{"question", "diagrams"}}}).second, "default");
  EXPECT_EQ(run(ask, {{{"question", "weather"}}}).second, "unknown-question");
  EXPECT_THROW(run(ask, {}), ds::service::BadRequest);
}

TEST_F(TestHandModel, TheModelIsDescribedWhole) {
  const ds::service::HandModel hand(metamodel);
  const auto [json, end] = run(hand, {});
  EXPECT_EQ(end, "default");
  EXPECT_NE(json.find("p@m::C"), std::string::npos);
}

TEST_F(TestLearnLanguage, APageIsDescribedAndOneTheManualLacksIsNoTopic) {
  const ds::service::LearnLanguage learn(metamodel);
  EXPECT_FALSE(learn.page("").empty());
  EXPECT_EQ(run(learn, {}).second, "default");
  EXPECT_EQ(run(learn, {{{"topic", "no-such-topic"}}}).second, "no-topic");
  EXPECT_FALSE(learn.needs(model, {}).has(ds::check::Step::Linker));  // it reads no design
}

TEST_F(TestEditDesign, TheWordsThatMayComeAtAPlaceAreDescribedAndALineThatIsNoNumberIsABadRequest) {
  const ds::service::EditDesign edit;
  EXPECT_FALSE(edit.expectedAt("package p {\n}\n", 1, 0).empty());
  EXPECT_EQ(run(edit, {{{"text", "package p {\n}\n"}, {"line", "1"}, {"column", "0"}}}).second, "default");
  EXPECT_THROW(run(edit, {{{"text", ""}, {"line", "one"}, {"column", "0"}}}), ds::service::BadRequest);
}

}  // namespace
