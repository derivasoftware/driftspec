#include "ds/api/Workspace.hpp"

#include <nlohmann/json.hpp>

#include "ds/api/Api.hpp"
#include "ds/check/Report.hpp"
#include "ds/diff/Diff.hpp"
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

namespace {

model::Model loaded(const Files& files) {
  parse::Parser parser;
  std::vector<parse::ParsedFile> parsed;
  for (const auto& file : files) parsed.push_back(parser.parseText(file.first, file.second));
  return Api::load(parsed);
}

std::string ids(const std::vector<std::string>& list) { return nlohmann::json(list).dump(); }

}  // namespace

Workspace::Workspace(const Files& files) : model_(loaded(files)) {}

std::string Workspace::exportModel() const { return exporter::JsonExport().render(model_, false); }

std::string Workspace::report() const { return Api::report(model_); }

std::string Workspace::find(const std::string& id) const {
  const model::Entity* e = model_.find(id);
  return e ? exporter::JsonExport().entity(*e) : "null";
}

std::string Workspace::from(const std::string& id) const { return exporter::JsonExport().relations(model_.index().from(id)); }

std::string Workspace::to(const std::string& id) const { return exporter::JsonExport().relations(model_.index().to(id)); }

std::string Workspace::touching(const std::string& id) const { return exporter::JsonExport().relations(model_.index().touching(id)); }

std::string Workspace::members(const std::string& scope) const { return ids(model_.index().members(scope)); }

std::string Workspace::parent(const std::string& id) const { return nlohmann::json(model_.index().parent(id)).dump(); }

std::string Workspace::children(const std::string& id) const { return ids(model_.index().children(id)); }

std::string Workspace::revisionOf(const std::string& artefact) const { return nlohmann::json(model_.index().revisionOf(artefact)).dump(); }

std::string Workspace::tree(const std::string& sequence) const {
  const model::SequenceTree tree(model_, sequence);
  nlohmann::json frames = nlohmann::json::array();
  for (const auto& f : tree.frames()) {
    frames.push_back({{"id", f},
                      {"kind", model_.find(f)->field("kind")},
                      {"frame", tree.frameOf(f)},
                      {"at", tree.at(f)},
                      {"first", tree.first(f)},
                      {"else", tree.elseOf(f)},
                      {"two_sides", tree.twoSides(f)},
                      {"scenario", tree.scenario(f, false)},
                      {"else_scenario", tree.scenario(f, true)}});
  }
  nlohmann::json messages = nlohmann::json::array();
  for (const auto& m : tree.messages()) messages.push_back({{"id", m}, {"frame", tree.frameOf(m)}});
  return nlohmann::json{{"sequence", sequence}, {"messages", messages}, {"frames", frames}}.dump();
}

std::string Workspace::steps(const std::string& sequence, const std::string& path) const {
  nlohmann::json out = nlohmann::json::array();
  for (const auto& t : query::stepsOf(model_, sequence, path))
    out.push_back({{"message", t.message}, {"next", t.next}, {"next_path", t.nextPath}});
  return out.dump();
}

std::string Workspace::walk(const std::string& sequence, const std::string& path, int depth) const {
  nlohmann::json out = nlohmann::json::array();
  for (const auto& w : query::walkOf(model_, sequence, path, depth)) out.push_back({{"message", w.message}, {"depth", w.depth}});
  return out.dump();
}

std::string Workspace::ask(const std::string& question, const std::string& subject) const {
  return format::AnswersJson().write(query::Query(model_).ask(question, subject), {});
}

std::string Workspace::shows(const std::string& diagram) const {
  return format::AnswersJson().write(query::Query(model_).shows(diagram), {});
}

std::string Workspace::render(const std::string& diagram, const std::string& format, int width) const {
  const std::string ir = render::IrExport(model_).render(model::diagramId(diagram), false);
  if (ir.empty()) return ir;
  format::FormatOptions options;
  options.width = width;
  const auto formats = format::diagramFormats();
  const auto chosen = formats.find(format);
  return (chosen != formats.end() ? chosen->second : formats.at("text"))->write({ir}, options);
}

std::string Workspace::page(const std::string& title, const std::vector<std::string>& focus) const {
  format::FormatOptions options;
  options.title = title;
  options.focus = focus;
  return format::SceneHtml().write(scene::Scene(model_).build(), options);
}

std::string Workspace::diff(const Workspace& before) const {
  const diff::DiffResult result = diff::Diff::compare(before.model_, model_);
  nlohmann::json changes = nlohmann::json::array();
  for (const auto& c : result.changes) {
    changes.push_back({{"kind", c.kind},
                       {"what", c.what},
                       {"id", c.id},
                       {"entity", c.entity},
                       {"scope", c.scope},
                       {"detail", c.detail},
                       {"from", c.from},
                       {"label", c.label}});
  }
  nlohmann::json findings = nlohmann::json::array();
  for (const auto& f : result.findings) {
    findings.push_back({{"code", f.code},
                        {"severity", f.severity},
                        {"message", f.message},
                        {"location", {{"file", f.location.file}, {"line", f.location.line}, {"column", f.location.column}}}});
  }
  return nlohmann::json{{"changes", changes}, {"findings", findings}}.dump();
}

std::string Identity::qualify(const std::string& scope, const std::string& name) { return model::qualify(scope, name); }
std::string Identity::outer(const std::string& scope) { return model::outer(scope); }
std::string Identity::boxOf(const std::string& scope) { return model::boxOf(scope); }
std::string Identity::packageOf(const std::string& scope) { return model::packageOf(scope); }
std::string Identity::behaviourOf(const std::string& id) { return model::behaviourOf(id); }
std::string Identity::subjectOf(const std::string& id) { return model::subjectOf(id); }
std::string Identity::scenarioOf(const std::string& id) { return model::scenarioOf(id); }
std::string Identity::withoutRevision(const std::string& id) { return model::withoutRevision(id); }

}  // namespace api
}  // namespace ds
