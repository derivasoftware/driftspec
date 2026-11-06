#include "ds/scene/Scene.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <regex>
#include <set>
#include <utility>

#include "ds/render/Ir.hpp"
#include "ds/render/IrExport.hpp"

namespace ds {
namespace scene {

const char* viewerTemplate();  // viewer/scene.html, compiled into ds by CMake

namespace {

// Sizes in scene units. A row is one member; a unit is sized by its longest row.
const double kRow = 11, kHead = 22, kUnitPadBottom = 6, kCharWidth = 3.95, kUnitMinW = 150, kUnitMaxW = 430;
const double kModulePad = 24, kModuleHead = 46, kUnitGap = 22;
const double kPackagePad = 60, kPackageHead = 90, kBoxGap = 70, kLeftForInterfaces = 300;
const double kInterfaceW = 360, kOperationRow = 30, kTopGap = 120;

/// The size of a box and where its children sit, relative to its top left corner.
struct Shape {
  double w = 0;
  double h = 0;
  std::vector<std::pair<std::string, std::pair<double, double>>> kids;
};

std::string field(const model::Entity& e, const std::string& key) {
  const auto it = e.fields.find(key);
  return it == e.fields.end() ? "" : it->second;
}

std::string visibility(const model::Entity& e) { return field(e, "visibility") == "-" ? "-" : "+"; }

// a qualified type reads better unqualified in a box: std::vector<model::Entity> is vector<Entity>
std::string unqualified(const std::string& type) {
  static const std::regex qualifier(R"(\b[A-Za-z_]\w*::)");
  return std::regex_replace(type, qualifier, "");
}

size_t characters(const std::string& text) {
  size_t n = 0;
  for (const unsigned char c : text) n += (c & 0xC0) != 0x80 ? 1 : 0;
  return n;
}

std::string signature(const std::string& name, const std::vector<model::Param>& params, const std::string& returns) {
  std::string out = name + "(";
  for (size_t i = 0; i < params.size(); ++i) out += (i ? ", " : "") + params[i].name;
  out += ")";
  if (!returns.empty() && returns != "void" && returns != "none") out += " → " + unqualified(returns);
  return out;
}

/// Places boxes left to right in shelves no wider than the target; returns the width and height used.
std::pair<double, double> shelf(std::vector<std::pair<std::string, std::pair<double, double>>>& kids,
                                const std::map<std::string, Shape>& shapes, double gap, double target) {
  double x = 0, y = 0, rowH = 0, width = 0;
  for (auto& kid : kids) {
    const Shape& s = shapes.at(kid.first);
    if (x > 0 && x + s.w > target) {
      x = 0;
      y += rowH + gap;
      rowH = 0;
    }
    kid.second = {x, y};
    x += s.w + gap;
    rowH = std::max(rowH, s.h);
    width = std::max(width, x - gap);
  }
  return {width, y + rowH};
}

double target(const std::vector<std::pair<std::string, std::pair<double, double>>>& kids, const std::map<std::string, Shape>& shapes,
              double atLeast, double factor) {
  double area = 0, widest = atLeast;
  for (const auto& k : kids) {
    const Shape& s = shapes.at(k.first);
    area += s.w * s.h;
    widest = std::max(widest, s.w);
  }
  return std::max(widest, std::sqrt(area) * factor);
}

void shift(std::vector<std::pair<std::string, std::pair<double, double>>>& kids, double dx, double dy) {
  for (auto& k : kids) k.second = {k.second.first + dx, k.second.second + dy};
}

/// Measures and lays out every box of the model once, for one variant (lean hides private members).
class Layout {
 public:
  Layout(const model::Model& model, bool lean, std::map<std::string, double>* widths) : lean_(lean), widths_(widths) {
    for (const auto& pair : model.entities()) {
      const model::Entity& e = pair.second;
      if (!e.expected) byScope_[e.scope].push_back(&e);
    }
    for (const auto& r : model.relations()) {
      if (r.kind == "requires" || r.kind == "constrained-by") linked_[r.from].push_back(r.to);
    }
  }

  std::map<std::string, Shape> shapes;
  std::map<std::string, Node> nodes;
  std::vector<std::string> top;

  void run() {
    std::vector<std::pair<std::string, std::pair<double, double>>> roots;
    for (const model::Entity* e : children("")) {
      if (e->kind == model::Kind::Package && !e->has("extern")) roots.push_back({package(*e, ""), {0, 0}});
    }
    const auto size = shelf(roots, shapes, kTopGap, width("", target(roots, shapes, 0, 1.6)));
    keep("", size.first);
    root_ = {size.first, size.second, roots};
    for (const auto& r : roots) place(r.first, r.second.first, r.second.second);
    for (const auto& r : roots) top.push_back(r.first);
  }

 private:
  bool lean_;
  std::map<std::string, double>* widths_;  // the shelf width of every box in the full layout, kept by the lean one

  // the lean layout fills its shelves no wider than the full one came out, so hiding private
  // members never widens a box, only shortens it
  double width(const std::string& id, double computed) const {
    if (!lean_) return computed;
    const auto kept = widths_->find(id);
    return kept == widths_->end() ? computed : kept->second;
  }
  void keep(const std::string& id, double used) {
    if (!lean_) (*widths_)[id] = used;
  }
  std::map<std::string, std::vector<const model::Entity*>> byScope_;
  std::map<std::string, std::vector<std::string>> linked_;  // requirements and constraints, by the entity linked to them
  Shape root_;

  const std::vector<const model::Entity*>& children(const std::string& scope) {
    static const std::vector<const model::Entity*> none;
    const auto it = byScope_.find(scope);
    return it == byScope_.end() ? none : it->second;
  }

  Node& node(const std::string& id, const std::string& kind, const std::string& parent, const std::string& label,
             const std::string& stereotype, const std::string& vis, const std::string& linkedFrom) {
    Node& n = nodes[id];
    n.id = id;
    n.kind = kind;
    n.parent = parent;
    n.label = label;
    n.stereotype = stereotype;
    n.visibility = vis;
    const auto linked = linked_.find(linkedFrom);
    if (linked != linked_.end()) n.requirements = linked->second;
    return n;
  }

  void row(const std::string& id, const std::string& parent, const std::string& label, const std::string& stereotype,
           const std::string& vis, const std::string& linkedFrom, double w, double h) {
    node(id, "row", parent, label, stereotype, vis, linkedFrom);
    shapes[id] = {w, h, {}};
  }

  std::string unit(const model::Entity& e, const std::string& parent) {
    struct Row {
      std::string id, label, stereotype, vis, from;
    };
    std::vector<Row> rows;
    if (e.kind == model::Kind::Function) {
      rows.push_back({e.id + "#sig", signature(e.name, e.params, field(e, "returns")), "signature", "+", ""});
    } else if (e.kind == model::Kind::Enum) {
      const std::string values = field(e, "values");
      size_t start = 0;
      while (start < values.size()) {
        size_t end = values.find(',', start);
        if (end == std::string::npos) end = values.size();
        std::string v = values.substr(start, end - start);
        v.erase(0, v.find_first_not_of(' '));
        v.erase(v.find_last_not_of(' ') + 1);
        if (!v.empty()) rows.push_back({e.id + "::" + v, v, "value", "+", ""});
        start = end + 1;
      }
    } else {
      std::vector<const model::Entity*> members;
      for (const model::Entity* m : children(e.id)) {
        if (m->kind == model::Kind::Attribute || m->kind == model::Kind::Method || m->kind == model::Kind::Operator) members.push_back(m);
      }
      std::stable_sort(members.begin(), members.end(), [](const model::Entity* a, const model::Entity* b) {
        const bool aAttr = a->kind == model::Kind::Attribute, bAttr = b->kind == model::Kind::Attribute;
        if (aAttr != bAttr) return aAttr;
        return a->location.line < b->location.line;
      });
      for (const model::Entity* m : members) {
        const std::string vis = visibility(*m);
        if (lean_ && vis == "-") continue;
        const std::string label = m->kind == model::Kind::Attribute ? vis + " " + m->name + ": " + unqualified(field(*m, "type"))
                                                                    : vis + " " + signature(m->name, m->params, field(*m, "returns"));
        rows.push_back({m->id, label, model::kindName(m->kind), vis, m->id});
      }
    }
    double widest = static_cast<double>(characters(e.name)) * 1.6 + 14;
    for (const auto& r : rows) widest = std::max(widest, static_cast<double>(characters(r.label)));
    const double w = std::max(kUnitMinW, std::min(kUnitMaxW, widest * kCharWidth + 18));
    Shape s{w, kHead + static_cast<double>(rows.size()) * kRow + kUnitPadBottom, {}};
    for (size_t i = 0; i < rows.size(); ++i) {
      row(rows[i].id, e.id, rows[i].label, rows[i].stereotype, rows[i].vis, rows[i].from, w, kRow);
      s.kids.push_back({rows[i].id, {0, kHead + static_cast<double>(i) * kRow}});
    }
    node(e.id, "unit", parent, e.name, e.has("dataclass") ? "dataclass" : model::kindName(e.kind), visibility(e), e.id);
    shapes[e.id] = s;
    return e.id;
  }

  std::string module(const model::Entity& e, const std::string& parent) {
    std::vector<std::pair<std::string, std::pair<double, double>>> units, boxes;
    std::vector<const model::Entity*> kids(children(e.id).begin(), children(e.id).end());
    std::stable_sort(kids.begin(), kids.end(), [](const model::Entity* a, const model::Entity* b) {
      const bool aPriv = visibility(*a) == "-", bPriv = visibility(*b) == "-";
      if (aPriv != bPriv) return bPriv;
      const bool aFn = a->kind == model::Kind::Function, bFn = b->kind == model::Kind::Function;
      if (aFn != bFn) return bFn;
      return a->name.size() > b->name.size();
    });
    for (const model::Entity* k : kids) {
      if (k->has("extern")) continue;
      if (k->kind == model::Kind::Module) boxes.push_back({module(*k, e.id), {0, 0}});
      if (k->kind == model::Kind::Class || k->kind == model::Kind::Function || k->kind == model::Kind::Enum)
        units.push_back({unit(*k, e.id), {0, 0}});
    }
    std::stable_sort(units.begin(), units.end(), [&](const auto& a, const auto& b) { return shapes[a.first].h > shapes[b.first].h; });
    units.insert(units.end(), boxes.begin(), boxes.end());
    const auto size = shelf(units, shapes, kUnitGap, width(e.id, target(units, shapes, 260, 1.5)));
    keep(e.id, size.first);
    shift(units, kModulePad, kModuleHead);
    node(e.id, "module", parent, e.name, "module", visibility(e), e.id);
    shapes[e.id] = {size.first + 2 * kModulePad, size.second + kModuleHead + kModulePad, units};
    return e.id;
  }

  std::string interface(const model::Entity& e, const std::string& parent) {
    Shape s{kInterfaceW, 0, {}};
    double y = 56;
    for (const model::Entity* op : children(e.id)) {
      if (op->kind != model::Kind::Operation) continue;
      std::string command = field(*op, "command");
      command = command.substr(0, command.find(" ["));
      row(op->id, e.id, op->name + (command.empty() ? "" : "  ·  " + command), "operation", "+", op->id, kInterfaceW - 20, 22);
      s.kids.push_back({op->id, {10, y}});
      y += kOperationRow;
    }
    s.h = y + 20;
    node(e.id, "interface", parent, e.name, "interface", "+", e.id);
    shapes[e.id] = s;
    return e.id;
  }

  std::string package(const model::Entity& e, const std::string& parent) {
    std::vector<std::pair<std::string, std::pair<double, double>>> boxes, interfaces;
    for (const model::Entity* k : children(e.id + "@")) {
      if (k->has("extern")) continue;
      if (k->kind == model::Kind::Package) boxes.push_back({package(*k, e.id), {0, 0}});
      if (k->kind == model::Kind::Module) boxes.push_back({module(*k, e.id), {0, 0}});
      if (k->kind == model::Kind::Interface) interfaces.push_back({interface(*k, e.id), {0, 0}});
    }
    const auto size = shelf(boxes, shapes, kBoxGap, width(e.id, target(boxes, shapes, 0, 1.6)));
    keep(e.id, size.first);
    const double left = interfaces.empty() ? kPackagePad : kLeftForInterfaces;
    shift(boxes, left, kPackageHead);
    double y = kPackageHead + 40;
    for (auto& i : interfaces) {  // on the border: half outside, half inside, as a port of the package
      i.second = {-kInterfaceW / 2, y};
      y += shapes[i.first].h + 20;
    }
    boxes.insert(boxes.end(), interfaces.begin(), interfaces.end());
    node(e.id, "package", parent, e.name, "package", "+", e.id);
    shapes[e.id] = {size.first + left + kPackagePad, std::max(size.second + kPackageHead + kPackagePad, y + kPackagePad), boxes};
    return e.id;
  }

  void place(const std::string& id, double x, double y) {
    const Shape& s = shapes.at(id);
    nodes[id].full = {x, y, s.w, s.h};
    for (const auto& k : s.kids) place(k.first, x + k.second.first, y + k.second.second);
  }
};

nlohmann::json boxJson(const Box& b) {
  const auto r = [](double v) { return std::round(v * 10) / 10; };
  return nlohmann::json::array({r(b.x), r(b.y), r(b.w), r(b.h)});
}

Box bounds(const std::vector<Node>& nodes, bool lean) {
  bool any = false;
  double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  for (const auto& n : nodes) {
    if (lean && !n.lean) continue;
    const Box& b = lean ? n.leanBox : n.full;
    if (!any) {
      x0 = b.x, y0 = b.y, x1 = b.x + b.w, y1 = b.y + b.h, any = true;
      continue;
    }
    x0 = std::min(x0, b.x), y0 = std::min(y0, b.y), x1 = std::max(x1, b.x + b.w), y1 = std::max(y1, b.y + b.h);
  }
  return {x0, y0, x1 - x0, y1 - y0};
}

}  // namespace

Scene::Scene(const model::Model& model) : model_(model) {}

namespace {

// the node that stands for a model identity: itself, a function's signature row, or the nearest
// enclosing node; a message stands for the member whose behaviour it is written in
std::string nodeFor(const std::set<std::string>& ids, std::string id) {
  const auto sequence = id.find("::sequence");
  if (sequence != std::string::npos) id = id.substr(0, sequence);
  for (;;) {
    if (ids.count(id)) return id;
    const auto at = id.rfind("::");
    if (at == std::string::npos) return "";
    id = id.substr(0, at);
  }
}

}  // namespace

void Scene::relate(SceneDocument& out_document) const {
  std::set<std::string> ids;
  std::map<std::string, std::string> parent;
  for (const auto& n : out_document.nodes) {
    ids.insert(n.id);
    parent[n.id] = n.parent;
  }
  static const std::set<std::string> kinds{"inherits", "realises", "owns", "shares", "refs", "uses", "calls"};
  std::map<std::tuple<std::string, std::string, std::string>, Link> links;
  std::map<std::tuple<std::string, std::string, std::string>, std::set<std::string>> sequences;
  for (const auto& r : model_.relations()) {
    if (!kinds.count(r.kind)) continue;
    const std::string from = nodeFor(ids, r.from), to = nodeFor(ids, r.to);
    if (from.empty() || to.empty() || from == to || parent[from] == to || parent[to] == from) continue;  // within one box
    const auto key = std::make_tuple(from, to, r.kind);
    Link& link = links[key];
    link.from = from;
    link.to = to;
    link.kind = r.kind;
    ++link.count;
    if (!r.multiplicity.empty()) link.multiplicity = r.multiplicity;
    if (r.kind == "calls") sequences[key].insert(r.from.substr(0, r.from.rfind("::")));
  }
  for (auto& pair : links) {
    for (const auto& s : sequences[pair.first]) pair.second.sequences.push_back(s);
    out_document.links.push_back(pair.second);
  }
  // where each box is drawn: every diagram of the workspace, through the projection a drawing uses
  const render::IrExport ir(model_);
  for (const auto& id : ir.diagrams()) {
    const std::string text = ir.render(id, false);
    if (text.empty()) continue;
    DiagramView view;
    view.name = id.substr(std::string("diagram:").size());
    const model::Entity* diagram = model_.find(id);
    view.kind = diagram && diagram->fields.count("kind") ? diagram->fields.at("kind") : "";
    std::set<std::string> shown;
    for (const auto& n : render::IrDocument::parse(text).bundled().nodes) {
      const std::string node = nodeFor(ids, n.id);
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

SceneDocument Scene::build() const {
  std::map<std::string, double> widths;
  Layout full(model_, false, &widths), lean(model_, true, &widths);
  full.run();
  lean.run();
  SceneDocument doc;
  // nodes in drawing order: parents before children, as the full layout placed them
  std::function<void(const std::string&)> walk = [&](const std::string& id) {
    Node n = full.nodes.at(id);
    const auto l = lean.nodes.find(id);
    if (l != lean.nodes.end()) {
      n.lean = true;
      n.leanBox = l->second.full;
    }
    doc.nodes.push_back(n);
    for (const auto& k : full.shapes.at(id).kids) walk(k.first);
  };
  for (const auto& t : full.top) walk(t);
  doc.world = bounds(doc.nodes, false);
  doc.leanWorld = bounds(doc.nodes, true);
  relate(doc);
  return doc;
}

std::string Scene::render(bool pretty) const {
  const SceneDocument doc = build();
  nlohmann::json out;
  out["nodes"] = nlohmann::json::array();
  for (const auto& n : doc.nodes) {
    nlohmann::json j = {{"id", n.id},
                        {"kind", n.kind},
                        {"parent", n.parent},
                        {"label", n.label},
                        {"stereotype", n.stereotype},
                        {"visibility", n.visibility},
                        {"requirements", n.requirements},
                        {"full", boxJson(n.full)}};
    j["lean"] = n.lean ? boxJson(n.leanBox) : nlohmann::json();
    out["nodes"].push_back(j);
  }
  out["world"] = boxJson(doc.world);
  out["lean_world"] = boxJson(doc.leanWorld);
  out["links"] = nlohmann::json::array();
  for (const auto& l : doc.links) {
    nlohmann::json j = {{"from", l.from}, {"to", l.to}, {"kind", l.kind}, {"count", l.count}};
    if (!l.multiplicity.empty()) j["multiplicity"] = l.multiplicity;
    if (!l.sequences.empty()) j["sequences"] = l.sequences;
    out["links"].push_back(j);
  }
  out["diagrams"] = nlohmann::json::array();
  for (const auto& d : doc.diagrams) out["diagrams"].push_back({{"name", d.name}, {"kind", d.kind}, {"nodes", d.nodes}});
  out["titles"] = doc.titles;
  return pretty ? out.dump(2) : out.dump();
}

std::string Scene::page(const std::string& title, const std::string& css, const std::vector<std::string>& focus) const {
  std::string json = render(false);
  for (size_t at = json.find("</"); at != std::string::npos; at = json.find("</", at + 2))
    json.replace(at, 2, "<\\/");  // never closes the script
  std::string safe;
  for (const char c : title) safe += c == '<' ? "&lt;" : c == '>' ? "&gt;" : c == '&' ? "&amp;" : std::string(1, c);
  std::string out = viewerTemplate();
  const auto put = [&out](const std::string& mark, const std::string& value) {
    for (size_t at = out.find(mark); at != std::string::npos; at = out.find(mark, at + value.size())) out.replace(at, mark.size(), value);
  };
  std::string style = css;
  for (size_t at = style.find("</"); at != std::string::npos; at = style.find("</", at + 3)) style.replace(at, 2, "<\\/");
  put("/*SCENE*/", json);
  put("/*TITLE*/", safe);
  put("/*CSS*/", style);
  std::string framed = nlohmann::json(focus).dump();
  for (size_t at = framed.find("</"); at != std::string::npos; at = framed.find("</", at + 3)) framed.replace(at, 2, "<\\/");
  put("/*FOCUS*/", framed);
  return out;
}

std::string Scene::frame(const std::string& page, const std::string& title, int height) {
  const auto attribute = [](const std::string& text) {
    std::string out;
    out.reserve(text.size() + text.size() / 8);
    for (const char c : text) out += c == '&' ? "&amp;" : c == '"' ? "&quot;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : std::string(1, c);
    return out;
  };
  return "<iframe title=\"" + attribute(title) + " \u00b7 DriftSpec\" style=\"width:100%;height:" + std::to_string(height) +
         "px;border:0;border-radius:8px\" loading=\"lazy\" allowfullscreen srcdoc=\"" + attribute(page) + "\"></iframe>\n";
}

}  // namespace scene
}  // namespace ds
