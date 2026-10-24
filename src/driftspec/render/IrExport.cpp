#include "driftspec/render/IrExport.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <sstream>

namespace driftspec {
namespace render {

namespace {

std::vector<std::string> split(const std::string& text, char sep) {
  std::vector<std::string> out;
  std::stringstream in(text);
  std::string item;
  while (std::getline(in, item, sep)) {
    const auto b = item.find_first_not_of(' ');
    const auto e = item.find_last_not_of(' ');
    if (b != std::string::npos) out.push_back(item.substr(b, e - b + 1));
  }
  return out;
}

std::string fieldOf(const model::Entity& e, const std::string& key) {
  auto it = e.fields.find(key);
  return it == e.fields.end() ? "" : it->second;
}

bool isClassLike(const model::Entity& e) { return e.kind == model::Kind::Class || e.kind == model::Kind::Enum; }

bool drawable(const model::Entity& e) { return isClassLike(e) || e.kind == model::Kind::Module; }

/// `Repository<1>` → `Repository<T>`, from the params field.
std::string displayName(const model::Entity& e) {
  const std::string params = fieldOf(e, "params");
  return params.empty() ? e.name : e.name + "<" + params + ">";
}

std::string memberLine(const model::Entity& m) {
  std::string line = fieldOf(m, "visibility");
  if (!line.empty()) line += " ";
  for (const auto& f : m.flags) {
    if (f == "abstract" || f == "static" || f == "const") line += f + " ";
  }
  if (m.kind == model::Kind::Attribute) {
    line += m.name + ": " + fieldOf(m, "type");
    const std::string def = fieldOf(m, "default");
    if (!def.empty()) line += " = " + def;
    return line;
  }
  line += m.name + "(" + fieldOf(m, "params") + ")";
  const std::string returns = fieldOf(m, "returns");
  if (!returns.empty()) line += " -> " + returns;
  return line;
}

std::string edgeKind(const std::string& relation) {
  if (relation == "inherits") return "inheritance";
  if (relation == "owns") return "composition";
  if (relation == "refs" || relation == "shares") return "aggregation";
  if (relation == "uses" || relation == "bounded-by") return "dependency";
  if (relation == "realises") return "realization";
  return "";
}

}  // namespace

std::vector<std::string> IrExport::diagrams() const {
  std::vector<std::string> out;
  for (const auto& pair : model_.entities()) {
    if (pair.second.kind == model::Kind::Diagram) out.push_back(pair.first);
  }
  return out;
}

bool IrExport::matches(const std::string& pattern, const model::Entity& entity) const {
  if (pattern == entity.id) return true;
  if (pattern.size() > 3 && pattern.compare(pattern.size() - 3, 3, "::*") == 0) {
    const std::string ns = pattern.substr(0, pattern.size() - 3);
    return entity.scope == ns || entity.scope.rfind(ns + "::", 0) == 0;
  }
  // a template written without arity: Repository matches Repository<1>
  const auto lt = entity.id.find('<');
  return lt != std::string::npos && entity.id.substr(0, lt) == pattern;
}

std::set<std::string> IrExport::select(const model::Entity& diagram) const {
  std::set<std::string> selected;
  for (const auto& pattern : split(fieldOf(diagram, "include"), ',')) {
    for (const auto& pair : model_.entities()) {
      if (drawable(pair.second) && !pair.second.expected && matches(pattern, pair.second)) selected.insert(pair.first);
    }
  }
  for (const auto& pattern : split(fieldOf(diagram, "exclude"), ',')) {
    for (auto it = selected.begin(); it != selected.end();) {
      it = matches(pattern, *model_.find(*it)) ? selected.erase(it) : std::next(it);
    }
  }
  return selected;
}

std::string IrExport::render(const std::string& diagramId, bool pretty) const {
  const model::Entity* diagram = model_.find(diagramId);
  if (!diagram || diagram->kind != model::Kind::Diagram) return "";
  // `show` adds optional detail (ownership, multiplicity, operators, notes, derived-stereotypes);
  // `hide` removes compartments (fields, methods). Nothing written = members only.
  const std::vector<std::string> showList = split(fieldOf(*diagram, "show"), ',');
  const std::set<std::string> show(showList.begin(), showList.end());
  const std::vector<std::string> hideList = split(fieldOf(*diagram, "hide"), ',');
  const std::set<std::string> hide(hideList.begin(), hideList.end());
  auto shows = [&](const char* what) { return show.count(what) > 0; };
  auto hidden = [&](const char* what) { return hide.count(what) > 0; };

  std::set<std::string> selected = select(*diagram);
  // modules of the selected classes become containers (R2)
  std::set<std::string> containers;
  for (const auto& id : selected) {
    const model::Entity* e = model_.find(id);
    if (e->kind == model::Kind::Module) containers.insert(id);
    for (std::string s = e->scope; !s.empty();) {
      const model::Entity* m = model_.find(s);
      if (m && m->kind == model::Kind::Module) containers.insert(s);
      const auto cut = s.rfind("::");
      s = cut == std::string::npos ? "" : s.substr(0, cut);
    }
  }

  nlohmann::json out;
  out["ir"] = 1;
  out["title"] = diagram->name;
  out["nodes"] = nlohmann::json::array();
  out["edges"] = nlohmann::json::array();
  std::set<std::string> placed;

  auto parentOf = [&](const model::Entity& e) -> std::string {
    for (std::string s = e.scope; !s.empty();) {
      if (containers.count(s)) return s;
      const auto cut = s.rfind("::");
      s = cut == std::string::npos ? "" : s.substr(0, cut);
    }
    return "";
  };

  for (const auto& id : containers) {
    const model::Entity& m = *model_.find(id);
    nlohmann::json node = {{"id", id}, {"kind", "container"}, {"label", id}, {"classifier", "namespace"}};
    const std::string parent = parentOf(m);
    if (!parent.empty()) node["parent"] = parent;
    out["nodes"].push_back(node);
    placed.insert(id);
  }

  for (const auto& id : selected) {
    const model::Entity& e = *model_.find(id);
    if (!isClassLike(e)) continue;
    nlohmann::json node = {{"id", id}, {"kind", "box"}, {"label", displayName(e)}};
    const std::string parent = parentOf(e);
    if (!parent.empty()) node["parent"] = parent;
    const std::string stereotypes = fieldOf(e, "stereotypes");
    if (e.kind == model::Kind::Enum) {
      node["classifier"] = "enum";
      node["stereotype"] = "enum";
      node["sections"] = nlohmann::json::array({split(fieldOf(e, "values"), ',')});
      node["sectionKinds"] = nlohmann::json::array({"attributes"});
    } else {
      const bool interface = stereotypes.find("interface") != std::string::npos;
      node["classifier"] = interface ? "interface" : e.has("dataclass") ? "dataclass" : "class";
      node["abstract"] = interface || stereotypes.find("abstract") != std::string::npos;
      // the badge: derived stereotypes, then the operator symbols the families do not already say
      std::vector<std::string> badges;
      if (shows("derived-stereotypes")) badges = split(stereotypes, ',');
      if (shows("operators")) {
        for (const auto& op : split(fieldOf(e, "operators"), ',')) {
          if (op != "==" && op != "!=" && op != "ordered" && op != "hash" && op != "print") badges.push_back(op);
        }
      }
      if (!badges.empty()) {
        std::string st;
        for (const auto& b : badges) st += (st.empty() ? "" : ", ") + b;
        node["stereotype"] = st;
      }
      std::vector<const model::Entity*> members;
      for (const auto& pair : model_.entities()) {
        if (pair.second.scope == id) members.push_back(&pair.second);
      }
      std::sort(members.begin(), members.end(), [](const model::Entity* a, const model::Entity* b) {
        return a->location.line < b->location.line;  // source order, not identity order
      });
      std::vector<std::string> attributes, methods;
      for (const model::Entity* m : members) {
        if (m->kind == model::Kind::Attribute) attributes.push_back(memberLine(*m));
        if (m->kind == model::Kind::Method) methods.push_back(memberLine(*m));
        if (m->kind == model::Kind::Operator && shows("operators") && !fieldOf(*m, "params").empty()) methods.push_back(memberLine(*m));
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
    const std::string note = fieldOf(e, "note");
    out["nodes"].push_back(node);
    placed.insert(id);
    if (shows("notes") && !note.empty()) {
      out["nodes"].push_back({{"id", id + "::note"}, {"kind", "note"}, {"label", note}});
      out["edges"].push_back({{"from", id + "::note"}, {"to", id}, {"kind", "attachment"}});
    }
  }

  // relations: lifted from members to their class; one edge per (from, to, kind, label)
  std::set<std::string> drawn;
  for (const auto& r : model_.relations()) {
    std::string kind = edgeKind(r.kind);
    if (kind.empty()) continue;
    const model::Entity* from = model_.find(r.from);
    const model::Entity* to = model_.find(r.to);
    if (!from || !to) continue;
    if (to->kind == model::Kind::Enum && kind == "composition") kind = "association";  // a value has no lifetime to own
    const std::string fromClass = isClassLike(*from) ? from->id : from->scope;
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
    out["edges"].push_back(edge);
  }
  return pretty ? out.dump(2) : out.dump();
}

}  // namespace render
}  // namespace driftspec
