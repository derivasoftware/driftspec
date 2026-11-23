#include "ds/render/IrExport.hpp"

#include <algorithm>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>

#include "ds/machine/Machine.hpp"
#include "ds/machine/Table.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/SequenceTree.hpp"
#include "ds/query/Filter.hpp"
#include "ds/query/Walk.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace render {

using text::join;
using text::splitTrimmed;

namespace {

/// What a class diagram draws as a box: a class, an enum, or a function of a module (one box each, «function»).
bool isBox(const model::Entity& e) {
  return e.kind == model::Kind::Class || e.kind == model::Kind::Enum || e.kind == model::Kind::Function || e.kind == model::Kind::Interface;
}

/// `Repository<1>` → `Repository<T>`, from the params field.
std::string displayName(const model::Entity& e) {
  const std::string params = e.kind == model::Kind::Class ? e.field("params") : "";  // a function's params are its signature
  return params.empty() ? e.name : e.name + "<" + params + ">";
}

std::string memberLine(const model::Entity& m) {
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

/// What a box is drawn as, the word its badge is read from: the same for a box of its own and for a row of the
/// closed box that holds it (DS-244).
std::string classifierOf(const model::Entity& e) {
  if (e.kind == model::Kind::Package) return "package";
  if (e.kind == model::Kind::Module) return "namespace";
  if (e.kind == model::Kind::Function) return "function";
  if (e.kind == model::Kind::Enum) return "enum";
  if (e.kind == model::Kind::Interface) return "interface";
  return e.has("dataclass") ? "dataclass" : "class";
}

/// How deep a diagram follows the calls: `depth: n`, `*` for every level (the guard against recursion is what ends
/// it), none when it is not written or is no number (a finding).
int levelsOf(const model::Entity& diagram) {
  const std::string written = diagram.field("depth");
  if (written == "*") return 1000;
  const bool digits = !written.empty() && std::all_of(written.begin(), written.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
  return digits ? std::stoi(written) : 0;
}

std::string edgeKind(const std::string& relation) {
  if (relation == "inherits") return "inheritance";
  if (relation == "owns") return "composition";
  if (relation == "refs" || relation == "shares") return "aggregation";
  if (relation == "uses" || relation == "bounded-by" || relation == "consumes" || relation == "sends")
    return "dependency";  // @consumes is drawn (DS-219)
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

std::string IrExport::render(const std::string& diagramId, bool pretty) const {
  const model::Entity* diagram = model_.find(diagramId);
  if (!diagram || diagram->kind != model::Kind::Diagram) return "";
  if (diagram->field("kind") == "sequence") return renderSequence(*diagram, pretty);
  if (diagram->field("kind") == "tour") return renderTour(*diagram, pretty);
  if (diagram->field("kind") == "requirement") return renderRequirements(*diagram, pretty);
  if (diagram->field("kind") == "state") return renderState(*diagram, pretty);
  if (diagram->field("kind") == "table") return renderTable(*diagram, pretty);
  return renderBoxes(*diagram, pretty);
}

std::string IrExport::renderTour(const model::Entity& tour, bool pretty) const {
  // the sequence, as a sequence diagram of the same fields draws it
  const nlohmann::json sequence = nlohmann::json::parse(renderSequence(tour, false));
  // who takes part, drawn as boxes on the same cut: each lifeline is one box, with the relations between them
  const std::set<std::string> who = query::SequenceFilter(splitTrimmed(tour.field("include"), ','), levelsOf(tour), tour.id).select(model_);
  model::Entity boxes = tour;
  boxes.fields["kind"] = "class";
  std::vector<std::string> taken, closed;
  for (const auto& id : who) {
    taken.push_back(id);
    const model::Entity* e = model_.find(id);
    if (!e || (e->kind != model::Kind::Package && e->kind != model::Kind::Module)) continue;
    // a box takes part as one box, with everything it holds: what it holds is not drawn, and its relations are
    // lifted to it. That is so whether the cut closes it or a scenario written between boxes names it
    closed.push_back(id);
    if (e->kind == model::Kind::Package) taken.push_back(id + "@*");
  }
  boxes.fields["include"] = join(taken, ", ");
  boxes.fields.erase("open");
  boxes.fields.erase("cut");
  boxes.fields.erase("close");
  if (!closed.empty()) boxes.fields["close"] = join(closed, ", ");
  if (!tour.fields.count("hide") && !tour.fields.count("show")) boxes.fields["hide"] = "fields, methods";
  const nlohmann::json drawn = nlohmann::json::parse(renderBoxes(boxes, false));
  // one document: its parts named in the order they are drawn, each node and edge saying which one it is of
  nlohmann::json out = {{"ir", 1}, {"title", tour.name}, {"parts", {"boxes", "sequence"}}};
  out["nodes"] = nlohmann::json::array();
  out["edges"] = nlohmann::json::array();
  for (const auto& part : {std::make_pair("boxes", &drawn), std::make_pair("sequence", &sequence)}) {
    for (const char* list : {"nodes", "edges"}) {
      for (nlohmann::json item : (*part.second)[list]) {
        item["part"] = part.first;
        out[list].push_back(item);
      }
    }
  }
  if (sequence.contains("notice")) out["notice"] = sequence["notice"];
  return pretty ? out.dump(2) : out.dump();
}

std::string IrExport::renderBoxes(const model::Entity& entity, bool pretty) const {
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
  const auto filter = query::filterFor(*diagram);
  const std::set<std::string> selected = static_cast<const query::ClassFilter&>(*filter).boxes(model_);
  const std::set<std::string> containers = query::ClassFilter::containers(model_, selected);
  auto up = [](const std::string& s) { return model::Scope(model::Scope(s).outer().text()).box().text(); };

  nlohmann::json out;
  out["ir"] = 1;
  out["title"] = diagram->name;
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
    const std::string fromClass = query::drawnFrom(model_, *from);                     // a behaviour's relation is its subject's (DS-219)
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
    edge["refs"]["bond"] = axis::bondOf(r.kind);  // what it reads as between boxes, once lifted (DS-230)
    out["edges"].push_back(edge);
  }
  // what a box does counts as much as what it declares, with or without open: (DS-219): its behaviours cross
  // boxes too. Types alone would leave an interface unconnected to what handles it
  for (const auto& r : model_.index().ofKinds({"couples-transient", "couples-structural"})) {
    const model::Entity* seq = model_.find(r.from);
    if (!seq || !placed.count(r.to)) continue;
    const std::string owner = query::drawnFrom(model_, *seq);
    if (!placed.count(owner) || owner == r.to) continue;
    // an edge between the two already says it (an attribute's ownership, a type used): no second arrow
    const std::string pair = owner + "|" + r.to + "|";
    const auto at = drawn.lower_bound(pair);
    if (at != drawn.end() && at->compare(0, pair.size(), pair) == 0) continue;
    drawn.insert(pair + "dependency||");
    out["edges"].push_back({{"from", owner}, {"to", r.to}, {"kind", "dependency"}, {"refs", {{"bond", axis::bondOf(r.kind)}}}});
  }
  // a cut written (`open:`, `close:`, `cut:`) says which boxes close: the drawing is lifted onto it (DS-229)
  const axis::Cut cut(model_, *diagram);
  if (cut.written()) return closeBoxes(out.dump(), cut, pretty);
  return pretty ? out.dump(2) : out.dump();
}

std::string IrExport::closeBoxes(const std::string& ir, const axis::Cut& cut, bool pretty) const {
  auto outerOf = [](const std::string& id) { return model::Scope(model::Scope(id).outer().text()).box().text(); };  // the box outside
  nlohmann::json doc = nlohmann::json::parse(ir);
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
  return pretty ? doc.dump(2) : doc.dump();
}

// ---- sequences: lifelines are the static context, children expand by depth (B3, B5, B9) ----

namespace {

/// The scenario of a behaviour drawn as written: every branch, with its frame.
const char* const kWhole = query::kEveryBranch;

struct SequenceBuilder {
  const model::Model& model;
  nlohmann::json nodes = nlohmann::json::array();
  nlohmann::json edges = nlohmann::json::array();
  std::map<std::string, std::string> lifelineOf;  // key → lifeline id
  int order = 0;
  const axis::Cut* cut = nullptr;  // the cut the lifelines stand on, and the calls it follows (DS-231)
  std::set<std::string> boxLines;  // the lifelines that are closed boxes

  /// The closed box that stands for a type on the cut; empty when the type draws a lifeline of its own.
  std::string closedAround(const std::string& typeId) const {
    if (!cut || typeId.empty()) return "";
    const std::string box = cut->standsFor(typeId);
    return cut->closed(box) ? box : "";  // a module that runs functions is a closed box itself
  }

  /// The lifeline of a closed box: one for everything inside it.
  std::string boxLine(const std::string& box) {
    boxLines.insert(box);
    const model::Entity* e = model.find(box);
    return lifeline(box, box.substr(box.find_last_of('@') == std::string::npos ? 0 : box.find_last_of('@') + 1),
                    e && e->kind == model::Kind::Package ? "package" : "namespace", box);
  }

  /// The lifeline of a participant (DS-232): the closed box that stands for it, the box itself when a box is
  /// named as a participant, or one lifeline for each object of the context.
  std::string lineOf(const std::string& typeId, const std::string& name, const std::string& behaviour) {
    const std::string box = closedAround(typeId);
    if (!box.empty()) return boxLine(box);
    const model::Entity* type = typeId.empty() ? nullptr : model.find(typeId);
    if (type && (type->kind == model::Kind::Package || type->kind == model::Kind::Module)) {
      return lifeline(typeId, type->name, type->kind == model::Kind::Package ? "package" : "namespace", typeId);
    }
    const std::string typeName = typeId.empty() ? "?" : model::Identity(typeId).lastName();  // an interface has no ::
    // two objects of one type are two lifelines: keyed by the name in the context, not by the type alone; a
    // participant named as what it is (an interface, a class) says it once
    return lifeline(typeId.empty() ? behaviour + "::" + name : typeId + "|" + name, name == typeName ? name : name + ": " + typeName,
                    "participant", typeId);
  }

  /// Whether the call that enters `behaviour` is followed into it.
  bool followed(const std::string& behaviour, int depth) const {
    return cut ? cut->follows(model::Identity(behaviour).subject().text(), depth) : depth > 0;
  }

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

  std::string shortName(const std::string& id) {
    const auto at = id.rfind("::");
    return at == std::string::npos ? id : id.substr(at + 2);
  }

  /// Emits the messages of one behaviour; `selfLine` is the lifeline its subject runs on.
  /// `backLine` is whoever called it, where an early return goes. `path` is the scenario it follows: its name,
  /// "" for the default path that takes no named branch, or kWhole for every branch, as written.
  void emit(const model::Entity& seq, const std::string& selfLine, const std::string& backLine, const std::string& path, int depth,
            std::set<std::string>& stack) {
    const std::string key = model::Identity(seq.id).withScenario(path).text();
    if (stack.count(key)) return;  // recursion: drawn once
    stack.insert(key);
    std::map<std::string, int> firstOrder, lastOrder, elseOrder;  // by frame id
    // a path draws only the messages it takes, and only the frames it cannot tell; every branch otherwise
    std::set<std::string> kept;
    const bool whole = path == kWhole;
    // what it takes and where each call goes on: the one walk of a behaviour (DS-130)
    const std::vector<query::Taken> steps = query::Walker(model).steps(seq.id, path, whole ? nullptr : &kept);
    const model::SequenceTree tree(model, seq.id);  // the frame around each step, where an alt's else begins (DS-172)
    auto drawn = [&](const std::string& frame) { return whole || kept.count(frame); };
    // a frame that holds no message (only prose, or a branch that ends with nothing) still has a row of its own
    std::vector<const model::Entity*> empty;
    if (whole) {
      for (const auto& f : tree.frames())
        if (tree.first(f).empty()) empty.push_back(model.find(f));
      std::stable_sort(empty.begin(), empty.end(),
                       [&tree](const model::Entity* a, const model::Entity* b) { return tree.at(a->id) < tree.at(b->id); });
    }
    size_t nextEmpty = 0;
    auto flushEmpty = [&](int before) {
      for (; nextEmpty < empty.size() && tree.at(empty[nextEmpty]->id) <= before; ++nextEmpty) {
        const model::Entity& f = *empty[nextEmpty];
        edges.push_back({{"from", selfLine}, {"to", selfLine}, {"kind", "message"}, {"order", order}, {"label", "..."}, {"dashed", true}});
        for (std::string id = f.id; !id.empty(); id = tree.frameOf(id)) {
          if (!firstOrder.count(id)) firstOrder[id] = order;
          lastOrder[id] = order;
        }
        ++order;
      }
    };
    for (const query::Taken& taken : steps) {
      const model::Entity* m = model.find(taken.message);
      flushEmpty(tree.orderOf(m->id));
      if (m->field("return") == "true") {
        // a return inside a frame ends that branch early: it is drawn, so the frame is never empty
        std::string frame = tree.frameOf(m->id);
        while (!frame.empty() && !drawn(frame)) frame = tree.frameOf(frame);
        const std::string fails = m->field("fails");                         // a path that ends failing is drawn wherever it ends (DS-187)
        if (backLine.empty() || (frame.empty() && fails.empty())) continue;  // the last return: the arrow back tells it
        if (selfLine == backLine && boxLines.count(selfLine)) continue;      // inside a closed box: nothing crosses
        for (std::string f = frame; !f.empty(); f = tree.frameOf(f)) {
          if (!drawn(f)) continue;
          if (!firstOrder.count(f)) firstOrder[f] = order;
          if (tree.elseOf(f) == m->id) elseOrder[f] = order;
        }
        const std::string value = m->field("result");
        edges.push_back({{"from", selfLine},
                         {"to", backLine},
                         {"kind", "message"},
                         {"order", order++},
                         {"label", !fails.empty()  ? "fail " + fails
                                   : value.empty() ? "return"
                                                   : "return " + value},
                         {"dashed", true}});
        for (std::string f = frame; !f.empty(); f = tree.frameOf(f)) {
          if (drawn(f)) lastOrder[f] = order - 1;
        }
        continue;
      }
      const std::string to = m->field("to");
      const std::string typeId = m->field("type");
      // who sends is the sender written, a participant like any other; the subject when none is (DS-232)
      const std::string sender = m->field("from");
      const std::string from =
          sender.empty() || sender == "self" || m->field("from-type").empty() ? selfLine : lineOf(m->field("from-type"), sender, seq.id);
      std::string target = selfLine;
      const std::string box = closedAround(typeId);
      if (to == "self" && box.empty() && !typeId.empty() && typeId != selfLine) {
        // a function of another module (or of the class's module, called from a method) runs on that module's lifeline
        target =
            lifeline(typeId, shortName(typeId.substr(typeId.find_last_of('@') == std::string::npos ? 0 : typeId.find_last_of('@') + 1)),
                     "participant", typeId);
      } else if (to != "self" || !box.empty()) {
        target = lineOf(typeId, to, seq.id);  // what a closed box holds, the box stands for
      }
      const std::string frame = tree.frameOf(m->id);
      const std::string& entered = taken.next;
      const bool goesOn = !entered.empty() && followed(entered, depth);
      if (target == from && boxLines.count(from)) {
        // both ends have the same representative: the message is internal and is not drawn. What it calls may
        // still cross, so it is followed like any other, and a frame is drawn around what crosses inside it
        const int before = order;
        if (goesOn) emit(*model.find(entered), target, from, taken.nextPath, depth - 1, stack);
        if (order > before) {
          for (std::string f = frame; !f.empty(); f = tree.frameOf(f)) {
            if (!drawn(f)) continue;
            if (!firstOrder.count(f)) firstOrder[f] = before;
            if (tree.elseOf(f) == m->id) elseOrder[f] = before;
            lastOrder[f] = order - 1;
          }
        }
        continue;
      }
      for (std::string f = frame; !f.empty(); f = tree.frameOf(f)) {
        if (!drawn(f)) continue;  // a branch the path takes is drawn inline, without its frame
        if (!firstOrder.count(f)) firstOrder[f] = order;
        if (tree.elseOf(f) == m->id) elseOrder[f] = order;
      }
      const int row = order++;
      nlohmann::json edge = {
          {"from", from},
          {"to", target},
          {"kind", "message"},
          {"order", row},
          // a message to a closed box says which of its children takes it
          {"label", (box.empty() || box == typeId ? "" : model::Identity(typeId).lastName() + "::") + m->field("op") + "(" +
                        m->field("args") + ")" + (m->field("scenario").empty() ? "" : " / " + m->field("scenario"))}};
      nlohmann::json refs = {{"location", m->location.file + ":" + std::to_string(m->location.line)}};
      const std::string& expands = taken.next;
      std::string title;
      if (!typeId.empty()) title = typeId + "::" + m->field("op");
      const std::string change = m->field("change");  // a diff marks messages too
      if (!change.empty()) {
        refs["change"] = change;
        const std::string detail = m->field("change-detail");
        title = change + (detail.empty() ? "" : ": " + detail) + (title.empty() ? "" : "\n" + title);
      }
      if (!expands.empty()) {
        refs["expands"] = expands;
        title += (title.empty() ? "" : "\n") + std::string(goesOn ? "expanded below: " : "expands to: ") + model.find(expands)->name;
        if (!goesOn) edge["href"] = "#" + expands;  // a page that also shows the child lands on it
      }
      if (!title.empty()) edge["title"] = title;
      const size_t at = edges.size();
      edges.push_back(edge);
      if (goesOn) {
        const int first = order;
        emit(*model.find(expands), target, from, taken.nextPath, depth - 1, stack);
        if (order > first) refs["span"] = std::to_string(first) + "-" + std::to_string(order - 1);  // the rows the expansion occupies
      }
      edges[at]["refs"] = refs;
      const std::string result = m->field("result");
      if (!result.empty())
        edges.push_back({{"from", target}, {"to", from}, {"kind", "message"}, {"order", order++}, {"label", result}, {"dashed", true}});
      for (std::string f = frame; !f.empty(); f = tree.frameOf(f))
        if (drawn(f)) lastOrder[f] = order - 1;  // after any expansion
    }
    flushEmpty(std::numeric_limits<int>::max());
    for (const auto& f : firstOrder) {
      const model::Entity* fe = model.find(f.first);
      nlohmann::json frame = {
          {"id", f.first},
          {"kind", "frame"},
          {"label", fe->field("kind") == "handler" ? "on " + fe->field("failure") : fe->field("kind") + " " + fe->field("condition")},
          {"classifier", fe->field("kind")},
          {"span", nlohmann::json::array({f.second, lastOrder[f.first]})}};
      if (elseOrder.count(f.first)) frame["dividers"] = nlohmann::json::array({{{"at", elseOrder[f.first]}, {"label", "else"}}});
      nodes.push_back(frame);
    }
    stack.erase(seq.id);
  }
};

}  // namespace

std::string IrExport::renderSequence(const model::Entity& diagram, bool pretty) const {
  const int depth = levelsOf(diagram);
  // the behaviours selected, by subject id, by sequence id, or a namespace: the query's filter (DS-127)
  std::vector<const model::Entity*> selected;
  std::map<const model::Entity*, std::string> pathOf;  // a named branch asked for: the path of that behaviour
  for (const query::Behaviour& taken :
       query::SequenceFilter(splitTrimmed(diagram.field("include"), ','), depth, diagram.id).behaviours(model_)) {
    selected.push_back(taken.sequence);
    if (taken.path != kWhole) pathOf[taken.sequence] = taken.path;
  }
  SequenceBuilder b{model_, nlohmann::json::array(), nlohmann::json::array(), {}, 0, nullptr, {}};
  // the lifelines stand on the cut the diagram writes, and the calls followed are the ones it says (DS-231)
  const axis::Cut cut(model_, diagram);
  b.cut = &cut;
  nlohmann::json out;
  out["ir"] = 1;
  out["title"] = diagram.name;
  std::set<std::string> subjects;  // the lifelines of the boxes and use cases whose scenarios are drawn
  for (const model::Entity* seq : selected) {
    const std::string subjectId = model::Identity(seq->id).subject().text();
    const model::Entity* subject = model_.find(subjectId);
    const bool written =
        subject && (subject->kind == model::Kind::Package || subject->kind == model::Kind::Module || subject->kind == model::Kind::UseCase);
    // a method runs on its class, a function on its module
    const bool function = subject && subject->kind == model::Kind::Function;
    const bool method = subject && (subject->kind == model::Kind::Method || subject->kind == model::Kind::Operation || function);
    const model::Entity* cls = method ? model_.find(subject->scope) : subject;
    // the caller, leftmost, invokes the entry operation on the subject: what is being looked at is the first thing drawn
    const std::string caller = method ? b.lifeline("caller", "caller", "actor") : "";
    const std::string selfLabel = cls ? (function ? cls->name : "self: " + cls->name) : "self: " + subjectId;
    const std::string around = cls ? b.closedAround(cls->id) : "";
    // a scenario of a use case starts from whoever asks; one of a box runs on the box (DS-232)
    const std::string self = written && subject->kind == model::Kind::UseCase ? b.lifeline("caller", "caller", "actor")
                             : written                                        ? b.lineOf(subjectId, subject->name, seq->id)
                             : !around.empty()                                ? b.boxLine(around)
                                               : b.lifeline(cls ? cls->id : subjectId, selfLabel, "participant", cls ? cls->id : "");
    if (written) subjects.insert(self);
    if (method) {
      b.edges.push_back(
          {{"from", caller},
           {"to", self},
           {"kind", "message"},
           {"order", b.order++},
           {"label", (around.empty() || around == cls->id ? "" : cls->name + "::") + subject->name + "(" + subject->field("params") + ")"},
           {"title", subject->id},
           {"refs", {{"location", subject->location.file + ":" + std::to_string(subject->location.line)}}}});
    }
    std::set<std::string> stack;
    // a behaviour asked for by its subject is drawn whole; a scenario of it, as its path
    const std::string path = pathOf.count(seq) ? pathOf[seq] : kWhole;
    b.emit(*seq, self, caller, path, depth, stack);
    const std::string returns = method ? subject->field("returns") : "";
    if (method && !returns.empty()) {  // a return written is drawn as written: ds knows no language's nothing (DS-18)
      std::string given = returns;     // what its `return` says, or the declared type
      for (const query::Taken& taken : query::Walker(model_).steps(seq->id, path)) {
        const model::Entity* m = model_.find(taken.message);
        if (m->field("return") == "true" && !m->field("result").empty()) given = m->field("result");
      }
      b.edges.push_back({{"from", self}, {"to", caller}, {"kind", "message"}, {"order", b.order++}, {"label", given}, {"dashed", true}});
    }
  }
  // a scenario written between boxes may never send from its own subject: that lifeline is drawn when a message
  // touches it (DS-232)
  std::set<std::string> touched;
  for (const auto& e : b.edges) {
    touched.insert(e["from"].get<std::string>());
    touched.insert(e["to"].get<std::string>());
  }
  nlohmann::json kept = nlohmann::json::array();
  for (const auto& n : b.nodes)
    if (!subjects.count(n["id"].get<std::string>()) || touched.count(n["id"].get<std::string>())) kept.push_back(n);
  b.nodes = kept;
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
  for (const auto& machine : query::StateFilter(splitTrimmed(diagram.field("include"), ',')).machines(model_)) {  // DS-127
    const model::Entity* state = model_.find(machine);
    const machine::Machine written(model_, *state);  // a state a rule computes shows its rule (DS-249)
    const std::vector<machine::Call> calls = written.calls();
    const auto box = [&](const std::string& name) {
      const std::string id = state->id + "::" + name;
      if (!drawn.insert(id).second) return id;
      nlohmann::json node = {{"id", id}, {"kind", "box"}, {"label", name}, {"classifier", "state"}};
      const std::vector<std::string> rules = written.rules(name);
      if (!rules.empty()) {
        nlohmann::json lines = nlohmann::json::array();
        for (const std::string& rule : rules)
          for (const std::string& line : wrap("if " + rule, 40)) lines.push_back(line);
        node["sections"] = nlohmann::json::array({lines});
        node["sectionKinds"] = nlohmann::json::array({"attributes"});
      }
      if (const model::Entity* view = model_.find(id)) {
        node["refs"] = {{"location", view->location.file + ":" + std::to_string(view->location.line)}};
        nlohmann::json sections = nlohmann::json::array();
        nlohmann::json kinds = nlohmann::json::array();
        const std::string text = view->field("text");
        if (!text.empty()) {
          sections.push_back(wrap(text, 36));
          kinds.push_back("attributes");
        }
        std::vector<std::string> binds;
        for (const auto& r : model_.index().from(id)) {
          if (r.kind == "satisfies" || r.kind == "constrained-by") binds.push_back(r.to);
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
    const std::string initial = written.initial();  // the one written, else where the defaults are (DS-249)
    if (!initial.empty()) {
      const std::string start = state->id + "::initial";
      nodes.push_back({{"id", start}, {"kind", "box"}, {"label", "start"}, {"classifier", "initial"}});
      edges.push_back({{"from", start}, {"to", box(initial)}, {"kind", "association"}});
    }
    std::stringstream first(state->field("transitions"));
    for (std::string move; std::getline(first, move);) {  // in the order written: the first jumps go down
      const auto cut = move.find('>');
      if (cut == std::string::npos) continue;
      box(move.substr(0, cut));
      const auto end = move.find('>', cut + 1);
      if (end != std::string::npos) box(move.substr(cut + 1, end - cut - 1));
    }
    for (const std::string& name : written.states()) box(name);     // a state a rule computes is drawn though no jump names it
    for (const auto& member : model_.index().members(state->id)) {  // a view no transition names is still drawn: it is the finding
      const model::Entity& view = *model_.find(member);
      if (view.kind == model::Kind::View) box(view.name);
    }
    std::stringstream moves(state->field("transitions"));
    for (std::string move; std::getline(moves, move);) {
      const auto arrow = move.find('>');
      const auto second = move.find('>', arrow + 1);
      if (arrow == std::string::npos || second == std::string::npos) continue;
      // a jump says the behaviours that call its event (DS-250): the label stays as it is written
      const std::string label = move.substr(second + 1);
      const std::string event = label.substr(0, label.find_first_of(" ["));
      std::vector<std::string> callers;
      for (const machine::Call& call : calls)
        if (call.event == event && std::find(callers.begin(), callers.end(), call.behaviour) == callers.end())
          callers.push_back(call.behaviour);
      std::string in;
      for (const std::string& caller : callers) in += (in.empty() ? "" : ", ") + caller;
      nlohmann::json edge = {{"from", box(move.substr(0, arrow))},
                             {"to", box(move.substr(arrow + 1, second - arrow - 1))},
                             {"kind", "association"},
                             {"label", label}};
      if (!in.empty()) edge["title"] = event + " is called in " + in;  // one nobody calls says nothing
      edges.push_back(edge);
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

// ---- a machine as a table -----------------------------------------------------------------
// What the table says is ds::machine's; here it becomes the table of the IR: a cell that is a jump, one
// where the machine has none, and one in which a behaviour calls the event against the machine.

std::string IrExport::renderTable(const model::Entity& diagram, bool pretty) const {
  nlohmann::json out = {{"ir", 1}, {"title", diagram.name}, {"nodes", nlohmann::json::array()}, {"edges", nlohmann::json::array()}};
  const auto listed = [](const std::vector<std::string>& names) {
    std::string all;
    for (const std::string& name : names) all += (all.empty() ? "" : ", ") + name;
    return all;
  };
  const auto written = model_.tables().find(diagram.id);
  if (written != model_.tables().end() && !written->second.columns.empty()) {
    // written with its rows and columns (DS-268): the first column heads each row
    const model::Tabulated& t = written->second;
    nlohmann::json rows = nlohmann::json::array();
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
      nlohmann::json cells = nlohmann::json::array();
      for (std::size_t c = 1; c < t.rows[r].size(); ++c)
        cells.push_back({{"text", t.rows[r][c]}, {"kind", t.kinds[r][c].empty() ? "jump" : t.kinds[r][c]}, {"title", ""}});
      rows.push_back({{"label", t.rows[r][0]}, {"title", ""}, {"cells", cells}});
    }
    out["table"] = {
        {"corner", t.columns[0]}, {"columns", std::vector<std::string>(t.columns.begin() + 1, t.columns.end())}, {"rows", rows}};
    return pretty ? out.dump(2) : out.dump();
  }
  for (const auto& id : query::StateFilter(splitTrimmed(diagram.field("include"), ',')).machines(model_)) {
    const machine::Table table(model_, *model_.find(id));
    nlohmann::json rows = nlohmann::json::array();
    for (const machine::Row& row : table.rows()) {
      nlohmann::json cells = nlohmann::json::array();
      for (std::size_t i = 0; i < row.cells.size(); ++i) {
        const machine::Cell& cell = row.cells[i];
        const std::string& state = table.states()[i];
        if (!cell.against.empty())
          cells.push_back(
              {{"text", listed(cell.against)},
               {"kind", "against"},
               {"title", listed(cell.against) + " calls " + row.event + " when it can be " + state + ": the machine has no jump here"}});
        else if (cell.to.empty())
          cells.push_back({{"text", ""}, {"kind", "none"}, {"title", ""}});
        else
          cells.push_back({{"text", cell.to}, {"kind", "jump"}, {"title", row.event + " takes " + state + " to " + cell.to}});
      }
      rows.push_back({{"label", row.event},
                      {"title", row.callers.empty() ? "" : row.event + " is called in " + listed(row.callers)},
                      {"cells", cells}});
    }
    out["table"] = {{"corner", table.subject()}, {"columns", table.states()}, {"rows", rows}};
    break;  // one machine is one table
  }
  if (!out.contains("table")) out["notice"] = "No state machine selected: include names what it is of (shop@order::Order)";
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
  const std::vector<std::string> showList = splitTrimmed(diagram.field("show"), ',');
  const std::set<std::string> show(showList.begin(), showList.end());
  const bool withDesign = show.empty() || show.count("design");
  const bool withVerifications = show.empty() || show.count("verifications");
  // the roots and their umbrella are the query's filter of the diagram (DS-127)
  const std::set<std::string> selected =
      query::RequirementsFilter(splitTrimmed(diagram.field("include"), ','), withDesign, withVerifications, diagram.id).select(model_);
  const axis::Cut cut(model_, diagram);  // what answers to a requirement is the box that stands for it (DS-231)
  nlohmann::json out;
  out["ir"] = 1;
  out["title"] = diagram.name;
  out["nodes"] = nlohmann::json::array();
  out["edges"] = nlohmann::json::array();
  for (const auto& id : selected) {
    const model::Entity& e = *model_.find(id);
    // an artefact, or a use case, is drawn as its card: id, stereotype with its status, title
    const bool card = isArtefact(e) || e.kind == model::Kind::UseCase;
    nlohmann::json node = {{"id", id}, {"kind", "box"}, {"label", card ? e.id : e.name}, {"classifier", model_.metamodel().nameOf(e.kind)}};
    if (card) {
      std::vector<std::string> lines = wrap(e.field("title"), 44);
      const std::string status = e.field("status");
      const std::string method = e.field("method");
      std::string stereotype = model_.metamodel().nameOf(e.kind);
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
  return pretty ? out.dump(2) : out.dump();
}

}  // namespace render
}  // namespace ds
