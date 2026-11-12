#include "ds/scene/Layout.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <utility>

namespace ds {
namespace scene {

namespace {

// Sizes in scene units. A row is one member; a unit is sized by its longest row.
const double kRow = 11, kHead = 22, kUnitPadBottom = 6, kCharWidth = 3.95, kUnitMinW = 150, kUnitMaxW = 430;
const double kModulePad = 24, kModuleHead = 46, kUnitGap = 22;
const double kPackagePad = 60, kPackageHead = 90, kBoxGap = 70, kLeftForInterfaces = 300;
const double kInterfaceW = 360, kOperationRow = 30, kTopGap = 120;
const double kTreeGap = 44;  // between a base and the row of what inherits from it: room for the trunk

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
class Variant {
 public:
  Variant(const model::Model& model, bool lean, std::map<std::string, double>* widths) : lean_(lean), widths_(widths) {
    for (const auto& pair : model.entities()) {
      const model::Entity& e = pair.second;
      if (!e.expected) byScope_[e.scope].push_back(&e);
    }
    for (const auto& r : model.index().ofKinds({"requires", "constrained-by", "inherits", "realises"})) {
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
    std::vector<std::pair<std::string, std::pair<double, double>>> units, boxes, offered;  // offered: the module's own interfaces
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
      if (k->kind == model::Kind::Interface) offered.push_back({interface(*k, e.id), {0, 0}});
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
    units.insert(units.begin(), offered.begin(), offered.end());  // what the module offers reads first
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
    boxes.insert(boxes.end(), interfaces.begin(), interfaces.end());
    node(e.id, "package", parent, e.name, "package", "+", e.id);
    shapes[e.id] = {size.first + left + kPackagePad, std::max(size.second + kPackageHead + kPackagePad, y + kPackagePad), boxes};
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

Layout::Layout(const model::Model& model) : model_(model) {}

// The layout runs twice, once per variant; the full one's order is kept and every node that has a
// lean place gets it.
SceneDocument Layout::place() const {
  std::map<std::string, double> widths;
  Variant full(model_, false, &widths), lean(model_, true, &widths);
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
  return doc;
}

}  // namespace scene
}  // namespace ds
