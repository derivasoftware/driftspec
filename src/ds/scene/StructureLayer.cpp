#include <algorithm>
#include <map>
#include <set>
#include <tuple>

#include "Shared.hpp"
#include "ds/query/Filter.hpp"
#include "ds/query/Query.hpp"
#include "ds/scene/Layers.hpp"

namespace ds {
namespace scene {

StructureLayer::StructureLayer(const model::Model& model, const Map& map) : model_(model), map_(map) {}

// the relations between boxes, lifted to what the map draws; where each box is drawn; the titles of what
// the nodes answer to
void StructureLayer::decorate(SceneDocument& out_document) {
  static const std::set<std::string> kinds{"inherits", "realises", "owns", "shares", "refs", "uses", "consumes", "calls"};
  std::map<std::tuple<std::string, std::string, std::string>, Link> links;
  std::map<std::tuple<std::string, std::string, std::string>, std::set<std::string>> sequences;
  for (const auto& r : model_.relations()) {
    if (!kinds.count(r.kind)) continue;
    const std::string from = map_.land(r.from), to = map_.land(r.to);
    if (from.empty() || to.empty() || from == to || map_.parentOf(from) == to || map_.parentOf(to) == from) continue;  // within one box
    // a class that inherits from an interface realises it (R14): read as every drawing reads it
    const std::string kind = model::readAs(model_, r);
    const auto key = std::make_tuple(from, to, kind);
    Link& link = links[key];
    link.from = from;
    link.to = to;
    link.kind = kind;
    ++link.count;
    if (!r.multiplicity.empty()) link.multiplicity = r.multiplicity;
    if (r.kind == "calls") sequences[key].insert(r.from.substr(0, r.from.rfind("::")));
  }
  for (auto& pair : links) {
    for (const auto& s : sequences[pair.first]) pair.second.sequences.push_back(s);
    out_document.links.push_back(pair.second);
  }
  // where each box is drawn: every diagram of the workspace, taken by the filter of its kind (DS-127)
  const query::Query query(model_);
  for (const auto& pair : model_.entities()) {
    const auto filter = query::filterFor(pair.second);
    if (!filter) continue;
    DiagramView view;
    view.name = model::diagramName(pair.first);
    view.kind = pair.second.fields.count("kind") ? pair.second.fields.at("kind") : "";
    std::set<std::string> shown;
    for (const auto& id : query.select(*filter)) {
      const std::string node = map_.land(id);
      if (!node.empty() && shown.insert(node).second) view.nodes.push_back(node);
    }
    if (!view.nodes.empty()) out_document.diagrams.push_back(view);
  }
  for (const auto& n : out_document.nodes) {
    for (const auto& r : n.requirements) {
      const model::Entity* e = model_.find(r);
      const auto title = e ? e->fields.find("title") : std::map<std::string, std::string>::const_iterator();
      if (e && title != e->fields.end()) out_document.titles[r] = title->second;
    }
  }
}

}  // namespace scene
}  // namespace ds
