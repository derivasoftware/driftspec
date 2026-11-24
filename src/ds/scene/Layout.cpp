#include "ds/scene/Layout.hpp"

#include <algorithm>
#include <functional>
#include <map>

#include "ds/scene/Packer.hpp"
#include "ds/scene/Projection.hpp"

namespace ds {
namespace scene {

Layout::Layout(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}

// The model is projected and packed twice, once per variant; the full one's order is kept, the lean one fills its
// shelves no wider than the full one came out, so hiding private members never widens a box, only shortens it, and
// every node that has a lean place gets it.
SceneDocument Layout::place() const {
  std::map<std::string, double> widths;
  const Projection full(model_, metamodel_, false);
  const std::map<std::string, Box> fullBoxes = Packer(&widths, false).pack(full.root());
  const Projection lean(model_, metamodel_, true);
  const std::map<std::string, Box> leanBoxes = Packer(&widths, true).pack(lean.root());
  const auto at = [](const std::map<std::string, Box>& boxes, const std::string& id) {
    const auto it = boxes.find(id);
    return it == boxes.end() ? Box{} : it->second;
  };
  SceneDocument doc;
  // nodes in drawing order: parents before children, as the full projection reads them
  std::function<void(const Frame&)> walk = [&](const Frame& f) {
    const auto n = full.nodes().find(f.id);
    if (n != full.nodes().end()) {  // a tree of a taxonomy groups units without being drawn
      Node node = n->second;
      node.full = at(fullBoxes, f.id);
      if (lean.nodes().count(f.id)) {
        node.lean = true;
        node.leanBox = at(leanBoxes, f.id);
      }
      doc.nodes.push_back(node);
    }
    for (const Frame& k : f.kids) walk(k);
    for (const Frame& p : f.ports) walk(p);
  };
  for (const Frame& t : full.root().kids) walk(t);
  doc.world = bounds(doc.nodes, false);
  doc.leanWorld = bounds(doc.nodes, true);
  return doc;
}

Box Layout::bounds(const std::vector<Node>& nodes, bool lean) {
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

}  // namespace scene
}  // namespace ds
