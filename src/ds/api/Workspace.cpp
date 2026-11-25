#include "ds/api/Workspace.hpp"

#include <optional>

#include "ds/api/Api.hpp"
#include "ds/check/Report.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/format/FormatFactory.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/SequenceTree.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"
#include "ds/query/Walk.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/scene/Scene.hpp"

namespace ds {
namespace api {

Workspace Workspace::madeFor(const model::Metamodel& metamodel, const Files& files, Output output, const std::string& diagram) {
  Workspace made{service::UseCaseFactory(metamodel)};
  made.output_ = output;
  made.diagram_ = diagram;
  made.given_ = true;
  made.reload(files);
  return made;
}

Workspace::Workspace(const service::UseCaseFactory& useCases)
    : useCases_(useCases), metamodel_(useCases.metamodel()), output_(Output::Model), model_(useCases.metamodel()) {}

void Workspace::reload(const Files& files) {
  parse(files);
  stale_ = true;
  if (!given_) return;  // loaded when a use case asks, for what it needs (DS-408)
  // made for an output: checked now, with the steps that output reads, as the Api loads it (DS-376, DS-420)
  Api api(metamodel_);
  Bound bound = api.bound(this->files());
  selection_ = Api::stepsFor(output_, bound.model, diagram_);
  model_ = api.checked(std::move(bound), selection_);
  stale_ = false;
  ++loads_;
}

void Workspace::parse(const Files& files) {
  parse::Parser parser;
  std::map<std::string, parse::ParsedFile> kept;
  for (const auto& [path, text] : files) {
    const auto held = parsed_.find(path);
    kept[path] = held != parsed_.end() && held->second.text == text ? held->second : parser.parseText(path, text);
  }
  parsed_ = std::move(kept);
}

std::vector<parse::ParsedFile> Workspace::files() const {
  std::vector<parse::ParsedFile> out;
  for (const auto& p : parsed_) out.push_back(p.second);
  return out;
}

service::Outcome Workspace::run(const std::string& name, const service::Request& request, emit::ISink& sink) {
  prepare(name, request);
  return useCases_.of(name)->run(model_, request, sink);
}

void Workspace::prepare(const std::string& name, const service::Request& request) {
  const std::unique_ptr<service::IUseCase> use = useCases_.of(name);
  const Api api(metamodel_);
  if (stale_) {  // bound once: what the use case reads of the bound model says what is checked
    Bound bound = api.bound(files());
    selection_ = selection_.joined(use->needs(bound.model, request));
    model_ = api.checked(std::move(bound), selection_);
    stale_ = false;
    ++loads_;
    return;
  }
  const check::Selection needs = use->needs(model_, request);
  if (selection_.covers(needs)) return;
  // a checked model takes no more steps: bound again, with every step asked so far
  selection_ = selection_.joined(needs);
  model_ = api.checked(api.bound(files()), selection_);
  ++loads_;
}

// Every answer is written by a JSON sink asked for here, at the engine's edge, and taken whole (DS-397).
std::string Workspace::texts(const std::vector<std::string>& list) {
  const auto sink = emit::SinkFactory::json(false);
  sink->beginList();
  for (const auto& t : list) sink->value(t);
  sink->endList();
  return sink->result();
}

std::string Workspace::text(const std::string& value) {
  const auto sink = emit::SinkFactory::json(false);
  sink->value(value);
  return sink->result();
}

std::string Workspace::exportModel() const {
  const auto sink = emit::SinkFactory::json(false);
  exporter::JsonExport(metamodel_).render(model_, *sink);
  return sink->result();
}

std::string Workspace::report() const { return Api(metamodel_).report(model_); }

std::string Workspace::find(const std::string& id) const {
  const auto sink = emit::SinkFactory::json(false);
  const model::Entity* e = model_.find(id);
  if (e)
    exporter::JsonExport(metamodel_).entity(*e, *sink);
  else
    sink->null();
  return sink->result();
}

std::string Workspace::relations(const std::vector<model::Relation>& relations) const {
  const auto sink = emit::SinkFactory::json(false);
  exporter::JsonExport(metamodel_).relations(relations, *sink);
  return sink->result();
}

std::string Workspace::from(const std::string& id) const { return relations(model_.index().from(id)); }

std::string Workspace::to(const std::string& id) const { return relations(model_.index().to(id)); }

std::string Workspace::touching(const std::string& id) const { return relations(model_.index().touching(id)); }

std::string Workspace::members(const std::string& scope) const { return texts(model_.index().members(scope)); }

std::string Workspace::parent(const std::string& id) const { return text(model_.index().parent(id)); }

std::string Workspace::children(const std::string& id) const { return texts(model_.index().children(id)); }

std::string Workspace::revisionOf(const std::string& artefact) const { return text(model_.index().revisionOf(artefact)); }

std::string Workspace::tree(const std::string& sequence) const {
  const model::SequenceTree tree(model_, sequence);
  const auto sink = emit::SinkFactory::json(false);
  sink->beginRecord().field("frames").beginList();
  for (const auto& f : tree.frames()) {
    sink->beginRecord()
        .field("at")
        .integer(tree.at(f))
        .field("else")
        .value(tree.elseOf(f))
        .field("else_scenario")
        .value(tree.scenario(f, true));
    sink->field("first").value(tree.first(f)).field("frame").value(tree.frameOf(f)).field("id").value(f);
    sink->field("kind").value(model_.find(f)->field("kind")).field("scenario").value(tree.scenario(f, false));
    sink->field("two_sides").boolean(tree.twoSides(f)).endRecord();
  }
  sink->endList().field("messages").beginList();
  for (const auto& m : tree.messages()) sink->beginRecord().field("frame").value(tree.frameOf(m)).field("id").value(m).endRecord();
  sink->endList().field("sequence").value(sequence).endRecord();
  return sink->result();
}

std::string Workspace::steps(const std::string& sequence, const std::string& path) const {
  const auto sink = emit::SinkFactory::json(false);
  sink->beginList();
  for (const auto& t : query::Walker(model_).steps(sequence, path))
    sink->beginRecord().field("message").value(t.message).field("next").value(t.next).field("next_path").value(t.nextPath).endRecord();
  sink->endList();
  return sink->result();
}

std::string Workspace::walk(const std::string& sequence, const std::string& path, int depth) const {
  const auto sink = emit::SinkFactory::json(false);
  sink->beginList();
  for (const auto& w : query::Walker(model_).walk(sequence, path, depth))
    sink->beginRecord().field("depth").integer(w.depth).field("message").value(w.message).endRecord();
  sink->endList();
  return sink->result();
}

std::string Workspace::answers(const std::vector<query::Answer>& answers) {
  const auto sink = emit::SinkFactory::json(false);
  format::DescriptionFactory::answers(answers)->describe(*sink);
  return sink->result();
}

// the questions asked through the query's interface, as the browser asks them (DS-405); shows is one of them
std::string Workspace::ask(const std::string& question, const std::string& subject) const {
  return answers(query::QueryFactory::of(model_, metamodel_)->ask(question, subject));
}

std::string Workspace::shows(const std::string& diagram) const { return ask("shows", diagram); }

std::string Workspace::render(const std::string& diagram, const std::string& format, int width) const {
  // a diagram is its IR: its layout and its cells are described in JSON, a line after (DS-398); the IR and the drawings
  // are written by their writers; a name that is none of them is a pair ds does not write, said before anything is drawn
  const bool described = format == "layout" || format == "cells";
  const auto writers = format::FormatFactory::diagram(theme_);
  const auto chosen = writers.find(format);
  if (!described && chosen == writers.end()) throw emit::NotImplemented("diagram", format, {"cells", "ir", "layout", "svg", "text"});
  const std::optional<render::IrDocument> document =
      render::IrExport(model_, metamodel_).document(model::Identity::ofDiagram(diagram).text());
  if (!document) return "";
  const auto sink = emit::SinkFactory::json(false);  // the IR travels compact to the writers that draw it (DS-418)
  format::DescriptionFactory::ir(*document)->describe(*sink);
  const render::IrText ir{sink->result()};
  if (described) {
    const auto description =
        format == "layout" ? format::DescriptionFactory::layout(ir, theme_, width) : format::DescriptionFactory::cells(ir, theme_, width);
    description->describe(*sink);
    return sink->result() + "\n";
  }
  format::FormatOptions options;
  options.width = width;
  return chosen->second->write(ir, options, *sink);
}

std::string Workspace::page(const std::string& title, const std::vector<std::string>& focus) const {
  format::FormatOptions options;
  options.title = title;
  options.focus = focus;
  const auto sink = emit::SinkFactory::json(false);  // the scene and the focus travel compact inside the page
  return format::FormatFactory::scene().at("html")->write(scene::Scene(model_, metamodel_).build(), options, *sink);
}

std::string Identity::qualify(const std::string& scope, const std::string& name) { return model::Scope(scope).qualify(name).text(); }
std::string Identity::outer(const std::string& scope) { return model::Scope(scope).outer().text(); }
std::string Identity::boxOf(const std::string& scope) { return model::Scope(scope).box().text(); }
std::string Identity::packageOf(const std::string& scope) { return model::Scope(scope).package().text(); }
std::string Identity::behaviourOf(const std::string& id) { return model::Identity(id).behaviour().text(); }
std::string Identity::subjectOf(const std::string& id) { return model::Identity(id).subject().text(); }
std::string Identity::scenarioOf(const std::string& id) { return model::Identity(id).scenario(); }
std::string Identity::withoutRevision(const std::string& id) { return model::ArtefactName(id).withoutRevision().text(); }

}  // namespace api
}  // namespace ds
