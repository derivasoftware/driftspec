#include "ds/render/RequirementProjector.hpp"

#include <nlohmann/json.hpp>

#include "ds/axis/Cut.hpp"
#include "ds/query/Filter.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace render {

using text::splitTrimmed;

nlohmann::json RequirementProjector::project(const model::Entity& diagram) const {
  const std::vector<std::string> showList = splitTrimmed(diagram.field("show"), ',');
  const std::set<std::string> show(showList.begin(), showList.end());
  const bool withDesign = show.empty() || show.count("design");
  const bool withVerifications = show.empty() || show.count("verifications");
  // the roots and their umbrella are the query's filter of the diagram (DS-127)
  const std::set<std::string> selected =
      query::RequirementsFilter(metamodel_, splitTrimmed(diagram.field("include"), ','), withDesign, withVerifications, diagram.id)
          .select(model_);
  const axis::Cut cut(model_, metamodel_, diagram);  // what answers to a requirement is the box that stands for it (DS-231)
  nlohmann::json out;
  out["nodes"] = nlohmann::json::array();
  out["edges"] = nlohmann::json::array();
  for (const auto& id : selected) {
    const model::Entity& e = *model_.find(id);
    // an artefact, or a use case, is drawn as its card: id, stereotype with its status, title
    const bool card = isArtefact(e) || e.kind == model::Kind::UseCase;
    nlohmann::json node = {{"id", id}, {"kind", "box"}, {"label", card ? e.id : e.name}, {"classifier", metamodel_.nameOf(e.kind)}};
    if (card) {
      std::vector<std::string> lines = text::wrap(e.field("title"), 44);
      const std::string status = e.field("status");
      const std::string method = e.field("method");
      std::string stereotype = metamodel_.nameOf(e.kind);
      if (!status.empty()) stereotype += ", " + status;
      if (!method.empty()) stereotype += ", " + method;
      if (e.kind == model::Kind::Requirement)
        stereotype += e.scope.empty() || model::Scope(e.scope).isSystem()
                          ? ", system"
                          : ", " + model::Identity(model::Scope(e.scope).box().text()).lastName();
      node["stereotype"] = stereotype;
      if (!lines.empty()) node["sections"] = nlohmann::json::array({lines});
      const std::string text = e.field("text");
      if (!text.empty()) node["title"] = text;
      if (!e.location.file.empty()) node["refs"] = {{"location", e.location.file + ":" + std::to_string(e.location.line)}};
    } else {
      node["parent"] = nlohmann::json();  // no containers in this view
      node.erase("parent");
      node["stereotype"] = e.field("stereotypes");
      if (node["stereotype"].get<std::string>().empty()) node.erase("stereotype");
    }
    const std::string change = e.field("change");
    if (!change.empty()) node["refs"]["change"] = change;
    out["nodes"].push_back(node);
  }
  std::set<std::string> said;  // one edge per pair: a class and its method that answer the same requirement draw once
  for (model::Relation r : model_.index().all()) {
    if ((r.kind == "satisfies" || r.kind == "constrained-by") && !selected.count(r.from)) {
      const model::Entity* from = model_.find(r.from);  // a member answers through the class that holds it (DS-220)
      if (from && selected.count(from->scope)) {
        r.from = from->scope;
      } else if (selected.count(cut.standsFor(r.from))) {
        r.from = cut.standsFor(r.from);  // and on a cut, through the box that stands for it (DS-231)
      }
    }
    if (!selected.count(r.from) || !selected.count(r.to)) continue;
    if (!said.insert(r.kind + "|" + r.from + "|" + r.to).second) continue;
    if (r.kind == "refines") {
      out["edges"].push_back({{"from", r.from}, {"to", r.to}, {"kind", "realization"}, {"label", "refines"}});  // the parent above
    } else if (r.kind == "verifies") {
      out["edges"].push_back({{"from", r.to}, {"to", r.from}, {"kind", "dependency"}, {"label", "verified by"}});
    } else if (r.kind == "satisfies") {
      out["edges"].push_back({{"from", r.to}, {"to", r.from}, {"kind", "dependency"}, {"label", "satisfied by"}});
    } else if (r.kind == "constrained-by") {
      out["edges"].push_back({{"from", r.to}, {"to", r.from}, {"kind", "dependency"}, {"label", "constrains"}});
    }
  }
  if (out["nodes"].empty()) out["notice"] = "No artefact selected: include names a requirement, a package (p@*) or a module (p@m).";
  return out;
}

bool RequirementProjector::isArtefact(const model::Entity& e) {
  return e.kind == model::Kind::Requirement || e.kind == model::Kind::Constraint || e.kind == model::Kind::Verification;
}

}  // namespace render
}  // namespace ds
