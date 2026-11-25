#include "ds/render/ClassProjector.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <utility>

#include "ds/query/ClassBoxes.hpp"
#include "ds/query/Filter.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace render {

using text::splitTrimmed;

nlohmann::json ClassProjector::project(const model::Entity& entity) const {
  const model::Entity* diagram = &entity;
  // `show` adds optional detail (ownership, multiplicity, operators, notes, derived-stereotypes);
  // `hide` removes compartments (fields, methods). Nothing written = members only.
  const std::vector<std::string> showList = splitTrimmed(diagram->field("show"), ',');
  const std::set<std::string> show(showList.begin(), showList.end());
  const std::vector<std::string> hideList = splitTrimmed(diagram->field("hide"), ',');
  const std::set<std::string> hide(hideList.begin(), hideList.end());
  auto shows = [&](const char* what) { return show.count(what) > 0; };
  auto hidden = [&](const char* what) { return hide.count(what) > 0; };

  // what the diagram takes is the query's filter of it (DS-127); the boxes around are drawn here
  const auto filter = query::FilterFactory::of(metamodel_, *diagram);
  const std::set<std::string> selected = filter->boxes(model_);  // through IFilter, never a concrete filter (DS-423)
  const std::set<std::string> containers = query::ClassBoxes::containers(model_, selected);
  auto up = [](const std::string& s) { return model::Scope(model::Scope(s).outer().text()).box().text(); };

  nlohmann::json out;
  out["nodes"] = nlohmann::json::array();
  out["edges"] = nlohmann::json::array();
  std::set<std::string> placed;

  auto parentOf = [&](const model::Entity& e) -> std::string {
    std::string s = model::Scope(e.scope).box().text();  // a package scope (app@core@) names the package app@core
    for (; !s.empty(); s = up(s)) {
      if (containers.count(s)) return s;
    }
    return "";
  };

  for (const auto& id : containers) {
    const model::Entity& m = *model_.find(id);
    const std::string label = id.substr(id.find_last_of('@') == std::string::npos ? 0 : id.find_last_of('@') + 1);  // without its packages
    nlohmann::json node = {
        {"id", id}, {"kind", "container"}, {"label", label}, {"classifier", m.kind == model::Kind::Package ? "package" : "namespace"}};
    const std::string parent = parentOf(m);
    if (!parent.empty()) node["parent"] = parent;
    out["nodes"].push_back(node);
    placed.insert(id);
  }

  for (const auto& id : selected) {
    const model::Entity& e = *model_.find(id);
    if (!isBox(e)) continue;
    nlohmann::json node = {{"id", id}, {"kind", "box"}, {"label", displayName(e)}};
    const std::string parent = parentOf(e);
    if (!parent.empty()) node["parent"] = parent;
    const std::string stereotypes = e.field("stereotypes");
    if (e.kind == model::Kind::Function) {
      node["classifier"] = "function";
      node["stereotype"] = "function";
      if (!hidden("methods")) {
        node["sections"] = nlohmann::json::array({nlohmann::json::array({memberLine(e)})});
        node["sectionKinds"] = nlohmann::json::array({"methods"});
      }
    } else if (e.kind == model::Kind::Enum) {
      node["classifier"] = "enum";
      node["stereotype"] = "enum";
      node["sections"] = nlohmann::json::array({splitTrimmed(e.field("values"), ',')});
      node["sectionKinds"] = nlohmann::json::array({"attributes"});
    } else {
      const bool interface = e.kind == model::Kind::Interface;
      node["classifier"] = interface ? "interface" : e.has("dataclass") ? "dataclass" : "class";
      node["abstract"] = interface || stereotypes.find("abstract") != std::string::npos;
      // the badge: derived stereotypes, then the operator symbols the families do not already say
      std::vector<std::string> badges;
      if (shows("derived-stereotypes")) badges = splitTrimmed(stereotypes, ',');
      if (shows("operators")) {
        for (const auto& op : splitTrimmed(e.field("operators"), ',')) {
          if (op != "==" && op != "!=" && op != "ordered" && op != "hash" && op != "print") badges.push_back(op);
        }
      }
      if (!badges.empty()) {
        std::string st;
        for (const auto& b : badges) st += (st.empty() ? "" : ", ") + b;
        node["stereotype"] = st;
      }
      std::vector<const model::Entity*> members;
      for (const auto& member : model_.index().members(id)) members.push_back(model_.find(member));
      std::sort(members.begin(), members.end(), [](const model::Entity* a, const model::Entity* b) {
        return a->location.line < b->location.line;  // source order, not identity order
      });
      std::vector<std::string> attributes, methods;
      for (const model::Entity* m : members) {
        if (m->kind == model::Kind::Attribute) attributes.push_back(memberLine(*m));
        if (m->kind == model::Kind::Method || m->kind == model::Kind::Operation) methods.push_back(memberLine(*m));
        if (m->kind == model::Kind::Operator && shows("operators") && !m->field("params").empty()) methods.push_back(memberLine(*m));
      }
      nlohmann::json sections = nlohmann::json::array();
      nlohmann::json kinds = nlohmann::json::array();
      if (!hidden("fields") && !attributes.empty()) {
        sections.push_back(attributes);
        kinds.push_back("attributes");
      }
      if (!hidden("methods") && !methods.empty()) {
        sections.push_back(methods);
        kinds.push_back("methods");
      }
      if (!sections.empty()) {
        node["sections"] = sections;
        node["sectionKinds"] = kinds;
      }
    }
    const std::string note = e.field("note");
    const std::string change = e.field("change");
    if (!change.empty()) {
      node["refs"] = {{"change", change}};
      const std::string detail = e.field("change-detail");
      const std::string from = e.field("change-from");
      node["title"] = change + (from.empty() ? "" : " from " + from) + (detail.empty() ? "" : ": " + detail);
    }
    out["nodes"].push_back(node);
    placed.insert(id);
    if (shows("notes") && !note.empty()) {
      out["nodes"].push_back({{"id", id + "::note"}, {"kind", "note"}, {"label", note}});
      out["edges"].push_back({{"from", id + "::note"}, {"to", id}, {"kind", "attachment"}});
    }
    if (shows("requirements")) {
      // the artefacts this entity realises or is bound by, one note
      std::vector<std::string> lines;
      for (const auto& r : model_.index().from(id)) {
        if (r.kind != "satisfies" && r.kind != "constrained-by") continue;
        const model::Entity* a = model_.find(r.to);
        if (!a || a->expected) continue;
        lines.push_back(a->id + "  " + a->field("title"));
      }
      if (!lines.empty()) {
        std::string text;
        for (const auto& l : lines) text += (text.empty() ? "" : "\n") + l;
        out["nodes"].push_back({{"id", id + "::artefacts"}, {"kind", "note"}, {"label", text}, {"classifier", "requirement"}});
        out["edges"].push_back({{"from", id + "::artefacts"}, {"to", id}, {"kind", "attachment"}});
      }
    }
  }

  // relations: lifted from members to their class; one edge per (from, to, kind, label)
  std::set<std::string> drawn;
  for (const auto& r : model_.index().all()) {
    std::string kind = edgeKind(r.kind);  // drawn as written: inheriting is inheritance (DS-301)
    if (kind.empty()) continue;
    const model::Entity* from = model_.find(r.from);
    const model::Entity* to = model_.find(r.to);
    if (!from || !to) continue;
    if (to->kind == model::Kind::Enum && kind == "composition") kind = "association";  // a value has no lifetime to own
    const std::string fromClass = query::ClassBoxes::drawnFrom(model_, *from);         // a behaviour's relation is its subject's (DS-219)
    if (!placed.count(fromClass) || !placed.count(r.to) || fromClass == r.to) continue;
    std::string label;
    if (r.kind == "owns" || r.kind == "refs" || r.kind == "shares") {
      if (shows("ownership")) label = from->name;
      if (shows("multiplicity") && !r.multiplicity.empty()) label += (label.empty() ? "" : " ") + r.multiplicity;
    } else if (r.kind == "bounded-by") {
      label = "T: " + model_.find(r.to)->name;
    }
    const std::string key = fromClass + "|" + r.to + "|" + kind + "|" + label;
    if (!drawn.insert(key).second) continue;
    nlohmann::json edge = {{"from", fromClass}, {"to", r.to}, {"kind", kind}};
    if (!label.empty()) edge["label"] = label;
    edge["refs"]["bond"] = axis::Cut::bondOf(r.kind);  // what it reads as between boxes, once lifted (DS-230)
    out["edges"].push_back(edge);
  }
  // what a box does counts as much as what it declares, with or without open: (DS-219): its behaviours cross
  // boxes too. Types alone would leave an interface unconnected to what handles it
  for (const auto& r : model_.index().ofKinds({"couples-transient", "couples-structural"})) {
    const model::Entity* seq = model_.find(r.from);
    if (!seq || !placed.count(r.to)) continue;
    const std::string owner = query::ClassBoxes::drawnFrom(model_, *seq);
    if (!placed.count(owner) || owner == r.to) continue;
    // an edge between the two already says it (an attribute's ownership, a type used): no second arrow
    const std::string pair = owner + "|" + r.to + "|";
    const auto at = drawn.lower_bound(pair);
    if (at != drawn.end() && at->compare(0, pair.size(), pair) == 0) continue;
    drawn.insert(pair + "dependency||");
    out["edges"].push_back({{"from", owner}, {"to", r.to}, {"kind", "dependency"}, {"refs", {{"bond", axis::Cut::bondOf(r.kind)}}}});
  }
  // a cut written (`open:`, `close:`, `cut:`) says which boxes close: the drawing is lifted onto it (DS-229)
  const axis::Cut cut(model_, metamodel_, *diagram);
  if (cut.written()) return closeBoxes(std::move(out), cut);
  return out;
}

nlohmann::json ClassProjector::closeBoxes(nlohmann::json doc, const axis::Cut& cut) const {
  auto outerOf = [](const std::string& id) { return model::Scope(model::Scope(id).outer().text()).box().text(); };  // the box outside
  // which boxes close and what stands for an id are the query's, as for what the diagram shows (DS-127)
  auto visible = [&](const std::string& id) { return cut.standsFor(id); };

  nlohmann::json nodes = nlohmann::json::array();
  for (auto n : doc["nodes"]) {
    const std::string id = n["id"];
    if (visible(id) != id) continue;
    if (cut.closed(id)) {
      // closed: a box that shows only what it offers, its public children
      std::vector<const model::Entity*> children;
      for (const auto& pair : model_.entities()) {
        const model::Entity& e = pair.second;
        if (outerOf(e.id) != id || e.expected) continue;
        if (e.kind != model::Kind::Interface && e.kind != model::Kind::Module && e.kind != model::Kind::Package &&
            e.kind != model::Kind::Class && e.kind != model::Kind::Enum && e.kind != model::Kind::Function) {
          continue;
        }
        if (e.field("visibility") != "-") children.push_back(&e);
      }
      // grouped by what they are, in one order for every box, and each group by name: a closed box reads as a
      // class does, a compartment for each kind of thing it offers (DS-244)
      n["kind"] = "box";
      nlohmann::json sections = nlohmann::json::array();
      nlohmann::json kinds = nlohmann::json::array();
      for (const char* kind : {"package", "namespace", "interface", "class", "dataclass", "enum", "function"}) {
        std::vector<std::string> names;
        for (const model::Entity* c : children)
          if (classifierOf(*c) == kind) names.push_back(displayName(*c));
        if (names.empty()) continue;
        std::sort(names.begin(), names.end());
        sections.push_back(names);
        kinds.push_back(kind);
      }
      if (!sections.empty()) {
        n["sections"] = sections;
        n["sectionKinds"] = kinds;
      }
    }
    nodes.push_back(n);
  }

  // an edge that loses an end to a closed box is lifted to it, and says which of the two bonds it is
  nlohmann::json edges = nlohmann::json::array();
  std::set<std::string> lifted;
  // what two drawn things already say of each other as they are is not said again by what is lifted to them: a
  // message written between two boxes and a call of the detail below are one use (DS-246)
  for (const auto& e : doc["edges"]) {
    const std::string from = e["from"], to = e["to"];
    if (visible(from) == from && visible(to) == to && e.contains("refs") && e["refs"].contains("bond"))
      lifted.insert(from + "|" + to + "|" + e["refs"]["bond"].get<std::string>());
  }
  // what a lifted arrow is made of: the interfaces of the box it reaches that the box it leaves goes through, each
  // with the operations asked of it (DS-266). The arrow says it when pointed at
  std::map<std::string, std::map<std::string, std::set<std::string>>> through;
  for (const auto& r : model_.index().ofKinds({"consumes", "uses", "calls"})) {
    const model::Entity* target = model_.find(r.to);
    if (!target) continue;
    const model::Entity* interface = target->kind == model::Kind::Interface ? target : model_.find(model::Scope(r.to).outer().text());
    if (!interface || interface->kind != model::Kind::Interface) continue;
    const std::string from = visible(r.from), to = visible(interface->id);
    if (from == to) continue;
    auto& operations = through[from + "|" + to][interface->name];
    if (target != interface) operations.insert(target->name);
  }
  auto crossed = [&](const std::string& from, const std::string& to) {
    std::string said;
    const auto at = through.find(from + "|" + to);
    if (at == through.end()) return said;
    for (const auto& [interface, operations] : at->second) {
      said += (said.empty() ? "" : "\n") + interface;
      std::string list;
      for (const auto& operation : operations) list += (list.empty() ? "" : ", ") + operation;
      if (!list.empty()) said += ": " + list;
    }
    return said;
  };
  for (auto e : doc["edges"]) {
    const std::string from = visible(e["from"]);
    const std::string to = visible(e["to"]);
    if (from == to) continue;  // what happens inside a closed box stays inside it
    if (from != e["from"].get<std::string>() || to != e["to"].get<std::string>()) {
      // lifted, a relation is one of two things, a use or a structural bond: one arrow of each at most (DS-230)
      const std::string bond = e.contains("refs") ? e["refs"].value("bond", "uses") : std::string("uses");
      if (!lifted.insert(from + "|" + to + "|" + bond).second) continue;
      e = {{"from", from}, {"to", to}, {"kind", bond == "structural" ? "association" : "dependency"}, {"refs", {{"bond", bond}}}};
    }
    // between two boxes, a use says which interfaces it goes through, lifted or written between them as they are
    const bool structural = e.contains("refs") && e["refs"].value("bond", "") == "structural";
    if (!structural && !e.contains("title") && !crossed(from, to).empty()) e["title"] = crossed(from, to);
    edges.push_back(e);
  }
  doc["nodes"] = nodes;
  doc["edges"] = edges;
  return doc;
}

bool ClassProjector::isBox(const model::Entity& e) {
  return e.kind == model::Kind::Class || e.kind == model::Kind::Enum || e.kind == model::Kind::Function || e.kind == model::Kind::Interface;
}

std::string ClassProjector::displayName(const model::Entity& e) {
  const std::string params = e.kind == model::Kind::Class ? e.field("params") : "";  // a function's params are its signature
  return params.empty() ? e.name : e.name + "<" + params + ">";
}

std::string ClassProjector::memberLine(const model::Entity& m) {
  std::string line = m.field("visibility");
  if (!line.empty()) line += " ";
  for (const auto& f : m.flags) {
    if (f == "abstract" || f == "static" || f == "const") line += f + " ";
  }
  if (m.kind == model::Kind::Attribute) {
    line += m.name + ": " + m.field("type");
    const std::string def = m.field("default");
    if (!def.empty()) line += " = " + def;
    return line;
  }
  line += m.name + "(" + m.field("params") + ")";
  const std::string returns = m.field("returns");
  if (!returns.empty()) line += " -> " + returns;
  return line;
}

std::string ClassProjector::classifierOf(const model::Entity& e) {
  if (e.kind == model::Kind::Package) return "package";
  if (e.kind == model::Kind::Module) return "namespace";
  if (e.kind == model::Kind::Function) return "function";
  if (e.kind == model::Kind::Enum) return "enum";
  if (e.kind == model::Kind::Interface) return "interface";
  return e.has("dataclass") ? "dataclass" : "class";
}

std::string ClassProjector::edgeKind(const std::string& relation) {
  if (relation == "inherits") return "inheritance";
  if (relation == "owns") return "composition";
  if (relation == "refs" || relation == "shares") return "aggregation";
  if (relation == "uses" || relation == "bounded-by" || relation == "consumes" || relation == "sends")
    return "dependency";  // @consumes is drawn (DS-219)
  if (relation == "realises") return "realization";
  return "";
}

}  // namespace render
}  // namespace ds
