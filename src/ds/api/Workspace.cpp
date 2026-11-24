#include "ds/api/Workspace.hpp"

#include "ds/api/Api.hpp"
#include "ds/check/Report.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/format/DiagramFormats.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/SequenceTree.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"
#include "ds/query/Walk.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/scene/Scene.hpp"

namespace ds {
namespace api {

Workspace::Workspace(const model::Metamodel& metamodel, const Files& files, Output output, const std::string& diagram)
    : metamodel_(metamodel), output_(output), diagram_(diagram), model_(loaded(files)) {}

void Workspace::reload(const Files& files) { model_ = loaded(files); }

model::Model Workspace::loaded(const Files& files) {
  parse::Parser parser;
  std::map<std::string, parse::ParsedFile> kept;
  std::vector<parse::ParsedFile> parsed;
  for (const auto& [path, text] : files) {
    const auto held = parsed_.find(path);
    kept[path] = held != parsed_.end() && held->second.text == text ? held->second : parser.parseText(path, text);
    parsed.push_back(kept[path]);
  }
  parsed_ = std::move(kept);
  return Api(metamodel_).load(parsed, output_, diagram_);
}

// Every answer is written by a JSON sink built here, at the engine's edge, and taken whole (DS-397).
std::string Workspace::texts(const std::vector<std::string>& list) {
  emit::JsonSink sink(false);
  sink.beginList();
  for (const auto& t : list) sink.value(t);
  sink.endList();
  return sink.result();
}

std::string Workspace::text(const std::string& value) {
  emit::JsonSink sink(false);
  sink.value(value);
  return sink.result();
}

std::string Workspace::exportModel() const {
  emit::JsonSink sink(false);
  exporter::JsonExport(metamodel_).render(model_, sink);
  return sink.result();
}

std::string Workspace::report() const { return Api(metamodel_).report(model_); }

std::string Workspace::find(const std::string& id) const {
  emit::JsonSink sink(false);
  const model::Entity* e = model_.find(id);
  if (e)
    exporter::JsonExport(metamodel_).entity(*e, sink);
  else
    sink.null();
  return sink.result();
}

std::string Workspace::relations(const std::vector<model::Relation>& relations) const {
  emit::JsonSink sink(false);
  exporter::JsonExport(metamodel_).relations(relations, sink);
  return sink.result();
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
  emit::JsonSink sink(false);
  sink.beginRecord().field("frames").beginList();
  for (const auto& f : tree.frames()) {
    sink.beginRecord()
        .field("at")
        .integer(tree.at(f))
        .field("else")
        .value(tree.elseOf(f))
        .field("else_scenario")
        .value(tree.scenario(f, true));
    sink.field("first").value(tree.first(f)).field("frame").value(tree.frameOf(f)).field("id").value(f);
    sink.field("kind").value(model_.find(f)->field("kind")).field("scenario").value(tree.scenario(f, false));
    sink.field("two_sides").boolean(tree.twoSides(f)).endRecord();
  }
  sink.endList().field("messages").beginList();
  for (const auto& m : tree.messages()) sink.beginRecord().field("frame").value(tree.frameOf(m)).field("id").value(m).endRecord();
  sink.endList().field("sequence").value(sequence).endRecord();
  return sink.result();
}

std::string Workspace::steps(const std::string& sequence, const std::string& path) const {
  emit::JsonSink sink(false);
  sink.beginList();
  for (const auto& t : query::Walker(model_).steps(sequence, path))
    sink.beginRecord().field("message").value(t.message).field("next").value(t.next).field("next_path").value(t.nextPath).endRecord();
  sink.endList();
  return sink.result();
}

std::string Workspace::walk(const std::string& sequence, const std::string& path, int depth) const {
  emit::JsonSink sink(false);
  sink.beginList();
  for (const auto& w : query::Walker(model_).walk(sequence, path, depth))
    sink.beginRecord().field("depth").integer(w.depth).field("message").value(w.message).endRecord();
  sink.endList();
  return sink.result();
}

std::string Workspace::ask(const std::string& question, const std::string& subject) const {
  emit::JsonSink sink(false);
  return format::AnswersJson().write(query::Query(model_, metamodel_).ask(question, subject), {}, sink);
}

std::string Workspace::shows(const std::string& diagram) const {
  emit::JsonSink sink(false);
  return format::AnswersJson().write(query::Query(model_, metamodel_).shows(diagram), {}, sink);
}

std::string Workspace::render(const std::string& diagram, const std::string& format, int width) const {
  const std::string ir = render::IrExport(model_, metamodel_).render(model::Identity::ofDiagram(diagram).text(), false);
  if (ir.empty()) return ir;
  format::FormatOptions options;
  options.width = width;
  const auto formats = format::diagramFormats(theme_);
  const auto chosen = formats.find(format);
  emit::JsonSink sink(false);  // what layout and cells write
  return (chosen != formats.end() ? chosen->second : formats.at("text"))->write({ir}, options, sink);
}

std::string Workspace::page(const std::string& title, const std::vector<std::string>& focus) const {
  format::FormatOptions options;
  options.title = title;
  options.focus = focus;
  emit::JsonSink sink(false);  // the scene and the focus travel compact inside the page
  return format::SceneHtml().write(scene::Scene(model_, metamodel_).build(), options, sink);
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
