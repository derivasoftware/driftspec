#include <algorithm>
#include <map>
#include <set>
#include <tuple>

#include "ds/query/Filter.hpp"
#include "ds/query/Query.hpp"
#include "ds/scene/Layers.hpp"

namespace ds {
namespace scene {

StructureLayer::StructureLayer(const model::Model& model, const model::Metamodel& metamodel, const Map& map)
    : model_(model), metamodel_(metamodel), map_(map) {}

// the relations between boxes, lifted to what the map draws; where each box is drawn; the titles of what
// the nodes answer to
void StructureLayer::decorate(SceneDocument& out_document) {
  links(out_document);
  diagrams(out_document);
  titles(out_document);
}

// each relation between two boxes, lifted to what the map draws, counted, with the calls that make it
void StructureLayer::links(SceneDocument& out_document) const {
  std::map<std::tuple<std::string, std::string, std::string>, Link> links;
  std::map<std::tuple<std::string, std::string, std::string>, std::set<std::string>> sequences;
  for (const auto& r : model_.index().ofKinds({"inherits", "realises", "owns", "shares", "refs", "uses", "consumes", "calls"})) {
    const std::string from = map_.land(r.from), to = map_.land(r.to);
    if (from.empty() || to.empty() || from == to || map_.parentOf(from) == to || map_.parentOf(to) == from) continue;  // within one box
    const std::string kind = r.kind;  // drawn as written: inheriting is inheritance (DS-301)
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
}

void StructureLayer::diagrams(SceneDocument& out_document) const {
  // where each box is drawn: every diagram of the workspace, taken by the filter of its kind (DS-127)
  const query::Query query(model_, metamodel_);
  const query::Filters filters(metamodel_);
  for (const auto& pair : model_.entities()) {
    const auto filter = filters.of(pair.second);
    if (!filter) continue;
    DiagramView view;
    view.name = model::Identity(pair.first).diagramName();
    view.kind = pair.second.fields.count("kind") ? pair.second.fields.at("kind") : "";
    std::set<std::string> shown;
    for (const auto& id : query.select(*filter)) {
      const std::string node = map_.land(id);
      if (!node.empty() && shown.insert(node).second) view.nodes.push_back(node);
    }
    if (!view.nodes.empty()) out_document.diagrams.push_back(view);
  }
}

// the titles of what the nodes answer to
void StructureLayer::titles(SceneDocument& out_document) const {
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
