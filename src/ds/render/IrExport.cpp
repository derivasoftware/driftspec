#include "ds/render/IrExport.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <sstream>

namespace ds {
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

/// What a class diagram draws as a box: a class, an enum, or a function of a module (one box each, «function»).
bool isBox(const model::Entity& e) {
  return e.kind == model::Kind::Class || e.kind == model::Kind::Enum || e.kind == model::Kind::Function || e.kind == model::Kind::Interface;
}

bool drawable(const model::Entity& e) { return isBox(e) || e.kind == model::Kind::Module; }

/// `Repository<1>` → `Repository<T>`, from the params field.
std::string displayName(const model::Entity& e) {
  const std::string params = e.kind == model::Kind::Class ? fieldOf(e, "params") : "";  // a function's params are its signature
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
  if (entity.has("extern")) return false;  // what lives outside the workspace is drawn only when named
  // a prefix with *: ds@* is the whole package, ds@ds::* a module and everything inside (full ids, R4)
  if (!pattern.empty() && pattern.back() == '*') {
    const std::string prefix = pattern.substr(0, pattern.size() - 1);
    return entity.id.rfind(prefix, 0) == 0 || entity.scope.rfind(prefix, 0) == 0;
  }
  // a module named as such: the module and everything inside
  if (const model::Entity* m = model_.find(pattern)) {
    if (m->kind == model::Kind::Module) return entity.scope == pattern || entity.scope.rfind(pattern + "::", 0) == 0;
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
  if (fieldOf(*diagram, "kind") == "sequence") return renderSequence(*diagram, pretty);
  if (fieldOf(*diagram, "kind") == "requirement") return renderRequirements(*diagram, pretty);
  if (fieldOf(*diagram, "kind") == "state") return renderState(*diagram, pretty);
  // `show` adds optional detail (ownership, multiplicity, operators, notes, derived-stereotypes);
  // `hide` removes compartments (fields, methods). Nothing written = members only.
  const std::vector<std::string> showList = split(fieldOf(*diagram, "show"), ',');
  const std::set<std::string> show(showList.begin(), showList.end());
  const std::vector<std::string> hideList = split(fieldOf(*diagram, "hide"), ',');
  const std::set<std::string> hide(hideList.begin(), hideList.end());
  auto shows = [&](const char* what) { return show.count(what) > 0; };
  auto hidden = [&](const char* what) { return hide.count(what) > 0; };

  std::set<std::string> selected = select(*diagram);
  // neighbours: n widens the selection to what the selected entities relate to, n steps away
  const int neighbours = fieldOf(*diagram, "neighbours").empty() ? 0 : std::stoi(fieldOf(*diagram, "neighbours"));
  for (int step = 0; step < neighbours; ++step) {
    std::set<std::string> grown = selected;
    for (const auto& r : model_.relations()) {
      if (edgeKind(r.kind).empty()) continue;
      const model::Entity* from = model_.find(r.from);
      const model::Entity* to = model_.find(r.to);
      if (!from || !to) continue;
      const std::string fromClass = isBox(*from) ? from->id : from->scope;
      const model::Entity* fromEntity = model_.find(fromClass);
      if (!fromEntity || !isBox(*fromEntity) || !isBox(*to) || to->expected || fromEntity->expected) continue;
      if (selected.count(fromClass) && !to->has("extern")) grown.insert(r.to);
      if (selected.count(r.to) && !fromEntity->has("extern")) grown.insert(fromClass);
    }
    selected = grown;
  }
  // modules of the selected classes become containers (R2), and so do nested packages (the HLD
  // boxes); the outermost package is the drawing itself. A container with nothing drawn in it is not drawn.
  std::set<std::string> containers;
  auto up = [](const std::string& s) {
    const auto cut = s.rfind("::");
    if (cut != std::string::npos) return s.substr(0, cut);  // ds@ds::b → ds@ds
    std::string t = s;
    if (!t.empty() && t.back() == '@') t.pop_back();
    const auto at = t.rfind('@');
    return at == std::string::npos ? std::string() : t.substr(0, at);  // app@core@model → app@core → app → ""
  };
  for (const auto& id : selected) {
    const model::Entity* e = model_.find(id);
    if (!isBox(*e)) continue;
    for (std::string s = e->scope; !s.empty(); s = up(s)) {
      const model::Entity* m = model_.find(s);
      if (m && (m->kind == model::Kind::Module || (m->kind == model::Kind::Package && !m->scope.empty()))) containers.insert(s);
    }
  }

  nlohmann::json out;
  out["ir"] = 1;
  out["title"] = diagram->name;
  out["nodes"] = nlohmann::json::array();
  out["edges"] = nlohmann::json::array();
  std::set<std::string> placed;

  auto parentOf = [&](const model::Entity& e) -> std::string {
    std::string s = e.scope;
    if (!s.empty() && s.back() == '@') s.pop_back();  // a package scope (app@core@) names the package app@core
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
    const std::string stereotypes = fieldOf(e, "stereotypes");
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
      node["sections"] = nlohmann::json::array({split(fieldOf(e, "values"), ',')});
      node["sectionKinds"] = nlohmann::json::array({"attributes"});
    } else {
      const bool interface = e.kind == model::Kind::Interface || stereotypes.find("interface") != std::string::npos;
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
        if (m->kind == model::Kind::Method || m->kind == model::Kind::Operation) methods.push_back(memberLine(*m));
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
    const std::string change = fieldOf(e, "change");
    if (!change.empty()) {
      node["refs"] = {{"change", change}};
      const std::string detail = fieldOf(e, "change-detail");
      const std::string from = fieldOf(e, "change-from");
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
      for (const auto& r : model_.relations()) {
        if (r.from != id || (r.kind != "requires" && r.kind != "constrained-by")) continue;
        const model::Entity* a = model_.find(r.to);
        if (!a || a->expected) continue;
        lines.push_back(a->id + "  " + fieldOf(*a, "title"));
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
  for (const auto& r : model_.relations()) {
    // a diff marks a relation as "<kind>+added" / "<kind>+removed"
    const auto plus = r.kind.find('+');
    const std::string change = plus == std::string::npos ? "" : r.kind.substr(plus + 1);
    const std::string baseKind = plus == std::string::npos ? r.kind : r.kind.substr(0, plus);
    std::string kind = edgeKind(baseKind);
    if (kind.empty()) continue;
    const model::Entity* from = model_.find(r.from);
    const model::Entity* to = model_.find(r.to);
    if (!from || !to) continue;
    if (to->kind == model::Kind::Enum && kind == "composition") kind = "association";  // a value has no lifetime to own
    const std::string fromClass = isBox(*from) ? from->id : from->scope;
    if (!placed.count(fromClass) || !placed.count(r.to) || fromClass == r.to) continue;
    std::string label;
    if (baseKind == "owns" || baseKind == "refs" || baseKind == "shares") {
      if (shows("ownership")) label = from->name;
      if (shows("multiplicity") && !r.multiplicity.empty()) label += (label.empty() ? "" : " ") + r.multiplicity;
    } else if (baseKind == "bounded-by") {
      label = "T: " + model_.find(r.to)->name;
    }
    const std::string key = fromClass + "|" + r.to + "|" + kind + "|" + label + "|" + change;
    if (!drawn.insert(key).second) continue;
    nlohmann::json edge = {{"from", fromClass}, {"to", r.to}, {"kind", kind}};
    if (!label.empty()) edge["label"] = label;
    if (!change.empty()) edge["refs"] = {{"change", change}};
    out["edges"].push_back(edge);
  }
  return pretty ? out.dump(2) : out.dump();
}

// ---- sequences: lifelines are the static context, children expand by depth (B3, B5, B9) ----

namespace {

struct SequenceBuilder {
  const model::Model& model;
  nlohmann::json nodes = nlohmann::json::array();
  nlohmann::json edges = nlohmann::json::array();
  std::map<std::string, std::string> lifelineOf;  // key → lifeline id
  int order = 0;

  std::string lifeline(const std::string& key, const std::string& label, const std::string& classifier, const std::string& typeId = "") {
    auto it = lifelineOf.find(key);
    if (it != lifelineOf.end()) return it->second;
    nlohmann::json node = {{"id", key}, {"kind", "lifeline"}, {"label", label}, {"classifier", classifier}};
    if (!typeId.empty()) {
      node["href"] = "#" + typeId;  // a page that also shows the class diagram lands on the class
      node["title"] = typeId;
      const model::Entity* type = model.find(typeId);
      if (type && !type->location.file.empty())
        node["refs"] = {{"location", type->location.file + ":" + std::to_string(type->location.line)}};
    }
    nodes.push_back(node);
    lifelineOf[key] = key;
    return key;
  }

  std::vector<const model::Entity*> messagesOf(const std::string& sequenceId) const {
    std::vector<const model::Entity*> out;
    for (const auto& pair : model.entities()) {
      if (pair.second.kind == model::Kind::Message && pair.second.scope == sequenceId) out.push_back(&pair.second);
    }
    std::sort(out.begin(), out.end(), [](const model::Entity* a, const model::Entity* b) {
      return std::stoi(fieldOf(*a, "order")) < std::stoi(fieldOf(*b, "order"));
    });
    return out;
  }

  std::string shortName(const std::string& id) {
    const auto cut = id.rfind("::");
    return cut == std::string::npos ? id : id.substr(cut + 2);
  }

  /// Emits the messages of one behaviour; `selfLine` is the lifeline its subject runs on.
  void emit(const model::Entity& seq, const std::string& selfLine, int depth, std::set<std::string>& stack) {
    if (stack.count(seq.id)) return;  // recursion: drawn once
    stack.insert(seq.id);
    std::map<std::string, int> firstOrder, lastOrder, elseOrder;  // by frame id
    for (const model::Entity* m : messagesOf(seq.id)) {
      if (fieldOf(*m, "return") == "true") continue;  // told by the arrow back to whoever called
      const std::string to = fieldOf(*m, "to");
      const std::string typeId = fieldOf(*m, "type");
      std::string target = selfLine;
      if (to == "self" && !typeId.empty() && typeId != selfLine) {
        // a function of another module (or of the class's module, called from a method) runs on that module's lifeline
        target =
            lifeline(typeId, shortName(typeId.substr(typeId.find_last_of('@') == std::string::npos ? 0 : typeId.find_last_of('@') + 1)),
                     "participant", typeId);
      } else if (to != "self") {
        // one lifeline per entity of the static context: the same attribute reached from two
        // behaviours of one class is one participant; an unresolved name stays local to its behaviour
        const std::string typeName = typeId.empty() ? "?" : shortName(typeId);
        target = lifeline(typeId.empty() ? seq.id + "::" + to : typeId, to + ": " + typeName, "participant", typeId);
      }
      const std::string frame = fieldOf(*m, "frame");
      for (std::string f = frame; !f.empty(); f = fieldOf(*model.find(f), "parent")) {
        if (!firstOrder.count(f)) firstOrder[f] = order;
        const model::Entity* fe = model.find(f);
        if (fieldOf(*fe, "else") == m->id) elseOrder[f] = order;
      }
      const int row = order++;
      nlohmann::json edge = {{"from", selfLine},
                             {"to", target},
                             {"kind", "message"},
                             {"order", row},
                             {"label", fieldOf(*m, "op") + "(" + fieldOf(*m, "args") + ")" +
                                           (fieldOf(*m, "scenario").empty() ? "" : " / " + fieldOf(*m, "scenario"))}};
      nlohmann::json refs = {{"location", m->location.file + ":" + std::to_string(m->location.line)}};
      const std::string expands = fieldOf(*m, "expands");
      std::string title;
      if (!typeId.empty()) title = typeId + "::" + fieldOf(*m, "op");
      const std::string change = fieldOf(*m, "change");  // a diff marks messages too
      if (!change.empty()) {
        refs["change"] = change;
        const std::string detail = fieldOf(*m, "change-detail");
        title = change + (detail.empty() ? "" : ": " + detail) + (title.empty() ? "" : "\n" + title);
      }
      if (!expands.empty()) {
        refs["expands"] = expands;
        title += (title.empty() ? "" : "\n") + std::string(depth > 0 ? "expanded below: " : "expands to: ") + model.find(expands)->name;
        if (depth <= 0) edge["href"] = "#" + expands;  // a page that also shows the child lands on it
      }
      if (!title.empty()) edge["title"] = title;
      const size_t at = edges.size();
      edges.push_back(edge);
      if (!expands.empty() && depth > 0) {
        const int first = order;
        emit(*model.find(expands), target, depth - 1, stack);
        if (order > first) refs["span"] = std::to_string(first) + "-" + std::to_string(order - 1);  // the rows the expansion occupies
      }
      edges[at]["refs"] = refs;
      const std::string result = fieldOf(*m, "result");
      if (!result.empty())
        edges.push_back({{"from", target}, {"to", selfLine}, {"kind", "message"}, {"order", order++}, {"label", result}, {"dashed", true}});
      for (std::string f = frame; !f.empty(); f = fieldOf(*model.find(f), "parent")) lastOrder[f] = order - 1;  // after any expansion
    }
    for (const auto& f : firstOrder) {
      const model::Entity* fe = model.find(f.first);
      nlohmann::json frame = {{"id", f.first},
                              {"kind", "frame"},
                              {"label", fieldOf(*fe, "kind") + " " + fieldOf(*fe, "condition")},
                              {"classifier", fieldOf(*fe, "kind")},
                              {"span", nlohmann::json::array({f.second, lastOrder[f.first]})}};
      if (elseOrder.count(f.first)) frame["dividers"] = nlohmann::json::array({{{"at", elseOrder[f.first]}, {"label", "else"}}});
      nodes.push_back(frame);
    }
    stack.erase(seq.id);
  }
};

}  // namespace

std::string IrExport::renderSequence(const model::Entity& diagram, bool pretty) const {
  const int depth = fieldOf(diagram, "depth").empty() ? 0 : std::stoi(fieldOf(diagram, "depth"));
  const std::string level = fieldOf(diagram, "level");
  // the behaviours selected: by subject id, by sequence id, or a namespace
  std::vector<const model::Entity*> selected;
  for (const auto& pattern : split(fieldOf(diagram, "include"), ',')) {
    for (const auto& pair : model_.entities()) {
      const model::Entity& e = pair.second;
      if (e.kind != model::Kind::Sequence || e.expected) continue;
      const std::string subjectId = e.id.substr(0, e.id.find("::sequence"));
      const bool prefix = !pattern.empty() && pattern.back() == '*' && e.id.rfind(pattern.substr(0, pattern.size() - 1), 0) == 0;
      // as it is written: `Class::op` the default, `Class::op / scenario` that one, `Class::op*` them all
      const bool isDefault = e.fields.find("scenario") == e.fields.end();
      const bool named = !isDefault && pattern == subjectId + " / " + e.fields.at("scenario");
      if (pattern == e.id || (pattern == subjectId && isDefault) || named || prefix) selected.push_back(&e);
    }
  }
  SequenceBuilder b{model_, nlohmann::json::array(), nlohmann::json::array(), {}, 0};
  nlohmann::json out;
  out["ir"] = 1;
  out["title"] = diagram.name;
  if (level == "module") {
    // lifting (B9): every message between classes of different modules is a message between the modules
    std::set<std::string> drawn;
    for (const model::Entity* seq : selected) {
      const std::string subjectId = seq->id.substr(0, seq->id.find("::sequence"));
      const model::Entity* subject = model_.find(subjectId);
      const bool runsInModule = subject && (subject->kind == model::Kind::Method || subject->kind == model::Kind::Function);
      const model::Entity* cls = runsInModule ? model_.find(subject->scope) : subject;
      if (!cls) continue;
      const std::string home = subject->kind == model::Kind::Function ? cls->id : cls->scope;  // a function runs in its module
      for (const model::Entity* m : b.messagesOf(seq->id)) {
        const std::string typeId = fieldOf(*m, "type");
        const model::Entity* type = typeId.empty() ? nullptr : model_.find(typeId);
        const std::string typeHome = type && type->kind == model::Kind::Module ? type->id : type ? type->scope : "";
        if (!type || typeHome == home) continue;
        auto moduleLabel = [](const std::string& id) {
          return id.substr(id.find_last_of('@') == std::string::npos ? 0 : id.find_last_of('@') + 1);
        };
        const std::string from = b.lifeline(home, moduleLabel(home), "namespace");
        const std::string to = b.lifeline(typeHome, moduleLabel(typeHome), "namespace");
        const std::string label = (type->kind == model::Kind::Module ? "" : b.shortName(typeId) + "::") + fieldOf(*m, "op");
        if (!drawn.insert(from + "|" + to + "|" + label).second) continue;
        b.edges.push_back({{"from", from}, {"to", to}, {"kind", "message"}, {"order", b.order++}, {"label", label}});
      }
    }
  } else {
    for (const model::Entity* seq : selected) {
      const std::string subjectId = seq->id.substr(0, seq->id.find("::sequence"));
      const model::Entity* subject = model_.find(subjectId);
      // a method runs on its class, a function on its module
      const bool function = subject && subject->kind == model::Kind::Function;
      const bool method = subject && (subject->kind == model::Kind::Method || subject->kind == model::Kind::Operation || function);
      const model::Entity* cls = method ? model_.find(subject->scope) : subject;
      // the caller, leftmost, invokes the entry operation on the subject: what is being looked at is the first thing drawn
      const std::string caller = method ? b.lifeline("caller", "caller", "actor") : "";
      const std::string selfLabel = cls ? (function ? cls->name : "self: " + cls->name) : "self: " + subjectId;
      const std::string self = b.lifeline(cls ? cls->id : subjectId, selfLabel, "participant", cls ? cls->id : "");
      if (method) {
        b.edges.push_back({{"from", caller},
                           {"to", self},
                           {"kind", "message"},
                           {"order", b.order++},
                           {"label", subject->name + "(" + fieldOf(*subject, "params") + ")"},
                           {"title", subject->id},
                           {"refs", {{"location", subject->location.file + ":" + std::to_string(subject->location.line)}}}});
      }
      std::set<std::string> stack;
      b.emit(*seq, self, depth, stack);
      const std::string returns = method ? fieldOf(*subject, "returns") : "";
      if (method && !returns.empty() && returns != "none" && returns != "void") {
        std::string given = returns;  // what its `return` says, or the declared type
        for (const model::Entity* m : b.messagesOf(seq->id)) {
          if (fieldOf(*m, "return") == "true") given = fieldOf(*m, "result");
        }
        b.edges.push_back({{"from", self}, {"to", caller}, {"kind", "message"}, {"order", b.order++}, {"label", given}, {"dashed", true}});
      }
    }
  }
  if (b.edges.empty()) out["notice"] = "No behaviour selected: include names a subject (ds::Checker::run), a behaviour or a namespace.";
  out["nodes"] = b.nodes;
  out["edges"] = b.edges;
  return pretty ? out.dump(2) : out.dump();
}

// ---- state machines: the states and the jumps; of an interface, its navigation ----------

namespace {
std::vector<std::string> wrap(const std::string& text, size_t width);
}  // namespace

std::string IrExport::renderState(const model::Entity& diagram, bool pretty) const {
  nlohmann::json nodes = nlohmann::json::array();
  nlohmann::json edges = nlohmann::json::array();
  std::set<std::string> drawn;
  for (const auto& pattern : split(fieldOf(diagram, "include"), ',')) {
    const model::Entity* state = model_.find(pattern + "::state");
    if (!state) state = model_.find(pattern);
    if (!state || state->kind != model::Kind::State) continue;
    const auto box = [&](const std::string& name) {
      const std::string id = state->id + "::" + name;
      if (!drawn.insert(id).second) return id;
      nlohmann::json node = {{"id", id}, {"kind", "box"}, {"label", name}, {"classifier", "state"}};
      if (const model::Entity* view = model_.find(id)) {
        node["refs"] = {{"location", view->location.file + ":" + std::to_string(view->location.line)}};
        nlohmann::json sections = nlohmann::json::array();
        nlohmann::json kinds = nlohmann::json::array();
        const std::string text = fieldOf(*view, "text");
        if (!text.empty()) {
          sections.push_back(wrap(text, 36));
          kinds.push_back("attributes");
        }
        std::vector<std::string> binds;
        for (const auto& r : model_.relations()) {
          if (r.from == id && (r.kind == "requires" || r.kind == "constrained-by")) binds.push_back(r.to);
        }
        if (!binds.empty()) {
          sections.push_back(binds);
          kinds.push_back("methods");
        }
        if (!sections.empty()) {
          node["sections"] = sections;
          node["sectionKinds"] = kinds;
        }
      }
      nodes.push_back(node);
      return id;
    };
    // the machine reads from where it starts: first in the document, so the layout flows down from it
    const std::string initial = fieldOf(*state, "initial");
    if (!initial.empty()) {
      const std::string start = state->id + "::initial";
      nodes.push_back({{"id", start}, {"kind", "box"}, {"label", "start"}, {"classifier", "initial"}});
      edges.push_back({{"from", start}, {"to", box(initial)}, {"kind", "association"}});
    }
    std::stringstream first(fieldOf(*state, "transitions"));
    for (std::string move; std::getline(first, move);) {  // in the order written: the first jumps go down
      const auto cut = move.find('>');
      if (cut == std::string::npos) continue;
      box(move.substr(0, cut));
      const auto end = move.find('>', cut + 1);
      if (end != std::string::npos) box(move.substr(cut + 1, end - cut - 1));
    }
    for (const auto& pair : model_.entities()) {  // a view no transition names is still drawn: it is the finding
      if (pair.second.kind == model::Kind::View && pair.second.scope == state->id) box(pair.second.name);
    }
    std::stringstream moves(fieldOf(*state, "transitions"));
    for (std::string move; std::getline(moves, move);) {
      const auto first = move.find('>');
      const auto second = move.find('>', first + 1);
      if (first == std::string::npos || second == std::string::npos) continue;
      edges.push_back({{"from", box(move.substr(0, first))},
                       {"to", box(move.substr(first + 1, second - first - 1))},
                       {"kind", "association"},
                       {"label", move.substr(second + 1)}});
    }
  }
  nlohmann::json out;
  out["ir"] = 1;
  out["title"] = diagram.name;
  if (nodes.empty()) out["notice"] = "No state machine selected: include names its class or interface (shop@web)";
  out["nodes"] = nodes;
  out["edges"] = edges;
  return pretty ? out.dump(2) : out.dump();
}

// ---- requirements: the umbrella of an artefact, or the tree of a package ----------------

namespace {

std::vector<std::string> wrap(const std::string& text, size_t width) {
  std::vector<std::string> lines;
  std::string line;
  std::stringstream in(text);
  std::string word;
  while (in >> word) {
    if (!line.empty() && line.size() + 1 + word.size() > width) {
      lines.push_back(line);
      line.clear();
    }
    line += (line.empty() ? "" : " ") + word;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

bool isArtefact(const model::Entity& e) {
  return e.kind == model::Kind::Requirement || e.kind == model::Kind::Constraint || e.kind == model::Kind::Verification;
}

}  // namespace

std::string IrExport::renderRequirements(const model::Entity& diagram, bool pretty) const {
  const std::vector<std::string> showList = split(fieldOf(diagram, "show"), ',');
  const std::set<std::string> show(showList.begin(), showList.end());
  const bool withDesign = show.empty() || show.count("design");
  const bool withVerifications = show.empty() || show.count("verifications");
  // the roots: artefacts named, or every artefact of a package / module
  std::set<std::string> roots;
  for (const auto& pattern : split(fieldOf(diagram, "include"), ',')) {
    for (const auto& pair : model_.entities()) {
      const model::Entity& e = pair.second;
      if (!isArtefact(e) || e.expected) continue;
      bool hit = pattern == e.id;
      if (!pattern.empty() && pattern.back() == '*') {
        // iris@* every artefact of the package (their scope starts with iris@), iris@projection::* those of a module
        hit |= e.scope.rfind(pattern.substr(0, pattern.size() - 1), 0) == 0;
      } else if (const model::Entity* m = model_.find(pattern)) {
        hit |= m->kind == model::Kind::Module && (e.scope == pattern || e.scope.rfind(pattern + "::", 0) == 0);
      }
      if (hit) roots.insert(e.id);
    }
  }
  // the umbrella: what refines a root (down, transitively), what verifies it, what realises it
  std::set<std::string> selected = roots;
  bool grew = true;
  while (grew) {
    grew = false;
    for (const auto& r : model_.relations()) {
      if ((r.kind == "refines" && selected.count(r.to) && !selected.count(r.from)) ||
          (withVerifications && r.kind == "verifies" && selected.count(r.to) && !selected.count(r.from)) ||
          (withDesign && (r.kind == "requires" || r.kind == "constrained-by") && selected.count(r.to) && !selected.count(r.from))) {
        const model::Entity* from = model_.find(r.from);
        if (!from || from->expected) continue;
        if (from->kind != model::Kind::Class && !isArtefact(*from)) continue;
        selected.insert(r.from);
        grew = true;
      }
    }
  }
  nlohmann::json out;
  out["ir"] = 1;
  out["title"] = diagram.name;
  out["nodes"] = nlohmann::json::array();
  out["edges"] = nlohmann::json::array();
  for (const auto& id : selected) {
    const model::Entity& e = *model_.find(id);
    nlohmann::json node = {{"id", id}, {"kind", "box"}, {"label", isArtefact(e) ? e.id : e.name}, {"classifier", model::kindName(e.kind)}};
    if (isArtefact(e)) {
      std::vector<std::string> lines = wrap(fieldOf(e, "title"), 44);
      const std::string status = fieldOf(e, "status");
      const std::string method = fieldOf(e, "method");
      std::string stereotype = model::kindName(e.kind);
      if (!status.empty()) stereotype += ", " + status;
      if (!method.empty()) stereotype += ", " + method;
      if (e.kind == model::Kind::Requirement)
        stereotype += e.scope.empty() || model::isSystemScope(e.scope)
                          ? ", system"
                          : ", " + model::lastName(e.scope.back() == '@' ? e.scope.substr(0, e.scope.size() - 1) : e.scope);
      node["stereotype"] = stereotype;
      if (!lines.empty()) node["sections"] = nlohmann::json::array({lines});
      const std::string text = fieldOf(e, "text");
      if (!text.empty()) node["title"] = text;
      if (!e.location.file.empty()) node["refs"] = {{"location", e.location.file + ":" + std::to_string(e.location.line)}};
    } else {
      node["parent"] = nlohmann::json();  // no containers in this view
      node.erase("parent");
      node["stereotype"] = fieldOf(e, "stereotypes");
      if (node["stereotype"].get<std::string>().empty()) node.erase("stereotype");
    }
    const std::string change = fieldOf(e, "change");
    if (!change.empty()) node["refs"]["change"] = change;
    out["nodes"].push_back(node);
  }
  for (const auto& r : model_.relations()) {
    if (!selected.count(r.from) || !selected.count(r.to)) continue;
    if (r.kind == "refines") {
      out["edges"].push_back({{"from", r.from}, {"to", r.to}, {"kind", "realization"}, {"label", "refines"}});  // the parent above
    } else if (r.kind == "verifies") {
      out["edges"].push_back({{"from", r.to}, {"to", r.from}, {"kind", "dependency"}, {"label", "verified by"}});
    } else if (r.kind == "requires") {
      out["edges"].push_back({{"from", r.to}, {"to", r.from}, {"kind", "dependency"}, {"label", "realised by"}});
    } else if (r.kind == "constrained-by") {
      out["edges"].push_back({{"from", r.to}, {"to", r.from}, {"kind", "dependency"}, {"label", "constrains"}});
    }
  }
  if (out["nodes"].empty()) out["notice"] = "No artefact selected: include names a requirement, a package (package:x) or a module.";
  return pretty ? out.dump(2) : out.dump();
}

}  // namespace render
}  // namespace ds
