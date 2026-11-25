#include "ds/service/UseCases.hpp"

#include <algorithm>
#include <set>

#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/format/FormatFactory.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/Manual.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/Drawings.hpp"
#include "ds/render/Text.hpp"

namespace ds {
namespace service {

// ---- UC-00001-1 ------------------------------------------------------------------------------------------------

check::Report CheckDesign::report(const model::Model& model) const { return check::Report::of(model, metamodel_); }

check::Selection CheckDesign::needs(const model::Model&, const Request&) const { return check::Selection::every(); }

Outcome CheckDesign::run(const model::Model& model, const Request& request, emit::ISink& sink) const {
  const check::Report document = report(model);
  // the workspace as its door was given it is where the report says the open coverage is listed (DS-431)
  format::DescriptionFactory::report(document, request.getOr("workspace", ""))->describe(sink);
  if (!document.errors) return {"default"};
  return {"errors", std::to_string(document.errors) + (document.errors == 1 ? " error is" : " errors are") + " among the findings"};
}

// ---- UC-00002-1 ------------------------------------------------------------------------------------------------

scene::SceneDocument SeeDesign::viewer(const model::Model& model) const { return scene::Scene(model, metamodel_).build(); }

std::optional<render::IrDocument> SeeDesign::diagram(const model::Model& model, const std::string& name) const {
  return render::IrExport(model, metamodel_).document(model::Identity::ofDiagram(name).text());
}

// a diagram is not drawn of what did not read whole (DS-433): a file of its own, or one that declares what it shows,
// that did not parse may hold the very boxes it draws. A diagram that is missing or shows nothing may have been lost to
// any of them; a file it does not read stops nothing (DS-411)
std::string SeeDesign::unread(const model::Model& model, const std::string& name, const std::string& workspace) const {
  std::set<std::string> reads;
  const model::Entity* d = model.find(model::Identity::ofDiagram(name).text());
  if (d) reads.insert(d->location.file);
  std::size_t shown = 0;
  for (const auto& a : query::QueryFactory::of(model, metamodel_)->ask("shows", name))
    if (const model::Entity* e = a.kind == "error" ? nullptr : model.find(a.id)) {
      reads.insert(e->location.file);
      ++shown;
    }
  const bool lost = !d || shown == 0;  // missing, or showing nothing: what did not parse may be why
  std::string said;
  std::size_t count = 0;
  for (const auto& f : model.findings()) {
    if ((f.code != "E-SYNTAX" && f.code != "E-ENCODING") || !(lost || reads.count(f.location.file))) continue;
    said += f.location.file + ":" + std::to_string(f.location.line) + ":" + std::to_string(f.location.column) + "  " + f.code + "  " +
            f.message + "\n";
    ++count;
  }
  if (!count) return "";
  const std::string files = count == 1 ? "a file" : std::to_string(count) + " files";
  const std::string verb = count == 1 ? "does" : "do";
  if (!lost) return said + name + " is not drawn: " + files + " it reads " + verb + " not read whole";
  return said + name + (d ? " shows nothing" : " is not found") + ", and " + files + " of " + workspace + " " + verb +
         " not read whole: what did not parse may be why";
}

check::Selection SeeDesign::needs(const model::Model& bound, const Request& request) const {
  const std::string format = request.getOr("format", "json");
  if (format == "json" || format == "html") return check::Selection::every();  // the viewer shows all a check derives
  // a drawing shows relations, calls and couplings, and stereotypes; a table is the answers of its rules (DS-268, DS-376)
  const check::Selection drawing = check::Selection::of({check::Step::Linker, check::Step::Behaviour, check::Step::Derivation});
  const std::string id = model::Identity::ofDiagram(request.getOr("diagram", "")).text();
  return bound.formulas().count(id) ? drawing.with(check::Step::Rules) : drawing;
}

std::vector<std::string> SeeDesign::formats() { return {"cells", "html", "ir", "json", "layout", "svg", "text"}; }

Outcome SeeDesign::run(const model::Model& model, const Request& request, emit::ISink& sink) const {
  const std::string format = request.getOr("format", "json");
  const std::vector<std::string> there = formats();
  if (std::find(there.begin(), there.end(), format) == there.end()) throw emit::NotImplemented("diagram", format, there);
  if (format == "json") {
    const scene::SceneDocument document = viewer(model);
    format::DescriptionFactory::scene(document)->describe(sink);
    return {"default"};
  }
  if (format == "html") return page(model, request, sink);
  return drawn(model, format, request, sink);
}

Outcome SeeDesign::page(const model::Model& model, const Request& request, emit::ISink& sink) const {
  // framed on what the diagram asked shows (DS-126), or on the identities the request names
  std::vector<std::string> focus = request.lines("focus");
  const std::string diagram = request.getOr("diagram", "");
  if (!diagram.empty()) {
    const auto shown = query::QueryFactory::of(model, metamodel_)->ask("shows", diagram);
    if (!shown.empty() && shown.front().kind == "error") return {"no-diagram", "no diagram named " + diagram};
    focus.clear();
    for (const auto& a : shown) focus.push_back(a.id);
  }
  format::FormatOptions options;
  options.title = request.getOr("title", "");
  options.css = request.getOr("css", "");
  options.focus = focus;
  options.embed = request.flag("embed");
  options.height = request.number("height", "640");
  // a page whose scene has a walkthrough carries every diagram drawn as render draws it in svg, the cards its steps
  // show (DS-470); any other page is written as it was
  const scene::SceneDocument document = viewer(model);
  if (!document.walkthroughs.empty()) options.drawings = render::Drawings::of(model, metamodel_, theme_);
  // the page is the scene in JSON inside its template: the scene and the focus travel compact inside it, written into
  // a sink of their own; the page, written whole, is handed to the sink lent
  const auto inside = emit::SinkFactory::json(false);
  sink.verbatim(format::FormatFactory::scene().at("html")->write(document, options, *inside));
  return {"default"};
}

Outcome SeeDesign::drawn(const model::Model& model, const std::string& format, const Request& request, emit::ISink& sink) const {
  const std::string& name = request.get("diagram");
  if (request.flag("whole")) {  // asked whole, as the command line asks: what did not read whole is said, not drawn (DS-433)
    const std::string why = unread(model, name, request.getOr("workspace", ""));
    if (!why.empty()) return {"no-diagram", why};
  }
  if (format == "svg" || format == "text") {
    // a drawing, drawn here and handed to the sink as it is: a bridge until a figure is described as marks (DS-463)
    const std::optional<std::string> drawing = picture(model, format, name, request);
    if (!drawing) return {"no-diagram", "no diagram named " + name};
    sink.verbatim(*drawing);
    return {"default"};
  }
  const std::optional<render::IrDocument> document = diagram(model, name);
  if (!document) return {"no-diagram", "no diagram named " + name};
  // each is handed the document (DS-458): the sink lent receives the drawing alone, never read back
  if (format == "ir") {
    format::DescriptionFactory::ir(*document)->describe(sink);
    return {"default"};
  }
  const int width = request.number("width", "0");
  const auto description = format == "layout" ? format::DescriptionFactory::layout(*document, theme_, width)
                                              : format::DescriptionFactory::cells(*document, theme_, width);
  description->describe(sink);
  return {"default"};
}

std::optional<std::string> SeeDesign::picture(const model::Model& model, const std::string& format, const std::string& name,
                                              const Request& request) const {
  if (format == "svg") {  // the one path from a diagram to its svg, as a walkthrough's cards take it (DS-470)
    const std::string drawing = render::Drawings::svg(model, metamodel_, theme_, name, request.number("width", "0"));
    if (drawing.empty()) return std::nullopt;
    return drawing;
  }
  const std::optional<render::IrDocument> document = diagram(model, name);
  if (!document) return std::nullopt;
  return render::Text(theme_).render(*document, request.flag("color"), request.number("width", "0"));
}

// ---- UC-00004-1 ------------------------------------------------------------------------------------------------

std::vector<query::Answer> AskModel::ask(const model::Model& model, const std::string& question, const std::string& subject) const {
  return query::Query(model, metamodel_).ask(question, subject);
}

check::Selection AskModel::needs(const model::Model&, const Request&) const { return check::Selection::every(); }

Outcome AskModel::run(const model::Model& model, const Request& request, emit::ISink& sink) const {
  const std::vector<query::Answer> answers = ask(model, request.get("question"), request.getOr("subject", ""));
  format::DescriptionFactory::answers(answers)->describe(sink);  // an error is an answer too: said as the others are
  if (answers.empty() || answers.front().kind != "error") return {"default"};
  // what was wrong with the question, and when it is no question at all the ones there are
  const query::Answer& error = answers.front();
  const bool unknown = error.detail.find("not a question") != std::string::npos;
  return {"unknown-question", error.id + ": " + error.detail + (unknown ? "\nquestions: " + query::Query::usage() : "")};
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

// The page as ds manual has written it: as structure its fields in byte order, as ds manual --json writes them (DS-397);
// as presentation its text, as ds manual prints it (DS-409). An unknown topic lists the ones there are.
Outcome LearnLanguage::run(const model::Model&, const Request& request, emit::ISink& sink) const {
  const std::string topic = request.getOr("topic", "");
  const model::Manual book(metamodel_);
  const std::string text = book.page(topic);
  if (text.empty()) {
    std::string topics;
    for (const auto& t : book.topics()) topics += " " + t;
    return {"no-topic", "no page '" + topic + "' in the manual; there are:" + topics};
  }
  sink.beginRecord();
  if (const model::CodeSpec* c = metamodel_.codeOf(topic)) {
    sink.field("code").beginRecord().field("code").value(c->code).field("fix").value(c->fix).field("meaning").value(c->meaning);
    sink.field("severity").value(metamodel_.severityOf(c->severity).name).endRecord();
  }
  const model::KindSpec* kind = nullptr;  // the kind the topic names, the last when two did
  for (const auto& k : metamodel_.kinds())
    if (topic == metamodel_.nameOf(k.kind)) kind = &k;
  if (kind) {
    sink.field("kind")
        .beginRecord()
        .field("category")
        .value(model::Metamodel::categoryName(kind->category))
        .field("example")
        .value(kind->example)
        .field("fields")
        .beginList();
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
  // a paragraph is its text and a line break, and every page ends its last line: the page as it is printed
  const std::string_view page(text);
  sink.paragraph(page.back() == '\n' ? page.substr(0, page.size() - 1) : page);
  return {"default"};
}

// ---- UC-00007-1 ------------------------------------------------------------------------------------------------

std::vector<std::string> EditDesign::expectedAt(const std::string& text, int line, int column) const {
  parse::Parser parser;
  return parser.expectedAt(text, line, column);
}

check::Selection EditDesign::needs(const model::Model&, const Request&) const { return check::Selection::of({}); }

Outcome EditDesign::run(const model::Model&, const Request& request, emit::ISink& sink) const {
  const std::vector<std::string> words = expectedAt(request.get("text"), request.number("line", ""), request.number("column", ""));
  sink.beginList();
  for (const auto& word : words) sink.value(word);
  sink.endList();
  return {"default"};
}

// ---- the factory -----------------------------------------------------------------------------------------------

std::unique_ptr<IUseCase> UseCaseFactory::of(const std::string& name) const {
  if (name == "check-design") return std::make_unique<CheckDesign>(metamodel_);
  if (name == "see-design") return std::make_unique<SeeDesign>(metamodel_);
  if (name == "ask-model") return std::make_unique<AskModel>(metamodel_);
  if (name == "hand-model") return std::make_unique<HandModel>(metamodel_);
  if (name == "learn-language") return std::make_unique<LearnLanguage>(metamodel_);
  if (name == "edit-design") return std::make_unique<EditDesign>();
  throw emit::NotImplemented("", name, names());
}

std::vector<std::string> UseCaseFactory::names() {
  return {"check-design", "see-design", "ask-model", "hand-model", "learn-language", "edit-design"};
}

}  // namespace service
}  // namespace ds
