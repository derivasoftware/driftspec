#include "ds/scene/Layout.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>

#include "ds/layout/EngineFactory.hpp"
#include "ds/scene/Projection.hpp"

namespace ds {
namespace scene {

Layout::Layout(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}

// The model is projected and packed twice, once per variant; the full one's order is kept, the lean one fills its
// shelves no wider than the full one came out, so hiding private members never widens a box, only shortens it, and
// every node that has a lean place gets it.
SceneDocument Layout::place() const {
  const std::unique_ptr<layout::IEngine> engine = layout::EngineFactory::packing();
  const Projection full(model_, metamodel_, false);
  layout::Graph fullGraph;
  fullGraph.root = &full.root();
  const layout::Placement fullPlaced = engine->place(fullGraph);
  const Projection lean(model_, metamodel_, true);
  layout::Graph leanGraph;
  leanGraph.root = &lean.root();
  leanGraph.widths = fullPlaced.widths;
  leanGraph.following = true;
  const layout::Placement leanPlaced = engine->place(leanGraph);
  const auto at = [](const layout::Placement& placed, const std::string& id) {
    const auto it = placed.boxes.find(id);
    return it == placed.boxes.end() ? layout::Box{} : it->second;
  };
  SceneDocument doc;
  // nodes in drawing order: parents before children, as the full projection reads them
  std::function<void(const layout::Frame&)> walk = [&](const layout::Frame& f) {
    const auto n = full.nodes().find(f.id);
    if (n != full.nodes().end()) {  // a tree of a taxonomy groups units without being drawn
      Node node = n->second;
      node.full = at(fullPlaced, f.id);
      if (lean.nodes().count(f.id)) {
        node.lean = true;
        node.leanBox = at(leanPlaced, f.id);
      }
      doc.nodes.push_back(node);
    }
    for (const layout::Frame& k : f.kids) walk(k);
    for (const layout::Frame& p : f.ports) walk(p);
  };
  for (const layout::Frame& t : full.root().kids) walk(t);
  doc.world = bounds(doc.nodes, false);
  doc.leanWorld = bounds(doc.nodes, true);
  return doc;
}

layout::Box Layout::bounds(const std::vector<Node>& nodes, bool lean) {
  bool any = false;
  double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  for (const auto& n : nodes) {
    if (lean && !n.lean) continue;
    const layout::Box& b = lean ? n.leanBox : n.full;
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
