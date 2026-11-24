#include "ds/layout/PackingEngine.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ds {
namespace layout {

Placement PackingEngine::place(const Graph& graph) const {
  Placement out;
  if (graph.root == nullptr) return out;
  const Packed packed = size(*graph.root, graph, out);
  land(*graph.root, packed, 0, 0, out.boxes);
  return out;
}

Packed PackingEngine::size(const Frame& frame, const Graph& graph, Placement& out) const {
  switch (frame.arrangement) {
    case Frame::Arrangement::Shelves:
      return shelves(frame, graph, out);
    case Frame::Arrangement::Tree:
      return tree(frame, graph, out);
    case Frame::Arrangement::Fixed:
    case Frame::Arrangement::Layered:  // layering is the layered engine's: packing keeps the frame's size
      break;
  }
  return fixed(frame, graph, out);
}

Packed PackingEngine::fixed(const Frame& frame, const Graph& graph, Placement& out) const {
  Packed p;
  p.w = frame.w;
  p.h = frame.h;
  p.at = frame.at;
  for (const Frame& k : frame.kids) p.kids.push_back(size(k, graph, out));
  return p;
}

Packed PackingEngine::shelves(const Frame& frame, const Graph& graph, Placement& out) const {
  Packed p;
  for (const Frame& k : frame.kids) p.kids.push_back(size(k, graph, out));
  const auto used = shelf(p.kids, frame.gap, width(frame.id, target(p.kids, frame.atLeast, frame.factor), graph), p.at);
  if (!graph.following) out.widths[frame.id] = used.first;
  for (auto& a : p.at) a = {a.first + frame.left, a.second + frame.top};
  double y = frame.portTop;
  for (const Frame& port : frame.ports) {  // on the left border: half outside, half inside
    Packed s = size(port, graph, out);
    p.at.push_back({-s.w / 2, y});
    y += s.h + frame.portGap;
    p.kids.push_back(std::move(s));
  }
  p.w = used.first + frame.left + frame.right;
  p.h = std::max(used.second + frame.top + frame.bottom, y + frame.bottom);
  return p;
}

Packed PackingEngine::tree(const Frame& frame, const Graph& graph, Placement& out) const {
  Packed p;
  if (frame.kids.empty()) return p;
  Packed base = size(frame.kids.front(), graph, out);
  std::vector<Packed> row;
  std::vector<double> xs;
  double x = 0, rowH = 0;
  for (size_t i = 1; i < frame.kids.size(); ++i) {
    row.push_back(size(frame.kids[i], graph, out));
    xs.push_back(x);
    x += row.back().w + frame.gap;
    rowH = std::max(rowH, row.back().h);
  }
  const double rowW = x - frame.gap, baseW = base.w, baseH = base.h;
  p.w = std::max(rowW, baseW);
  p.at.push_back({(p.w - baseW) / 2, 0});
  p.kids.push_back(std::move(base));
  for (size_t i = 0; i < row.size(); ++i) {
    p.at.push_back({xs[i] + (p.w - rowW) / 2, baseH + frame.drop});
    p.kids.push_back(std::move(row[i]));
  }
  p.h = baseH + frame.drop + rowH;
  return p;
}

double PackingEngine::width(const std::string& id, double computed, const Graph& graph) {
  if (!graph.following) return computed;
  const auto kept = graph.widths.find(id);
  return kept == graph.widths.end() ? computed : kept->second;
}

std::pair<double, double> PackingEngine::shelf(const std::vector<Packed>& kids, double gap, double target,
                                               std::vector<std::pair<double, double>>& at) {
  double x = 0, y = 0, rowH = 0, width = 0;
  for (const Packed& s : kids) {
    if (x > 0 && x + s.w > target) {
      x = 0;
      y += rowH + gap;
      rowH = 0;
    }
    at.push_back({x, y});
    x += s.w + gap;
    rowH = std::max(rowH, s.h);
    width = std::max(width, x - gap);
  }
  return {width, y + rowH};
}

double PackingEngine::target(const std::vector<Packed>& kids, double atLeast, double factor) {
  double area = 0, widest = atLeast;
  for (const Packed& s : kids) {
    area += s.w * s.h;
    widest = std::max(widest, s.w);
  }
  return std::max(widest, std::sqrt(area) * factor);
}

void PackingEngine::land(const Frame& frame, const Packed& packed, double x, double y, std::vector<Box>& out) {
  out.push_back({x, y, packed.w, packed.h});
  const size_t shelved = frame.kids.size();
  for (size_t i = 0; i < packed.kids.size(); ++i) {
    const Frame& k = i < shelved ? frame.kids[i] : frame.ports[i - shelved];
    land(k, packed.kids[i], x + packed.at[i].first, y + packed.at[i].second, out);
  }
}

}  // namespace layout
}  // namespace ds
