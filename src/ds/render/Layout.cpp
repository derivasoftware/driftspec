#include "ds/render/Layout.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <set>

#include "ds/layout/EngineFactory.hpp"
#include "ds/layout/Frame.hpp"
#include "ds/layout/Graph.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace render {

namespace {

size_t longest(const std::vector<std::string>& lines) {
  size_t n = 0;
  for (const auto& l : lines) n = std::max(n, l.size());
  return n;
}

bool isHierarchy(const std::string& kind) { return kind == "inheritance" || kind == "realization"; }

}  // namespace

Rect Layout::sizeOf(const IrNode& node) const {
  const Metrics& M = m_;
  Rect r;
  if (node.kind == "initial" || node.kind == "final") {  // a dot, a bullseye: three characters wide and a line high (DS-442)
    r.w = 3 * M.CHAR_W;
    r.h = M.LINE_H;
    return r;
  }
  if (node.kind == "note") {
    const auto lines = text::linesOf(node.label);
    r.w = static_cast<int>(std::max<size_t>(1, longest(lines))) * M.CHAR_W + 2 * M.PAD_X + M.NOTE_FOLD;
    r.h = 2 * M.PAD_Y + static_cast<int>(lines.size()) * M.LINE_H;
    return r;
  }
  int textW = static_cast<int>(node.label.size()) * M.CHAR_W + (node.kind == "box" ? M.BADGE_W : 0);
  textW = std::max(textW, static_cast<int>(node.stereotype.size()) * M.CHAR_W + 16);
  int sectionLines = 0;
  for (size_t i = 0; i < node.sections.size(); ++i) {
    const auto& s = node.sections[i];
    // a row of what a closed box offers carries a badge, wider than the dot of a member's visibility (DS-244)
    const bool offered = i < node.sectionKinds.size() && !holdsMembers(node.sectionKinds[i]);
    for (const auto& line : s) textW = std::max(textW, static_cast<int>(line.size()) * M.CHAR_W + (offered ? M.BADGE_W : M.DOT_W));
    sectionLines += static_cast<int>(s.size());
  }
  const int headerLines = node.stereotype.empty() ? 1 : 2;
  r.w = textW + 2 * M.PAD_X;
  r.h = 2 * M.PAD_Y + headerLines * M.LINE_H + static_cast<int>(node.sections.size()) * M.SECTION_GAP + sectionLines * M.LINE_H;
  return r;
}

// The document as a graph for the layout's layered engine (DS-392): every node a frame under its parent, in the order
// written; a container layered, sized by the engine, the width of its label given; any other node sized from its text.
// Every edge is told only whether it points up: a base above what inherits from it. The places come back as they
// went, in whole units.
LayoutResult Layout::place(const IrDocument& doc, int maxWidth) const {
  const size_t n = doc.nodes.size();
  std::map<std::string, int> index;
  for (size_t i = 0; i < n; ++i) index[doc.nodes[i].id] = static_cast<int>(i);
  std::vector<std::vector<int>> children(n);
  std::vector<int> roots;
  for (size_t i = 0; i < n; ++i) {
    const auto p = doc.nodes[i].parent.empty() ? index.end() : index.find(doc.nodes[i].parent);
    if (p != index.end() && p->second != static_cast<int>(i)) {
      children[p->second].push_back(static_cast<int>(i));
    } else {
      roots.push_back(static_cast<int>(i));
    }
  }
  std::function<layout::Frame(int)> frameOf = [&](int i) {
    const IrNode& node = doc.nodes[i];
    layout::Frame f;
    f.id = node.id;
    if (node.kind == "container") {
      f.arrangement = layout::Frame::Arrangement::Layered;
      f.label = static_cast<double>(static_cast<int>(node.label.size()) * m_.CHAR_W);
    } else {
      const Rect r = sizeOf(node);
      f.w = r.w;
      f.h = r.h;
    }
    for (int c : children[i]) f.kids.push_back(frameOf(c));
    return f;
  };
  layout::Frame root;
  for (int r : roots) root.kids.push_back(frameOf(r));
  layout::Graph graph;
  graph.root = &root;
  graph.maxWidth = maxWidth;
  for (const IrEdge& e : doc.edges) graph.edges.push_back({e.from, e.to, isHierarchy(e.kind)});
  layout::Spacing spacing;
  spacing.channel = m_.CHAR_W;
  spacing.gapX = m_.GAP_X;
  spacing.gapY = m_.GAP_Y;
  spacing.lane = m_.LANE;
  spacing.lanePad = m_.LANE_PAD;
  spacing.containerPad = m_.CONTAINER_PAD;
  spacing.containerLabelH = m_.CONTAINER_LABEL_H;
  spacing.aspect = m_.ASPECT;
  const layout::Placement placed = layout::EngineFactory::layered(spacing)->place(graph);
  LayoutResult out;
  for (const IrNode& node : doc.nodes) {
    Rect r;
    const auto box = placed.boxes.find(node.id);
    if (box != placed.boxes.end())
      r = {static_cast<int>(box->second.x), static_cast<int>(box->second.y), static_cast<int>(box->second.w),
           static_cast<int>(box->second.h)};
    out.placed.push_back({&node, r});
  }
  for (const auto& route : placed.routes) {
    Route& to = out.routes[route.first];
    for (const auto& p : route.second.points) to.points.push_back({static_cast<int>(p.first), static_cast<int>(p.second)});
  }
  return out;
}

}  // namespace render
}  // namespace ds
