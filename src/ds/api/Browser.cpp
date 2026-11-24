#include "ds/api/Browser.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/api/Workspace.hpp"
#include "ds/check/Report.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/format/Format.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/Manual.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/query/Query.hpp"

namespace ds {
namespace api {

// the artefacts as rows: what each one refines, what keeps it, what proves it; each kind named by the metamodel
void Browser::artefacts(const model::Model& model, emit::ISink& out_sink) const {
  out_sink.beginList();
  for (const auto& p : model.entities()) {
    const model::Entity& e = p.second;
    if (e.expected) continue;
    if (e.kind != model::Kind::Requirement && e.kind != model::Kind::Constraint && e.kind != model::Kind::UseCase) continue;
    std::string level;
    if (e.kind == model::Kind::Requirement)
      level =
          e.scope.empty() || model::Scope(e.scope).isSystem() ? "system" : model::Identity(model::Scope(e.scope).box().text()).lastName();
    std::string parent;
    for (const auto& r : model.index().from(e.id))
      if (r.kind == "refines" || (e.kind == model::Kind::UseCase && r.kind == "satisfies")) parent = r.to;
    const std::vector<model::Relation> reaching = model.index().to(e.id);
    out_sink.beginRecord().field("id").value(e.id).field("keptBy").beginList();
    for (const auto& r : reaching)
      if (r.kind == "satisfies" || r.kind == "constrained-by" || r.kind == "realises") out_sink.value(r.from);
    out_sink.endList().field("kind").value(metamodel_.nameOf(e.kind)).field("level").value(level).field("parent").value(parent);
    out_sink.field("provedBy").beginList();
    for (const auto& r : reaching)
      if (r.kind == "verifies") out_sink.value(r.from);
    out_sink.endList().field("status").value(e.field("status")).field("title").value(e.field("title")).endRecord();
  }
  out_sink.endList();
}

// the diagrams in the order they are written, file then line, each drawn as svg with what it takes
void Browser::diagrams(const Workspace& workspace, int width, emit::ISink& out_sink) const {
  std::vector<const model::Entity*> drawn;
  for (const auto& p : workspace.model().entities())
    if (p.second.kind == model::Kind::Diagram) drawn.push_back(&p.second);
  std::stable_sort(drawn.begin(), drawn.end(), [](const model::Entity* a, const model::Entity* b) {
    return a->location.file != b->location.file ? a->location.file < b->location.file : a->location.line < b->location.line;
  });
  const auto questions = query::QueryFactory::of(workspace.model(), metamodel_);  // what each diagram takes
  out_sink.beginList();
  for (const model::Entity* e : drawn) {
    const std::string id = model::Identity(e->id).diagramName();
    const auto kind = e->fields.find("kind");
    const auto include = e->fields.find("include");
    std::string svg, error;
    bool failed = false;
    try {
      svg = workspace.render(id, "svg", width);
    } catch (const std::exception& ex) {
      error = ex.what();
      failed = true;
    }
    out_sink.beginRecord();
    if (failed) out_sink.field("error").value(error);
    out_sink.field("file").value(e->location.file).field("id").value(id);
    out_sink.field("include").value(include == e->fields.end() ? "" : include->second);
    out_sink.field("kind").value(kind == e->fields.end() ? "class" : kind->second).field("line").integer(e->location.line);
    out_sink.field("shows").beginList();  // what it takes: a call leads to the diagram of what it calls
    for (const auto& a : questions->ask("shows", id)) out_sink.value(a.id);
    out_sink.endList().field("svg").value(svg).endRecord();
  }
  out_sink.endList();
}

void Browser::respond(const std::string& request, emit::ISink& out_sink) const {
  const auto in = nlohmann::json::parse(request);  // the request is read; the answer is written by the sink
  if (in.contains("manual")) {                     // a page of the manual, or with "" its guides and topics (DS-272)
    const std::string topic = in["manual"].get<std::string>();
    const model::Manual manual(metamodel_);  // made from the metamodel the browser's engine holds (DS-342)
    out_sink.beginRecord().field("manual").beginRecord();
    if (topic.empty()) {
      out_sink.field("guides").beginList();
      for (const auto& guide : manual.guides()) out_sink.value(guide.first);
      out_sink.endList();
    }
    out_sink.field("text").value(manual.page(topic)).field("topic").value(topic);
    if (topic.empty()) {
      out_sink.field("topics").beginList();
      for (const auto& t : manual.topics()) out_sink.value(t);
      out_sink.endList();
    }
    out_sink.endRecord().endRecord();
    return;
  }
  Files files;
  for (const auto& f : in.at("files").items()) files[f.key()] = f.value().get<std::string>();
  const Workspace workspace(metamodel_, files);
  out_sink.beginRecord().field("artefacts");
  artefacts(workspace.model(), out_sink);
  out_sink.field("diagrams");
  diagrams(workspace, in.value("width", 0), out_sink);
  out_sink.field("entities").beginList();
  for (const auto& p : workspace.model().entities())
    if (!p.second.expected) out_sink.value(p.first);
  out_sink.endList().field("locations").beginRecord();  // where each identity is written: where a click on a drawing leads
  for (const auto& p : workspace.model().entities()) {
    const auto& at = p.second.location;
    if (p.second.expected || at.file.empty()) continue;
    out_sink.field(p.first).beginList().value(at.file).integer(at.line).integer(at.endLine > 0 ? at.endLine : at.line).endList();
  }
  out_sink.endRecord().field("report");
  const check::Report report = check::Report::of(workspace.model(), metamodel_);
  format::common::ReportEncoder(report).encode(out_sink);  // as ds check --json writes it
  out_sink.endRecord();
}

std::string Browser::answer(const std::string& request) const {
  // the answer is written by a JSON sink asked for here, at the engine's edge, and taken whole (DS-397)
  try {
    const auto sink = emit::SinkFactory::json(false);
    respond(request, *sink);
    return sink->result();
  } catch (const std::exception& ex) {
    const auto sink = emit::SinkFactory::json(false);
    sink->beginRecord().field("error").value(ex.what()).endRecord();
    return sink->result();
  }
}

}  // namespace api
}  // namespace ds
