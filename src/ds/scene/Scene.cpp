#include "ds/scene/Scene.hpp"

#include <miniz.h>

#include <algorithm>
#include <cmath>
#include <cstring>
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
const double kTreeGap = 44;
const double kUseCaseGap = 70;  // between the column of use cases and the interfaces that realise them  // between a base and the row of
                                // what inherits from it: room for the trunk

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
      if (r.kind == "inherits" || r.kind == "realises") bases_[r.from].push_back(r.to);
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
  std::map<std::string, std::vector<std::string>> bases_;   // what each class inherits from or realises

  // the taxonomy of a module: each unit under the first base it has in the same module
  std::map<std::string, std::string> taxonomy(const std::vector<std::pair<std::string, std::pair<double, double>>>& units) const {
    std::set<std::string> here;
    for (const auto& u : units) here.insert(u.first);
    std::map<std::string, std::string> base;
    for (const auto& u : units) {
      const auto it = bases_.find(u.first);
      if (it == bases_.end()) continue;
      for (const auto& b : it->second) {
        if (b == u.first || !here.count(b)) continue;
        std::string up = b;  // never under one of its own descendants
        while (base.count(up) && up != u.first) up = base.at(up);
        if (up == u.first) continue;
        base[u.first] = b;
        break;
      }
    }
    return base;
  }

  // a tree of a taxonomy as one block: the base centred over the row of what inherits from it
  std::string tree(const std::string& root, const std::map<std::string, std::vector<std::string>>& below) {
    const auto kids = below.find(root);
    if (kids == below.end()) return root;
    Shape block;
    std::vector<std::pair<std::string, std::pair<double, double>>> row;
    double x = 0, rowH = 0;
    for (const auto& k : kids->second) {
      const std::string b = tree(k, below);
      row.push_back({b, {x, 0}});
      x += shapes[b].w + kUnitGap;
      rowH = std::max(rowH, shapes[b].h);
    }
    const double rowW = x - kUnitGap, baseW = shapes[root].w, baseH = shapes[root].h;
    block.w = std::max(rowW, baseW);
    block.kids.push_back({root, {(block.w - baseW) / 2, 0}});
    for (auto& r : row) {
      r.second = {r.second.first + (block.w - rowW) / 2, baseH + kTreeGap};
      block.kids.push_back(r);
    }
    block.h = baseH + kTreeGap + rowH;
    const std::string id = root + "#tree";
    shapes[id] = block;
    return id;
  }
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

  // what a box says it is: interface and abstract are derived by the checker (R14), never written
  static std::string stereotypeOf(const model::Entity& e) {
    if (e.has("dataclass")) return "dataclass";
    const std::string derived = field(e, "stereotypes");
    if (derived.find("interface") != std::string::npos) return "interface";
    if (derived.find("abstract") != std::string::npos) return "abstract";
    return model::kindName(e.kind);
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
    double title = static_cast<double>(characters(e.name));
    const auto bases = bases_.find(e.id);  // a header may name what it inherits from: Console ◁ Channel
    if (bases != bases_.end())
      for (const auto& b : bases->second) title += static_cast<double>(characters(b.substr(b.rfind(':') + 1))) * 0.85 + 3;
    double widest = title * 1.6 + 14;
    for (const auto& r : rows) widest = std::max(widest, static_cast<double>(characters(r.label)));
    const double w = std::max(kUnitMinW, std::min(kUnitMaxW, widest * kCharWidth + 18));
    Shape s{w, kHead + static_cast<double>(rows.size()) * kRow + kUnitPadBottom, {}};
    for (size_t i = 0; i < rows.size(); ++i) {
      row(rows[i].id, e.id, rows[i].label, rows[i].stereotype, rows[i].vis, rows[i].from, w, kRow);
      s.kids.push_back({rows[i].id, {0, kHead + static_cast<double>(i) * kRow}});
    }
    node(e.id, "unit", parent, e.name, stereotypeOf(e), visibility(e), e.id);
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
    // the taxonomy shapes the module: every tree of inheritance is one block, base above
    const auto base = taxonomy(units);
    std::map<std::string, std::vector<std::string>> below;
    for (const auto& u : units) {
      const auto b = base.find(u.first);
      if (b != base.end()) below[b->second].push_back(u.first);
    }
    std::vector<std::pair<std::string, std::pair<double, double>>> blocks;
    for (const auto& u : units) {
      if (!base.count(u.first)) blocks.push_back({tree(u.first, below), {0, 0}});
    }
    std::stable_sort(blocks.begin(), blocks.end(), [&](const auto& a, const auto& b) {
      const bool aTree = a.first.size() > 5 && a.first.compare(a.first.size() - 5, 5, "#tree") == 0;
      const bool bTree = b.first.size() > 5 && b.first.compare(b.first.size() - 5, 5, "#tree") == 0;
      return aTree != bTree ? aTree : false;
    });
    units = blocks;
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

  // a use case: its title, its contract and its outcomes, one row each (DS-77)
  std::string usecase(const model::Entity& e, const std::string& parent) {
    std::vector<std::pair<std::string, std::string>> rows;  // id, label
    const std::string title = field(e, "title");
    if (!title.empty()) rows.push_back({e.id + "#title", title});
    const std::string operation = field(e, "operation");
    if (!operation.empty()) rows.push_back({e.id + "#sig", signature(operation, e.params, field(e, "returns"))});
    for (const model::Entity* o : children(e.id)) {
      if (o->kind != model::Kind::Outcome) continue;
      const std::string returns = field(*o, "returns");
      rows.push_back({o->id, o->name + (returns.empty() ? "" : " → " + unqualified(returns)) + " · " + field(*o, "text")});
    }
    double widest = static_cast<double>(characters(e.name)) * 1.6 + 14;
    for (const auto& r : rows) widest = std::max(widest, static_cast<double>(characters(r.second)));
    const double w = std::max(kUnitMinW, std::min(kUnitMaxW, widest * kCharWidth + 18));
    Shape s{w, kHead + static_cast<double>(rows.size()) * kRow + kUnitPadBottom, {}};
    for (size_t i = 0; i < rows.size(); ++i) {
      const bool outcome = rows[i].first.find('#') == std::string::npos;
      row(rows[i].first, e.id, rows[i].second, outcome ? "outcome" : rows[i].first.substr(rows[i].first.find('#') + 1), "+",
          outcome ? rows[i].first : "", w, kRow);
      s.kids.push_back({rows[i].first, {0, kHead + static_cast<double>(i) * kRow}});
    }
    node(e.id, "usecase", parent, e.name, "usecase", "+", e.id);
    shapes[e.id] = s;
    return e.id;
  }

  std::string package(const model::Entity& e, const std::string& parent) {
    std::vector<std::pair<std::string, std::pair<double, double>>> boxes, interfaces, usecases;
    for (const model::Entity* k : children(e.id + "@")) {
      if (k->has("extern")) continue;
      if (k->kind == model::Kind::Package) boxes.push_back({package(*k, e.id), {0, 0}});
      if (k->kind == model::Kind::Module) boxes.push_back({module(*k, e.id), {0, 0}});
      if (k->kind == model::Kind::Interface) interfaces.push_back({interface(*k, e.id), {0, 0}});
      if (k->kind == model::Kind::UseCase && !k->expected) usecases.push_back({usecase(*k, e.id), {0, 0}});
    }
    order(boxes);
    const auto size = shelf(boxes, shapes, kBoxGap, width(e.id, target(boxes, shapes, 0, 1.6)));
    keep(e.id, size.first);
    const double left = interfaces.empty() ? kPackagePad : kLeftForInterfaces;
    shift(boxes, left, kPackageHead);
    double y = kPackageHead + 40;
    for (auto& i : interfaces) {  // on the border: half outside, half inside, as a port of the package
      i.second = {-kInterfaceW / 2, y};
      y += shapes[i.first].h + 20;
    }
    // the use cases stand left of the interfaces, outside the package: the generic above what specialises it
    std::map<std::string, std::string> general;
    std::set<std::string> here;
    for (const auto& u : usecases) here.insert(u.first);
    for (const auto& u : usecases) {
      const auto b = bases_.find(u.first);
      if (b != bases_.end())
        for (const auto& to : b->second)
          if (here.count(to) && to != u.first) general[u.first] = to;
    }
    std::vector<std::pair<std::string, int>> column;  // in taxonomy order, with depth
    std::set<std::string> placed;
    std::function<void(const std::string&, int)> walk = [&](const std::string& id, int depth) {
      if (!placed.insert(id).second) return;
      column.push_back({id, depth});
      for (const auto& u : usecases)
        if (general.count(u.first) && general.at(u.first) == id) walk(u.first, depth + 1);
    };
    for (const auto& u : usecases)
      if (!general.count(u.first)) walk(u.first, 0);
    for (const auto& u : usecases) walk(u.first, 0);
    double widest = 0;
    for (const auto& c : column) widest = std::max(widest, shapes[c.first].w + c.second * 30.0);
    double uy = kPackageHead + 40;
    for (const auto& c : column) {
      boxes.push_back({c.first, {-kInterfaceW / 2 - kUseCaseGap - widest + c.second * 30.0, uy}});
      uy += shapes[c.first].h + kTreeGap;
    }
    boxes.insert(boxes.end(), interfaces.begin(), interfaces.end());
    node(e.id, "package", parent, e.name, "package", "+", e.id);
    shapes[e.id] = {size.first + left + kPackagePad, std::max({size.second + kPackageHead + kPackagePad, y + kPackagePad, uy}), boxes};
    return e.id;
  }

  // a box whose classes are inherited from comes before the boxes that inherit: bases read first
  void order(std::vector<std::pair<std::string, std::pair<double, double>>>& boxes) const {
    const auto inside = [](const std::string& id, const std::string& box) { return id.compare(0, box.size() + 2, box + "::") == 0; };
    const auto heirOf = [&](const std::string& heir, const std::string& base) {
      for (const auto& pair : bases_) {
        if (!inside(pair.first, heir)) continue;
        for (const auto& b : pair.second)
          if (inside(b, base)) return true;
      }
      return false;
    };
    std::vector<std::pair<std::string, std::pair<double, double>>> done;
    std::vector<bool> taken(boxes.size(), false);
    while (done.size() < boxes.size()) {
      size_t pick = boxes.size();
      for (size_t i = 0; i < boxes.size() && pick == boxes.size(); ++i) {
        if (taken[i]) continue;
        bool waits = false;
        for (size_t j = 0; j < boxes.size() && !waits; ++j) waits = !taken[j] && j != i && heirOf(boxes[i].first, boxes[j].first);
        if (!waits) pick = i;
      }
      if (pick == boxes.size()) pick = std::find(taken.begin(), taken.end(), false) - taken.begin();  // a cycle: keep the order written
      taken[pick] = true;
      done.push_back(boxes[pick]);
    }
    boxes = done;
  }

  void place(const std::string& id, double x, double y) {
    const Shape& s = shapes.at(id);
    if (nodes.count(id)) nodes[id].full = {x, y, s.w, s.h};  // a tree of a taxonomy only groups
    for (const auto& k : s.kids) place(k.first, x + k.second.first, y + k.second.second);
  }
};

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
    // a class that inherits from an interface realises it: the checker derives the interface (R14)
    const model::Entity* target = model_.find(r.to);
    const bool realises = r.kind == "inherits" && target && field(*target, "stereotypes").find("interface") != std::string::npos;
    const std::string kind = realises ? "realises" : r.kind;
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
    if (!full.nodes.count(id)) {  // a tree of a taxonomy groups units without being drawn
      for (const auto& k : full.shapes.at(id).kids) walk(k.first);
      return;
    }
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
  tour(doc);
  trace(doc);
  return doc;
}

// The document is compact, and says how to read itself: every record is an array whose
// columns "fields" names, and a node, a behaviour or a link is named by its index.
std::string Scene::render(bool pretty) const {
  const SceneDocument doc = build();
  std::map<std::string, int> at;
  std::set<std::string> ids;
  for (size_t i = 0; i < doc.nodes.size(); ++i) {
    at[doc.nodes[i].id] = static_cast<int>(i);
    ids.insert(doc.nodes[i].id);
  }
  const auto index = [&at](const std::string& id) { return at.count(id) ? at.at(id) : -1; };
  const auto ints = [](const Box& b) {
    return nlohmann::json::array({std::lround(b.x), std::lround(b.y), std::lround(b.w), std::lround(b.h)});
  };
  std::map<std::string, int> behaviourOf;  // "subject / scenario" -> its index
  for (size_t i = 0; i < doc.behaviours.size(); ++i) behaviourOf[doc.behaviours[i].id] = static_cast<int>(i);

  nlohmann::json out;
  out["version"] = 2;
  out["fields"] = {{"node", {"id", "kind", "parent", "label", "stereotype", "visibility", "requirements", "full", "lean"}},
                   {"link", {"from", "to", "kind", "count", "multiplicity", "behaviours"}},
                   {"behaviour", {"subject", "scenario", "calls"}},
                   {"call", {"to", "label", "behaviour", "result"}},
                   {"tour", {"behaviour", "group"}},
                   {"diagram", {"name", "kind", "nodes"}},
                   {"requirement", {"id", "kind", "title", "status", "text", "parent", "scope", "nodes"}},
                   {"verification", {"id", "title", "status", "method", "tests", "requirements"}}};
  out["nodes"] = nlohmann::json::array();
  for (const auto& n : doc.nodes) {
    out["nodes"].push_back({n.id, n.kind, index(n.parent), n.label, n.stereotype, n.visibility, n.requirements, ints(n.full),
                            n.lean ? ints(n.leanBox) : nlohmann::json()});
  }
  out["world"] = ints(doc.world);
  out["lean_world"] = ints(doc.leanWorld);
  out["links"] = nlohmann::json::array();
  for (const auto& l : doc.links) {
    nlohmann::json written = nlohmann::json::array();
    for (const auto& s : l.sequences) {  // a sequence id, X::sequence or X::sequence/scenario, as the behaviour it is
      const auto cut = s.find("::sequence");
      const std::string scenario = s.size() > cut + 10 ? s.substr(cut + 11) : "";
      const auto b = behaviourOf.find(nodeFor(ids, s.substr(0, cut)) + (scenario.empty() ? "" : " / " + scenario));
      if (b != behaviourOf.end()) written.push_back(b->second);
    }
    out["links"].push_back({index(l.from), index(l.to), l.kind, l.count, l.multiplicity, written});
  }
  out["behaviours"] = nlohmann::json::array();
  for (const auto& b : doc.behaviours) {
    nlohmann::json calls = nlohmann::json::array();
    for (const auto& c : b.calls) calls.push_back({index(c.to), c.label, c.expands, c.result});
    out["behaviours"].push_back({index(b.subject), b.scenario, calls});
  }
  out["tours"] = nlohmann::json::array();
  for (const auto& t : doc.tours) out["tours"].push_back({t.behaviour, t.group});
  out["diagrams"] = nlohmann::json::array();
  for (const auto& d : doc.diagrams) {
    nlohmann::json shown = nlohmann::json::array();
    for (const auto& n : d.nodes) shown.push_back(index(n));
    out["diagrams"].push_back({d.name, d.kind, shown});
  }
  out["titles"] = doc.titles;
  std::map<std::string, int> requirementAt;
  for (size_t i = 0; i < doc.requirements.size(); ++i) requirementAt[doc.requirements[i].id] = static_cast<int>(i);
  const auto requirement = [&requirementAt](const std::string& id) { return requirementAt.count(id) ? requirementAt.at(id) : -1; };
  out["requirements"] = nlohmann::json::array();
  for (const auto& r : doc.requirements) {
    nlohmann::json shown = nlohmann::json::array();
    for (const auto& n : r.nodes) shown.push_back(index(n));
    out["requirements"].push_back({r.id, r.kind, r.title, r.status, r.text, requirement(r.parent), index(r.scope), shown});
  }
  out["verifications"] = nlohmann::json::array();
  for (const auto& v : doc.verifications) {
    nlohmann::json proves = nlohmann::json::array();
    for (const auto& r : v.requirements)
      if (requirement(r) >= 0) proves.push_back(requirement(r));
    out["verifications"].push_back({v.id, v.title, v.status, v.method, v.tests, proves});
  }
  return pretty ? out.dump(2) : out.dump();
}

namespace {

const size_t kMaxSteps = 2000;

std::string fieldOf(const model::Entity& e, const std::string& key) {
  const auto it = e.fields.find(key);
  return it == e.fields.end() ? "" : it->second;
}

/// Writes each behaviour once: a sequence along a scenario, its calls in order, each call naming
/// the behaviour it goes on into (found or written the first time it is reached).
struct Behaviours {
  const model::Model& model;
  const std::set<std::string>& ids;
  std::map<std::string, std::string> calls;  // message -> the operation it calls
  std::map<std::pair<std::string, std::string>, int> index;
  std::vector<Behaviour>& out;

  int of(const std::string& sequence, const std::string& scenario) {
    const auto key = std::make_pair(sequence, scenario);
    const auto found = index.find(key);
    if (found != index.end()) return found->second;
    const int at = static_cast<int>(out.size());
    index[key] = at;  // before its calls: a behaviour that reaches itself names itself
    const auto cut = sequence.find("::sequence");
    Behaviour b;
    b.subject = nodeFor(ids, sequence.substr(0, cut));
    b.scenario = scenario.empty() && sequence.size() > cut + 10 ? sequence.substr(cut + 11) : scenario;
    b.id = b.subject + (b.scenario.empty() ? "" : " / " + b.scenario);
    out.push_back(b);
    std::vector<Call> found_calls;
    for (const model::Entity* m : model::pathOf(model, sequence, scenario, nullptr)) {
      const auto call = calls.find(m->id);
      if (call == calls.end()) continue;
      const std::string callee = nodeFor(ids, call->second);
      if (callee.empty()) continue;
      std::string args;
      for (const auto& a : m->args) args += (args.empty() ? "" : ", ") + a;
      const std::string op = fieldOf(*m, "op");
      Call c{callee, op.substr(op.rfind("::") == std::string::npos ? 0 : op.rfind("::") + 2) + "(" + args + ")", -1, fieldOf(*m, "result")};
      // a scenario written apart is a sequence of its own; one written as a branch is a path through the base
      const std::string expands = fieldOf(*m, "expands");
      if (!expands.empty()) {
        if (model.find(expands)) {
          c.expands = of(expands, "");
        } else if (expands.find('/') != std::string::npos) {
          const auto slash = expands.find('/');
          if (model.find(expands.substr(0, slash))) c.expands = of(expands.substr(0, slash), expands.substr(slash + 1));
        }
      }
      found_calls.push_back(c);
    }
    out[static_cast<size_t>(at)].calls = found_calls;
    return at;
  }
};

}  // namespace

void Scene::tour(SceneDocument& out_document) const {
  std::set<std::string> ids;
  for (const auto& n : out_document.nodes) ids.insert(n.id);
  Behaviours behaviours{model_, ids, {}, {}, out_document.behaviours};
  for (const auto& r : model_.relations()) {
    if (r.kind == "calls") behaviours.calls[r.from] = r.to;
  }
  const auto visible = [](const model::Entity* e) { return e && fieldOf(*e, "visibility") != "-"; };
  for (const auto& pair : model_.entities()) {
    const model::Entity& seq = pair.second;
    if (seq.kind != model::Kind::Sequence) continue;
    const auto at = seq.id.find("::sequence");
    if (at == std::string::npos) continue;
    const model::Entity* subject = model_.find(seq.id.substr(0, at));
    if (!subject) continue;
    const bool operation = subject->kind == model::Kind::Operation;
    const bool member = subject->kind == model::Kind::Method && visible(subject) && visible(model_.find(subject->scope));
    const bool function = subject->kind == model::Kind::Function && visible(subject);
    if (!operation && !member && !function) continue;  // a private behaviour is replayed inside the public one that reaches it
    if (nodeFor(ids, subject->id).empty()) continue;
    // its scenarios: the default path and every branch it names, or the one it is written apart for
    std::vector<std::string> scenarios{""};
    if (seq.id.size() == at + 10) {
      std::set<std::string> named;
      for (const auto& f : model_.entities()) {
        if (f.second.kind != model::Kind::Frame || f.second.scope != seq.id) continue;
        for (const char* key : {"scenario", "else-scenario"}) {
          const std::string s = fieldOf(f.second, key);
          if (!s.empty() && named.insert(s).second) scenarios.push_back(s);
        }
      }
    }
    for (const auto& scenario : scenarios) {
      const int b = behaviours.of(seq.id, scenario);
      if (out_document.behaviours[static_cast<size_t>(b)].calls.empty()) continue;
      out_document.tours.push_back({b, operation ? "interface" : "method"});
    }
  }
}

std::vector<Step> Scene::expand(const SceneDocument& document, int behaviour, int depth) {
  std::vector<Step> steps;
  std::set<int> stack;
  std::function<void(int, const std::string&, int)> walk = [&](int b, const std::string& caller, int level) {
    stack.insert(b);
    for (const auto& c : document.behaviours[static_cast<size_t>(b)].calls) {
      if (steps.size() >= kMaxSteps) break;
      steps.push_back({caller, c.to, c.label, level, false});
      if (c.expands < 0 || level >= depth || stack.count(c.expands)) continue;
      walk(c.expands, c.to, level + 1);
      steps.push_back({c.to, caller, c.result.empty() ? "\u21a9" : c.result, level, true});
    }
    stack.erase(b);
  };
  if (behaviour >= 0 && behaviour < static_cast<int>(document.behaviours.size()))
    walk(behaviour, document.behaviours[static_cast<size_t>(behaviour)].subject, 1);
  return steps;
}

namespace {

const char kBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64(const unsigned char* data, size_t size) {
  std::string out;
  out.reserve((size + 2) / 3 * 4);
  for (size_t i = 0; i < size; i += 3) {
    const unsigned v = (static_cast<unsigned>(data[i]) << 16) | (i + 1 < size ? static_cast<unsigned>(data[i + 1]) << 8 : 0u) |
                       (i + 2 < size ? static_cast<unsigned>(data[i + 2]) : 0u);
    out += kBase64[(v >> 18) & 63];
    out += kBase64[(v >> 12) & 63];
    out += i + 1 < size ? kBase64[(v >> 6) & 63] : '=';
    out += i + 2 < size ? kBase64[v & 63] : '=';
  }
  return out;
}

std::string fromBase64(const std::string& text) {
  std::string out;
  unsigned v = 0;
  int bits = -8;
  for (const char c : text) {
    if (c == '=' || c == 0) break;
    const char* at = std::strchr(kBase64, c);
    if (!at) break;
    v = ((v << 6) | static_cast<unsigned>(at - kBase64)) & 0xFFFFFF;
    bits += 6;
    if (bits >= 0) {
      out += static_cast<char>((v >> bits) & 0xFF);
      bits -= 8;
    }
  }
  return out;
}

}  // namespace

std::string Scene::pack(const std::string& text) {
  mz_ulong size = mz_compressBound(static_cast<mz_ulong>(text.size()));
  std::vector<unsigned char> out(size);
  if (mz_compress2(out.data(), &size, reinterpret_cast<const unsigned char*>(text.data()), static_cast<mz_ulong>(text.size()),
                   MZ_BEST_COMPRESSION) != MZ_OK)
    return "";
  return base64(out.data(), size);
}

std::string Scene::unpack(const std::string& packed) {
  const std::string bytes = fromBase64(packed);
  for (mz_ulong room = static_cast<mz_ulong>(bytes.size()) * 8 + 1024; room < (1ul << 30); room *= 2) {
    std::vector<unsigned char> out(room);
    mz_ulong size = room;
    const int status =
        mz_uncompress(out.data(), &size, reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<mz_ulong>(bytes.size()));
    if (status == MZ_OK) return std::string(out.begin(), out.begin() + static_cast<long>(size));
    if (status != MZ_BUF_ERROR) return "";
  }
  return "";
}

std::string Scene::page(const std::string& title, const std::string& css, const std::vector<std::string>& focus) const {
  const std::string json = pack(render(false));  // base64 holds no quote and no tag: nothing closes the script, nothing to escape
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
  // the viewer's code travels packed as well: the page carries the scene and the code as two packs
  // and a loader that inflates the code and runs it, so the page stays small as the viewer grows
  // (the scene and the focus stay outside the pack: they are what changes from one page to another)
  const std::string open = "<script>", close = "</script>";
  const std::string mark = "const PACKED='" + json + "';", aim = "const FOCUS=" + framed + ";";
  const size_t from = out.find(open, out.find("<body>")), to = out.rfind(close);  // a reader's stylesheet may hold "<script>"
  if (from == std::string::npos || to == std::string::npos || to < from) return out;
  std::string code = out.substr(from + open.size(), to - from - open.size());
  for (const std::string* cut : {&mark, &aim}) {
    const size_t at = code.find(*cut);
    if (at == std::string::npos) return out;
    code.erase(at, cut->size());
  }
  const std::string loader =
      "\n" + mark + "\n" + aim + "\nconst VIEWER='" + pack(code) +
      "';\n(async()=>{try{const b=Uint8Array.from(atob(VIEWER),c=>c.charCodeAt(0));"
      "const t=await new Response(new Blob([b]).stream().pipeThrough(new DecompressionStream('deflate'))).text();"
      "const s=document.createElement('script');s.textContent=t;document.body.appendChild(s);}"
      "catch(e){document.body.insertAdjacentHTML('beforeend','<p style=\"padding:20px;font-family:sans-serif\">"
      "This browser cannot inflate the viewer (it lacks DecompressionStream). Open the page in a current one.</p>');}})();\n";
  out.replace(from + open.size(), to - from - open.size(), loader);
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

void Scene::trace(SceneDocument& out_document) const {
  std::set<std::string> ids;
  for (const auto& n : out_document.nodes) ids.insert(n.id);
  std::map<std::string, size_t> at;
  std::map<std::string, size_t> verificationAt;
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (e.expected) continue;
    if (e.kind == model::Kind::Requirement || e.kind == model::Kind::Constraint) {
      Requirement r;
      r.id = e.id;
      r.kind = e.kind == model::Kind::Requirement ? "requirement" : "constraint";
      r.title = fieldOf(e, "title");
      r.status = fieldOf(e, "status");
      r.text = fieldOf(e, "text");
      if (!e.scope.empty()) r.scope = nodeFor(ids, e.scope.back() == '@' ? e.scope.substr(0, e.scope.size() - 1) : e.scope);
      at[r.id] = out_document.requirements.size();
      out_document.requirements.push_back(r);
    } else if (e.kind == model::Kind::Verification) {
      Verification v;
      v.id = e.id;
      v.title = fieldOf(e, "title");
      v.status = fieldOf(e, "status");
      v.method = fieldOf(e, "method");
      std::string test;
      for (const char c : fieldOf(e, "tests") + ",") {  // the tests are written one per line or separated by commas
        if (c == ',' || c == '\n') {
          test.erase(0, test.find_first_not_of(" \t"));
          test.erase(test.find_last_not_of(" \t") + 1);
          if (!test.empty()) v.tests.push_back(test);
          test.clear();
        } else {
          test += c;
        }
      }
      verificationAt[v.id] = out_document.verifications.size();
      out_document.verifications.push_back(v);
    }
  }
  for (const auto& r : model_.relations()) {
    if (r.kind == "refines" && at.count(r.from)) {
      out_document.requirements[at[r.from]].parent = r.to;
    } else if (r.kind == "verifies" && verificationAt.count(r.from)) {
      out_document.verifications[verificationAt[r.from]].requirements.push_back(r.to);
    } else if ((r.kind == "requires" || r.kind == "constrained-by") && at.count(r.to)) {
      const std::string node = nodeFor(ids, r.from);
      auto& nodes = out_document.requirements[at[r.to]].nodes;
      if (!node.empty() && std::find(nodes.begin(), nodes.end(), node) == nodes.end()) nodes.push_back(node);
    }
  }
}

}  // namespace scene
}  // namespace ds
