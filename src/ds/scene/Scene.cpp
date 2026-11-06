#include "ds/scene/Scene.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <regex>
#include <utility>

namespace ds {
namespace scene {

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
  Layout(const model::Model& model, bool lean) : lean_(lean) {
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
    const auto size = shelf(roots, shapes, kTopGap, target(roots, shapes, 0, 1.6));
    root_ = {size.first, size.second, roots};
    for (const auto& r : roots) place(r.first, r.second.first, r.second.second);
    for (const auto& r : roots) top.push_back(r.first);
  }

 private:
  bool lean_;
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
    const auto size = shelf(units, shapes, kUnitGap, target(units, shapes, 260, 1.5));
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
    const auto size = shelf(boxes, shapes, kBoxGap, target(boxes, shapes, 0, 1.6));
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

SceneDocument Scene::build() const {
  Layout full(model_, false), lean(model_, true);
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
  return pretty ? out.dump(2) : out.dump();
}

}  // namespace scene
}  // namespace ds
