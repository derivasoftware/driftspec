#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "ds/api/Api.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/emit/TextSink.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"
#include "ds/service/UseCases.hpp"
#include "ds/theme/Theme.hpp"

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

/// A sink that writes into another and says whether anything was written (DS-462): a use case that ends in an outcome
/// such as no-diagram writes nothing, since an outcome is returned, not written, and nothing written has no result.
class Counted : public ds::emit::ISink {
 public:
  explicit Counted(ds::emit::ISink& to) : to_(to) {}
  bool written() const { return written_; }
  ds::emit::ISink& beginRecord() override { return mark(to_.beginRecord()); }
  ds::emit::ISink& endRecord() override { return mark(to_.endRecord()); }
  ds::emit::ISink& field(std::string_view name) override { return mark(to_.field(name)); }
  ds::emit::ISink& beginList() override { return mark(to_.beginList()); }
  ds::emit::ISink& endList() override { return mark(to_.endList()); }
  ds::emit::ISink& value(std::string_view text) override { return mark(to_.value(text)); }
  ds::emit::ISink& integer(std::int64_t value) override { return mark(to_.integer(value)); }
  ds::emit::ISink& number(double value) override { return mark(to_.number(value)); }
  ds::emit::ISink& boolean(bool value) override { return mark(to_.boolean(value)); }
  ds::emit::ISink& null() override { return mark(to_.null()); }
  ds::emit::ISink& paragraph(std::string_view text) override { return mark(to_.paragraph(text)); }
  ds::emit::ISink& table(const std::vector<std::string_view>& columns) override { return mark(to_.table(columns)); }
  ds::emit::ISink& row(const std::vector<std::string_view>& cells) override { return mark(to_.row(cells)); }
  ds::emit::ISink& endTable() override { return mark(to_.endTable()); }
  ds::emit::ISink& verbatim(std::string_view text) override { return mark(to_.verbatim(text)); }
  /// The end of a description writes nothing: the sink it writes into says whether it accepted any of it (DS-399).
  ds::emit::ISink& described(std::string_view document) override {
    to_.described(document);
    return *this;
  }
  std::string result() override { return to_.result(); }

 private:
  ds::emit::ISink& mark(ds::emit::ISink&) {
    written_ = true;
    return *this;
  }
  ds::emit::ISink& to_;
  bool written_ = false;
};

class Fixture : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model = load();

  ds::model::Model load() const {
    ds::parse::Parser parser;
    return ds::api::Api(metamodel).load({parser.parseText("p.ds", kDesign)});
  }

  /// What a use case describes into a compact JSON sink ("" when it wrote nothing), and how it ended.
  std::pair<std::string, std::string> run(const ds::service::IUseCase& use, const Request& request) const {
    const auto sink = ds::emit::SinkFactory::json(false);
    Counted counted(*sink);
    const ds::service::Outcome outcome = use.run(model, request, counted);
    return {counted.written() ? sink->result() : std::string(), outcome.name};
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

TEST_F(TestRequest, ANumberAFlagAndLinesAreReadByTheirName) {
  const Request request{{{"width", "60"}, {"wide", "sixty"}, {"color", "true"}, {"embed", "yes"}, {"focus", "p@m::C\n\np@m::D"}}};
  EXPECT_EQ(request.number("width", "0"), 60);
  EXPECT_EQ(request.number("height", "640"), 640);  // not given: its fallback
  EXPECT_THROW(request.number("wide", "0"), ds::service::BadRequest);
  EXPECT_TRUE(request.flag("color"));
  EXPECT_FALSE(request.flag("embed"));  // only "true" is true
  EXPECT_FALSE(request.flag("whole"));  // not given
  EXPECT_EQ(request.lines("focus"), (std::vector<std::string>{"p@m::C", "", "p@m::D"}));
  const Request ended{{{"focus", "a\nb\n"}}}, empty{{{"focus", "\n"}}};
  EXPECT_EQ(ended.lines("focus"), (std::vector<std::string>{"a", "b"}));  // each item ended by a line break
  EXPECT_EQ(empty.lines("focus"), (std::vector<std::string>{""}));        // one empty item
  EXPECT_TRUE(request.lines("none").empty());
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
  for (const auto& name : ds::service::UseCaseFactory::names()) EXPECT_EQ(ds::service::UseCaseFactory(metamodel).of(name)->name(), name);
  EXPECT_THROW(ds::service::UseCaseFactory(metamodel).of("teleport"), ds::emit::NotImplemented);
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
  EXPECT_TRUE(see.diagram(model, "CL_P").has_value());
  EXPECT_EQ(run(see, {{{"format", "layout"}, {"diagram", "CL_P"}}}).second, "default");
  EXPECT_EQ(run(see, {{{"format", "layout"}, {"diagram", "CL_Nope"}}}), (std::pair<std::string, std::string>{"", "no-diagram"}));
  EXPECT_THROW(run(see, {{{"format", "svg"}, {"diagram", "CL_P"}}}), ds::emit::NotImplemented);  // no JSON: its sink says so (DS-474)
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
  EXPECT_EQ(run(learn, {{{"topic", "no-such-topic"}}}), (std::pair<std::string, std::string>{"", "no-topic"}));
  EXPECT_FALSE(learn.needs(model, {}).has(ds::check::Step::Linker));  // it reads no design
}

TEST_F(TestEditDesign, TheWordsThatMayComeAtAPlaceAreDescribedAndALineThatIsNoNumberIsABadRequest) {
  const ds::service::EditDesign edit;
  EXPECT_FALSE(edit.expectedAt("package p {\n}\n", 1, 0).empty());
  EXPECT_EQ(run(edit, {{{"text", "package p {\n}\n"}, {"line", "1"}, {"column", "0"}}}).second, "default");
  EXPECT_THROW(run(edit, {{{"text", ""}, {"line", "one"}, {"column", "0"}}}), ds::service::BadRequest);
}

/// A drawing is see-design's (DS-474): drawn through render as it draws, and handed to the sink lent as one verbatim text,
/// so a door draws nothing; the text sink writes it as it is.
TEST_F(TestSeeDesign, ADrawingIsHandedToTheSinkAsOneVerbatimTextAsRenderDrawsIt) {
  const ds::service::SeeDesign see(metamodel);
  const ds::theme::Theme theme;
  const ds::render::IrDocument ir = *see.diagram(model, "CL_P");
  ds::emit::TextSink sink;
  EXPECT_EQ(see.run(model, {{{"format", "svg"}, {"diagram", "CL_P"}, {"width", "300"}}}, sink).name, "default");
  EXPECT_EQ(sink.result(), ds::render::Svg(theme).render(ir, 300));
  for (const bool color : {false, true}) {
    EXPECT_EQ(see.run(model, {{{"format", "text"}, {"diagram", "CL_P"}, {"color", color ? "true" : "false"}}}, sink).name, "default");
    EXPECT_EQ(sink.result(), ds::render::Text(theme).render(ir, color, 0)) << color;
  }
  // one verbatim text a document: the sink refuses a second, so a drawing is never two
  EXPECT_EQ(see.run(model, {{{"format", "text"}, {"diagram", "CL_P"}}}, sink).name, "default");
  EXPECT_THROW(see.run(model, {{{"format", "text"}, {"diagram", "CL_P"}}}, sink), std::logic_error);
  sink.result();
  // the IR is described, as the structure it is: the text sink writes nothing of it
  EXPECT_THROW(see.run(model, {{{"format", "ir"}, {"diagram", "CL_P"}}}, sink), ds::emit::NotImplemented);
  EXPECT_EQ(run(see, {{{"format", "text"}, {"diagram", "CL_Nope"}}}), (std::pair<std::string, std::string>{"", "no-diagram"}));
  EXPECT_THROW(run(see, {{{"format", "svg"}, {"diagram", "CL_P"}, {"width", "wide"}}}), ds::service::BadRequest);
}

TEST_F(TestSeeDesign, ThePageIsFramedOnWhatTheDiagramShowsAndOneTheWorkspaceLacksIsNoDiagram) {
  const ds::service::SeeDesign see(metamodel);
  ds::emit::TextSink sink;
  EXPECT_EQ(see.run(model, {{{"format", "html"}, {"title", "the <p> design"}, {"diagram", "CL_P"}}}, sink).name, "default");
  const std::string page = sink.result();
  EXPECT_NE(page.find("<html"), std::string::npos);
  EXPECT_NE(page.find("the &lt;p&gt; design"), std::string::npos);
  EXPECT_NE(page.find("const FOCUS=[\"p@m\",\"p@m::C\"]"), std::string::npos);  // what CL_P shows
  EXPECT_EQ(see.run(model, {{{"format", "html"}, {"title", "p"}, {"focus", "p@m::C\np\n"}}}, sink).name, "default");
  EXPECT_NE(sink.result().find("const FOCUS=[\"p@m::C\",\"p\"]"), std::string::npos);  // the identities given, one a line
  EXPECT_EQ(see.run(model, {{{"format", "html"}, {"title", "p"}, {"embed", "true"}, {"height", "300"}}}, sink).name, "default");
  const std::string frame = sink.result();
  EXPECT_EQ(frame.rfind("<iframe", 0), 0u);
  EXPECT_NE(frame.find("height:300px"), std::string::npos);
  const auto [written, end] = run(see, {{{"format", "html"}, {"diagram", "CL_Nope"}}});
  EXPECT_EQ(end, "no-diagram");
  EXPECT_EQ(written, "");
  EXPECT_TRUE(see.needs(model, {{{"format", "html"}}}).has(ds::check::Step::Rules));  // the viewer reads every step
}

TEST_F(TestSeeDesign, AFormatItDoesNotWriteIsNotImplemented) {
  const ds::service::SeeDesign see(metamodel);
  try {
    run(see, {{{"format", "yaml"}, {"diagram", "CL_P"}}});
    FAIL() << "a format see-design does not write was written";
  } catch (const ds::emit::NotImplemented& e) {
    EXPECT_EQ(e.document(), "diagram");
    EXPECT_EQ(e.technology(), "yaml");
    EXPECT_EQ(std::string(e.what()), "the diagram is not written in yaml; there is: cells html ir json layout svg text");
  }
}

/// A sink that builds what it is given elsewhere and has no text to give back, as a Python sink builds objects (DS-459):
/// its result() is empty. What was written through it is taken from where it went.
class NoText : public ds::emit::ISink {
 public:
  ds::emit::ISink& beginRecord() override { return keep(to_.beginRecord()); }
  ds::emit::ISink& endRecord() override { return keep(to_.endRecord()); }
  ds::emit::ISink& field(std::string_view name) override { return keep(to_.field(name)); }
  ds::emit::ISink& beginList() override { return keep(to_.beginList()); }
  ds::emit::ISink& endList() override { return keep(to_.endList()); }
  ds::emit::ISink& value(std::string_view text) override { return keep(to_.value(text)); }
  ds::emit::ISink& integer(std::int64_t value) override { return keep(to_.integer(value)); }
  ds::emit::ISink& number(double value) override { return keep(to_.number(value)); }
  ds::emit::ISink& boolean(bool value) override { return keep(to_.boolean(value)); }
  ds::emit::ISink& null() override { return keep(to_.null()); }
  ds::emit::ISink& paragraph(std::string_view text) override { return keep(to_.paragraph(text)); }
  ds::emit::ISink& table(const std::vector<std::string_view>& columns) override { return keep(to_.table(columns)); }
  ds::emit::ISink& row(const std::vector<std::string_view>& cells) override { return keep(to_.row(cells)); }
  ds::emit::ISink& endTable() override { return keep(to_.endTable()); }
  ds::emit::ISink& verbatim(std::string_view text) override { return keep(to_.verbatim(text)); }
  ds::emit::ISink& described(std::string_view document) override {
    to_.described(document);
    return *this;
  }
  std::string result() override { return ""; }  // nothing to read back: what it built is elsewhere
  /// What was written through it, taken where it went.
  std::string taken() { return to_.result(); }

 private:
  ds::emit::ISink& keep(ds::emit::ISink&) { return *this; }
  const std::unique_ptr<ds::emit::ISink> owned_ = ds::emit::SinkFactory::json(false);
  ds::emit::ISink& to_ = *owned_;
};

/// A use case describes into the sink it is lent and never reads it back (DS-458): seeing a diagram's layout or cells
/// through a sink with no text to give back still draws it, since the descriptions are handed the document itself.
TEST_F(TestSeeDesign, ALayoutAndItsCellsAreDrawnThroughASinkThatGivesNoTextBack) {
  const ds::service::SeeDesign see(metamodel);
  for (const char* format : {"layout", "cells"}) {
    NoText sink;
    EXPECT_EQ(see.run(model, {{{"format", format}, {"diagram", "CL_P"}}}, sink).name, "default") << format;
    EXPECT_FALSE(sink.taken().empty()) << format;  // the drawing, written through it
  }
}

}  // namespace
