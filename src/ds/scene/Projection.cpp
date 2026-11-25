#include "ds/scene/Projection.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>

#include "ds/model/WrittenType.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace scene {

Projection::Projection(const model::Model& model, const model::Metamodel& metamodel, bool lean) : metamodel_(metamodel), lean_(lean) {
  for (const auto& pair : model.entities()) {
    const model::Entity& e = pair.second;
    if (!e.expected) byScope_[e.scope].push_back(&e);
  }
  for (const auto& r : model.index().ofKinds({"satisfies", "constrained-by", "inherits", "realises"})) {
    if (r.kind == "satisfies" || r.kind == "constrained-by") linked_[r.from].push_back(r.to);
    if (r.kind == "inherits" || r.kind == "realises") bases_[r.from].push_back(r.to);
  }
  root_.arrangement = layout::Frame::Arrangement::Shelves;
  root_.gap = kTopGap;
  root_.factor = 1.6;
  for (const model::Entity* e : children("")) {
    if (e->kind == model::Kind::Package) root_.kids.push_back(package(*e, ""));
  }
}

const layout::Frame& Projection::root() const { return root_; }

const std::map<std::string, Node>& Projection::nodes() const { return nodes_; }

layout::Frame Projection::package(const model::Entity& e, const std::string& parent) {
  layout::Frame f;
  f.id = e.id;
  f.arrangement = layout::Frame::Arrangement::Shelves;
  for (const model::Entity* k : children(e.id + "@")) {
    if (k->kind == model::Kind::Package) f.kids.push_back(package(*k, e.id));
    if (k->kind == model::Kind::Module) f.kids.push_back(module(*k, e.id));
    if (k->kind == model::Kind::Interface) f.ports.push_back(interface(*k, e.id));
  }
  order(f.kids);
  f.gap = kBoxGap;
  f.factor = 1.6;
  f.left = f.ports.empty() ? kPackagePad : kLeftForInterfaces;
  f.top = kPackageHead;
  f.right = kPackagePad;
  f.bottom = kPackagePad;
  f.portTop = kPackageHead + 40;  // the interfaces on the border, as ports of the package
  f.portGap = 20;
  node(e.id, "package", parent, e.name, "package", "+", e.id);
  return f;
}

layout::Frame Projection::module(const model::Entity& e, const std::string& parent) {
  std::vector<layout::Frame> units, boxes, offered;  // offered: the module's own interfaces
  std::vector<const model::Entity*> kids(children(e.id).begin(), children(e.id).end());
  std::stable_sort(kids.begin(), kids.end(), [](const model::Entity* a, const model::Entity* b) {
    const bool aPriv = visibility(*a) == "-", bPriv = visibility(*b) == "-";
    if (aPriv != bPriv) return bPriv;
    const bool aFn = a->kind == model::Kind::Function, bFn = b->kind == model::Kind::Function;
    if (aFn != bFn) return bFn;
    return a->name.size() > b->name.size();
  });
  for (const model::Entity* k : kids) {
    if (k->kind == model::Kind::Module) boxes.push_back(module(*k, e.id));
    if (k->kind == model::Kind::Interface) offered.push_back(interface(*k, e.id));
    if (k->kind == model::Kind::Class || k->kind == model::Kind::Function || k->kind == model::Kind::Enum) units.push_back(unit(*k, e.id));
  }
  std::stable_sort(units.begin(), units.end(), [](const layout::Frame& a, const layout::Frame& b) { return a.h > b.h; });
  // the taxonomy shapes the module: every tree of inheritance is one block, base above
  const auto base = taxonomy(units);
  std::map<std::string, std::vector<std::string>> below;
  for (const layout::Frame& u : units) {
    const auto b = base.find(u.id);
    if (b != base.end()) below[b->second].push_back(u.id);
  }
  std::vector<std::string> written;
  std::map<std::string, layout::Frame> byId;
  for (layout::Frame& u : units) {
    written.push_back(u.id);
    byId[u.id] = std::move(u);
  }
  std::vector<layout::Frame> blocks;
  for (const std::string& id : written) {
    if (!base.count(id)) blocks.push_back(tree(std::move(byId.at(id)), byId, below));
  }
  std::stable_sort(blocks.begin(), blocks.end(), [](const layout::Frame& a, const layout::Frame& b) {
    const bool aTree = a.arrangement == layout::Frame::Arrangement::Tree, bTree = b.arrangement == layout::Frame::Arrangement::Tree;
    return aTree != bTree ? aTree : false;
  });
  layout::Frame f;
  f.id = e.id;
  f.arrangement = layout::Frame::Arrangement::Shelves;
  f.kids = std::move(offered);  // what the module offers reads first
  for (layout::Frame& b : blocks) f.kids.push_back(std::move(b));
  for (layout::Frame& b : boxes) f.kids.push_back(std::move(b));
  f.gap = kUnitGap;
  f.atLeast = 260;
  f.factor = 1.5;
  f.left = kModulePad;
  f.top = kModuleHead;
  f.right = kModulePad;
  f.bottom = kModulePad;
  node(e.id, "module", parent, e.name, "module", visibility(e), e.id);
  return f;
}

layout::Frame Projection::interface(const model::Entity& e, const std::string& parent) {
  layout::Frame f = leaf(e.id, kInterfaceW, 0);
  double y = 56;
  for (const model::Entity* op : children(e.id)) {
    if (op->kind != model::Kind::Operation) continue;
    std::string command = op->field("command");
    command = command.substr(0, command.find(" ["));
    node(op->id, "row", e.id, op->name + (command.empty() ? "" : "  ·  " + command), "operation", "+", op->id);
    f.kids.push_back(leaf(op->id, kInterfaceW - 20, 22));
    f.at.push_back({10, y});
    y += kOperationRow;
  }
  f.h = y + 20;
  node(e.id, "interface", parent, e.name, "interface", "+", e.id);
  return f;
}

layout::Frame Projection::unit(const model::Entity& e, const std::string& parent) {
  struct Row {  // a member, a value or a function's signature
    std::string id, label, stereotype, vis, from;
  };
  std::vector<Row> rows;
  if (e.kind == model::Kind::Function) {
    rows.push_back({e.id + "#sig", signature(e.name, e.params, e.field("returns")), "signature", "+", ""});
  } else if (e.kind == model::Kind::Enum) {
    const std::string values = e.field("values");
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
      const std::string label = m->kind == model::Kind::Attribute
                                    ? vis + " " + m->name + ": " + model::WrittenType(m->field("type")).unqualified()
                                    : vis + " " + signature(m->name, m->params, m->field("returns"));
      rows.push_back({m->id, label, metamodel_.nameOf(m->kind), vis, m->id});
    }
  }
  double title = static_cast<double>(text::characters(e.name));
  const auto bases = bases_.find(e.id);  // a header may name what it inherits from: Console ◁ Channel
  if (bases != bases_.end())
    for (const auto& b : bases->second) title += static_cast<double>(text::characters(b.substr(b.rfind(':') + 1))) * 0.85 + 3;
  double widest = title * 1.6 + 14;
  for (const auto& r : rows) widest = std::max(widest, static_cast<double>(text::characters(r.label)));
  const double w = std::max(kUnitMinW, std::min(kUnitMaxW, widest * kCharWidth + 18));
  layout::Frame f = leaf(e.id, w, kHead + static_cast<double>(rows.size()) * kRow + kUnitPadBottom);
  for (size_t i = 0; i < rows.size(); ++i) {
    node(rows[i].id, "row", e.id, rows[i].label, rows[i].stereotype, rows[i].vis, rows[i].from);
    f.kids.push_back(leaf(rows[i].id, w, kRow));
    f.at.push_back({0, kHead + static_cast<double>(i) * kRow});
  }
  node(e.id, "unit", parent, e.name, stereotypeOf(e), visibility(e), e.id);
  return f;
}

layout::Frame Projection::tree(layout::Frame base, std::map<std::string, layout::Frame>& units,
                               const std::map<std::string, std::vector<std::string>>& below) {
  const auto kids = below.find(base.id);
  if (kids == below.end()) return base;
  layout::Frame block;
  block.id = base.id + "#tree";
  block.arrangement = layout::Frame::Arrangement::Tree;
  block.gap = kUnitGap;
  block.drop = kTreeGap;
  block.kids.push_back(std::move(base));
  for (const auto& k : kids->second) block.kids.push_back(tree(std::move(units.at(k)), units, below));
  return block;
}

std::map<std::string, std::string> Projection::taxonomy(const std::vector<layout::Frame>& units) const {
  std::set<std::string> here;
  for (const layout::Frame& u : units) here.insert(u.id);
  std::map<std::string, std::string> base;
  for (const layout::Frame& u : units) {
    const auto it = bases_.find(u.id);
    if (it == bases_.end()) continue;
    for (const auto& b : it->second) {
      if (b == u.id || !here.count(b)) continue;
      std::string up = b;  // never under one of its own descendants
      while (base.count(up) && up != u.id) up = base.at(up);
      if (up == u.id) continue;
      base[u.id] = b;
      break;
    }
  }
  return base;
}

// Each inheritance is read once, as the boxes it joins; then the first box in the order written that waits for none
// of the boxes left is taken, and when every box left waits for another (a cycle) the first one left is, so the
// order written is kept.
void Projection::order(std::vector<layout::Frame>& boxes) const {
  std::unordered_map<std::string, std::vector<size_t>> at;  // the boxes by their identity
  for (size_t i = 0; i < boxes.size(); ++i) at[boxes[i].id].push_back(i);
  const auto holders = [&](const std::string& id) {  // the boxes an identity is inside: its prefixes that end before a ::
    std::vector<size_t> out;
    for (size_t p = id.find("::"); p != std::string::npos; p = id.find("::", p + 1)) {
      const auto it = at.find(id.substr(0, p));
      if (it != at.end()) out.insert(out.end(), it->second.begin(), it->second.end());
    }
    return out;
  };
  std::vector<std::set<size_t>> waits(boxes.size());  // the boxes each box inherits from
  for (const auto& pair : bases_) {
    const std::vector<size_t> heirs = holders(pair.first);
    if (heirs.empty()) continue;
    for (const auto& b : pair.second)
      for (const size_t j : holders(b))
        for (const size_t i : heirs)
          if (i != j) waits[i].insert(j);
  }
  std::vector<size_t> left(boxes.size());  // how many of the boxes not yet taken each box waits for
  std::vector<std::vector<size_t>> waitedBy(boxes.size());
  std::set<size_t> untaken, ready;
  for (size_t i = 0; i < boxes.size(); ++i) {
    left[i] = waits[i].size();
    for (const size_t j : waits[i]) waitedBy[j].push_back(i);
    untaken.insert(i);
    if (left[i] == 0) ready.insert(i);
  }
  std::vector<layout::Frame> done;
  done.reserve(boxes.size());
  while (!untaken.empty()) {
    const size_t pick = ready.empty() ? *untaken.begin() : *ready.begin();
    untaken.erase(pick);
    ready.erase(pick);
    done.push_back(std::move(boxes[pick]));
    for (const size_t i : waitedBy[pick])
      if (--left[i] == 0 && untaken.count(i)) ready.insert(i);
  }
  boxes = std::move(done);
}

const std::vector<const model::Entity*>& Projection::children(const std::string& scope) const {
  static const std::vector<const model::Entity*> none;
  const auto it = byScope_.find(scope);
  return it == byScope_.end() ? none : it->second;
}

Node& Projection::node(const std::string& id, const std::string& kind, const std::string& parent, const std::string& label,
                       const std::string& stereotype, const std::string& vis, const std::string& linkedFrom) {
  Node& n = nodes_[id];
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

std::string Projection::stereotypeOf(const model::Entity& e) const {
  if (e.has("dataclass")) return "dataclass";
  const std::string derived = e.field("stereotypes");
  if (derived.find("abstract") != std::string::npos) return "abstract";
  return metamodel_.nameOf(e.kind);
}

layout::Frame Projection::leaf(const std::string& id, double w, double h) {
  layout::Frame f;
  f.id = id;
  f.w = w;
  f.h = h;
  return f;
}

std::string Projection::visibility(const model::Entity& e) { return e.field("visibility") == "-" ? "-" : "+"; }

std::string Projection::signature(const std::string& name, const std::vector<model::Param>& params, const std::string& returns) {
  std::string out = name + "(";
  for (size_t i = 0; i < params.size(); ++i) out += (i ? ", " : "") + params[i].name;
  out += ")";
  if (!returns.empty())
    out += " → " + model::WrittenType(returns).unqualified();  // the return as written: void, None, whatever the code says
  return out;
}

}  // namespace scene
}  // namespace ds
