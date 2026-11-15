#include "ds/api/Browser.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/api/Workspace.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/Model.hpp"
#include "ds/query/Query.hpp"

namespace ds {
namespace api {

namespace {

struct Drawn {
  std::string id;
  std::string kind;
  std::string include;
  model::Location location;
};

// the artefacts as rows: what each one refines, what keeps it, what proves it
nlohmann::json artefacts(const model::Model& model) {
  nlohmann::json rows = nlohmann::json::array();
  for (const auto& p : model.entities()) {
    const model::Entity& e = p.second;
    if (e.expected) continue;
    if (e.kind != model::Kind::Requirement && e.kind != model::Kind::Constraint && e.kind != model::Kind::UseCase) continue;
    std::string level;
    if (e.kind == model::Kind::Requirement)
      level =
          e.scope.empty() || model::Scope(e.scope).isSystem() ? "system" : model::Identity(model::Scope(e.scope).box().text()).lastName();
    nlohmann::json row{{"id", e.id},
                       {"kind", model::kindName(e.kind)},
                       {"title", e.field("title")},
                       {"status", e.field("status")},
                       {"level", level},
                       {"parent", ""},
                       {"keptBy", nlohmann::json::array()},
                       {"provedBy", nlohmann::json::array()}};
    for (const auto& r : model.index().from(e.id))
      if (r.kind == "refines" || (e.kind == model::Kind::UseCase && r.kind == "requires")) row["parent"] = r.to;
    for (const auto& r : model.index().to(e.id)) {
      if (r.kind == "requires" || r.kind == "constrained-by" || r.kind == "realises") row["keptBy"].push_back(r.from);
      if (r.kind == "verifies") row["provedBy"].push_back(r.from);
    }
    rows.push_back(row);
  }
  return rows;
}

nlohmann::json diagrams(const Workspace& workspace, int width) {
  std::vector<Drawn> drawn;  // in the order they are written: file, then line
  for (const auto& p : workspace.model().entities()) {
    const model::Entity& e = p.second;
    if (e.kind != model::Kind::Diagram) continue;
    const auto kind = e.fields.find("kind");
    const auto include = e.fields.find("include");
    drawn.push_back({model::Identity(e.id).diagramName(), kind == e.fields.end() ? "class" : kind->second,
                     include == e.fields.end() ? "" : include->second, e.location});
  }
  std::stable_sort(drawn.begin(), drawn.end(), [](const Drawn& a, const Drawn& b) {
    return a.location.file != b.location.file ? a.location.file < b.location.file : a.location.line < b.location.line;
  });
  nlohmann::json out = nlohmann::json::array();
  for (const auto& d : drawn) {
    nlohmann::json one{{"id", d.id}, {"kind", d.kind}, {"include", d.include}, {"file", d.location.file}, {"line", d.location.line}};
    one["shows"] = nlohmann::json::array();  // what it takes: a call leads to the diagram of what it calls
    for (const auto& a : query::Query(workspace.model()).shows(d.id)) one["shows"].push_back(a.id);
    try {
      one["svg"] = workspace.render(d.id, "svg", width);
    } catch (const std::exception& ex) {
      one["svg"] = "";
      one["error"] = ex.what();
    }
    out.push_back(one);
  }
  return out;
}

std::string respond(const std::string& request) {
  const auto in = nlohmann::json::parse(request);
  Files files;
  for (const auto& f : in.at("files").items()) files[f.key()] = f.value().get<std::string>();
  const Workspace workspace(files);
  nlohmann::json out;
  out["report"] = nlohmann::json::parse(workspace.report());
  out["entities"] = nlohmann::json::array();
  out["locations"] = nlohmann::json::object();  // where each identity is written: where a click on a drawing leads
  for (const auto& p : workspace.model().entities()) {
    if (p.second.expected) continue;
    out["entities"].push_back(p.first);
    const auto& at = p.second.location;
    if (!at.file.empty()) out["locations"][p.first] = {at.file, at.line, at.endLine > 0 ? at.endLine : at.line};
  }
  out["artefacts"] = artefacts(workspace.model());
  out["diagrams"] = diagrams(workspace, in.value("width", 0));
  return out.dump();
}

}  // namespace

std::string Browser::answer(const std::string& request) {
  try {
    return respond(request);
  } catch (const std::exception& ex) {
    return nlohmann::json{{"error", ex.what()}}.dump();
  }
}

}  // namespace api
}  // namespace ds
