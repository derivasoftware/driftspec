#include "ds/service/UseCases.hpp"

#include "ds/emit/NotImplemented.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/model/Manual.hpp"
#include "ds/parse/Parser.hpp"

namespace ds {
namespace service {

namespace {

/// An argument written as a whole number; BadRequest when it is not one.
int number(const Request& request, const std::string& name, const std::string& fallback) {
  const std::string text = request.getOr(name, fallback);
  try {
    std::size_t end = 0;
    const int value = std::stoi(text, &end);
    if (end == text.size()) return value;
  } catch (const std::exception&) {
  }
  throw BadRequest(name + " is no whole number: " + text);
}

}  // namespace

// ---- UC-00001-1 ------------------------------------------------------------------------------------------------

check::Report CheckDesign::report(const model::Model& model) const { return check::Report::of(model, metamodel_); }

check::Selection CheckDesign::needs(const model::Model&, const Request&) const { return check::Selection::every(); }

Outcome CheckDesign::run(const model::Model& model, const Request&, emit::ISink& sink) const {
  const check::Report document = report(model);
  format::DescriptionFactory::report(document)->describe(sink);
  return {document.errors ? "errors" : "default"};
}

// ---- UC-00002-1 ------------------------------------------------------------------------------------------------

scene::SceneDocument SeeDesign::viewer(const model::Model& model) const { return scene::Scene(model, metamodel_).build(); }

render::IrText SeeDesign::diagram(const model::Model& model, const std::string& name) const {
  return {render::IrExport(model, metamodel_).render(model::Identity::ofDiagram(name).text(), false)};
}

check::Selection SeeDesign::needs(const model::Model& bound, const Request& request) const {
  if (request.getOr("format", "json") == "json") return check::Selection::every();  // the viewer shows all a check derives
  // a drawing shows relations, calls and couplings, and stereotypes; a table is the answers of its rules (DS-268, DS-376)
  const check::Selection drawing = check::Selection::of({check::Step::Linker, check::Step::Behaviour, check::Step::Derivation});
  const std::string id = model::Identity::ofDiagram(request.getOr("diagram", "")).text();
  return bound.formulas().count(id) ? drawing.with(check::Step::Rules) : drawing;
}

Outcome SeeDesign::run(const model::Model& model, const Request& request, emit::ISink& sink) const {
  const std::string format = request.getOr("format", "json");
  if (format == "json") {
    const scene::SceneDocument document = viewer(model);
    format::DescriptionFactory::scene(document)->describe(sink);
    return {"default"};
  }
  if (format != "layout" && format != "cells")  // the IR and the drawings are writers, not descriptions, until DS-399
    throw emit::NotImplemented("diagram as " + format, "", {"json", "layout", "cells"});
  const render::IrText ir = diagram(model, request.get("diagram"));
  if (ir.json.empty()) return {"no-diagram"};
  const int width = number(request, "width", "0");
  const auto description =
      format == "layout" ? format::DescriptionFactory::layout(ir, theme_, width) : format::DescriptionFactory::cells(ir, theme_, width);
  description->describe(sink);
  return {"default"};
}

// ---- UC-00004-1 ------------------------------------------------------------------------------------------------

std::vector<query::Answer> AskModel::ask(const model::Model& model, const std::string& question, const std::string& subject) const {
  return query::Query(model, metamodel_).ask(question, subject);
}

check::Selection AskModel::needs(const model::Model&, const Request&) const { return check::Selection::every(); }

Outcome AskModel::run(const model::Model& model, const Request& request, emit::ISink& sink) const {
  const std::vector<query::Answer> answers = ask(model, request.get("question"), request.getOr("subject", ""));
  format::DescriptionFactory::answers(answers)->describe(sink);  // an error is an answer too: said as the others are
  return {!answers.empty() && answers.front().kind == "error" ? "unknown-question" : "default"};
}

// ---- UC-00005-1 ------------------------------------------------------------------------------------------------

check::Selection HandModel::needs(const model::Model&, const Request&) const { return check::Selection::every(); }

Outcome HandModel::run(const model::Model& model, const Request&, emit::ISink& sink) const {
  exporter::JsonExport(metamodel_).render(model, sink);
  return {"default"};
}

// ---- UC-00006-1 ------------------------------------------------------------------------------------------------

std::string LearnLanguage::page(const std::string& topic) const { return model::Manual(metamodel_).page(topic); }

check::Selection LearnLanguage::needs(const model::Model&, const Request&) const { return check::Selection::of({}); }

// The page as ds manual --json has written it (DS-397): its fields in byte order. ds's own manual command still writes
// it itself until DS-409 hands it this use case.
Outcome LearnLanguage::run(const model::Model&, const Request& request, emit::ISink& sink) const {
  const std::string topic = request.getOr("topic", "");
  const model::Manual book(metamodel_);
  const std::string text = book.page(topic);
  if (text.empty()) return {"no-topic"};
  sink.beginRecord();
  if (const model::CodeSpec* c = metamodel_.codeOf(topic)) {
    sink.field("code").beginRecord().field("code").value(c->code).field("fix").value(c->fix).field("meaning").value(c->meaning);
    sink.field("severity").value(metamodel_.severityOf(c->severity).name).endRecord();
  }
  const model::KindSpec* kind = nullptr;  // the kind the topic names, the last when two did
  for (const auto& k : metamodel_.kinds())
    if (topic == metamodel_.nameOf(k.kind)) kind = &k;
  if (kind) {
    sink.field("kind").beginRecord().field("example").value(kind->example).field("fields").beginList();
    for (const auto& f : kind->fields) {
      sink.beginRecord().field("meaning").value(f.meaning).field("name").value(f.name).field("source").value(
          metamodel_.fieldSourceName(f.source));
      sink.field("type").value(metamodel_.fieldTypeName(f.type)).field("values").beginList();
      for (const auto& v : f.values) sink.value(v);
      sink.endList().endRecord();
    }
    sink.endList().field("flags").beginList();
    for (const auto& flag : kind->flags) sink.value(flag);
    sink.endList().field("meaning").value(kind->meaning).field("syntax").value(kind->syntax).endRecord();
  }
  sink.field("text").value(text).field("topic").value(topic.empty() ? "index" : topic);
  if (topic.empty()) {
    sink.field("topics").beginList();
    for (const auto& t : book.topics()) sink.value(t);
    sink.endList();
  }
  sink.endRecord();
  return {"default"};
}

// ---- UC-00007-1 ------------------------------------------------------------------------------------------------

std::vector<std::string> EditDesign::expectedAt(const std::string& text, int line, int column) const {
  parse::Parser parser;
  return parser.expectedAt(text, line, column);
}

check::Selection EditDesign::needs(const model::Model&, const Request&) const { return check::Selection::of({}); }

Outcome EditDesign::run(const model::Model&, const Request& request, emit::ISink& sink) const {
  const std::vector<std::string> words = expectedAt(request.get("text"), number(request, "line", ""), number(request, "column", ""));
  sink.beginList();
  for (const auto& word : words) sink.value(word);
  sink.endList();
  return {"default"};
}

// ---- the factory -----------------------------------------------------------------------------------------------

std::unique_ptr<IUseCase> UseCaseFactory::of(const std::string& name, const model::Metamodel& metamodel) {
  if (name == "check-design") return std::make_unique<CheckDesign>(metamodel);
  if (name == "see-design") return std::make_unique<SeeDesign>(metamodel);
  if (name == "ask-model") return std::make_unique<AskModel>(metamodel);
  if (name == "hand-model") return std::make_unique<HandModel>(metamodel);
  if (name == "learn-language") return std::make_unique<LearnLanguage>(metamodel);
  if (name == "edit-design") return std::make_unique<EditDesign>();
  throw emit::NotImplemented("", name, names());
}

std::vector<std::string> UseCaseFactory::names() {
  return {"check-design", "see-design", "ask-model", "hand-model", "learn-language", "edit-design"};
}

}  // namespace service
}  // namespace ds
