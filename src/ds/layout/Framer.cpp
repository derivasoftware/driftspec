#include "ds/layout/Framer.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <set>
#include <utility>

#include "ds/layout/Layerer.hpp"
#include "ds/layout/Layering.hpp"
#include "ds/layout/Orderer.hpp"
#include "ds/layout/Point.hpp"
#include "ds/layout/Positioner.hpp"
#include "ds/layout/Routing.hpp"

namespace ds {
namespace layout {

Framer::Framer(const Graph& graph, const Spacing& s) : edges_(graph.edges), S(s), maxWidth_(static_cast<int>(graph.maxWidth)) {
  // every frame numbered in the order the graph is read: the root is 0, a frame is followed by its kids and then
  // its ports. The engine lays out the frames under the root; a port, which it does not place, keeps its number
  std::function<std::size_t(const Frame&)> size = [&](const Frame& f) {
    std::size_t n = 1;
    for (const Frame& k : f.kids) n += size(k);
    for (const Frame& p : f.ports) n += size(p);
    return n;
  };
  std::size_t next = 1;
  std::function<void(const Frame&, int)> add = [&](const Frame& f, int parent) {
    const int i = static_cast<int>(nodes_.size());
    nodes_.push_back(&f);
    number_.push_back(next++);
    parent_.push_back(parent);
    children_.push_back({});
    if (parent >= 0) {
      children_[parent].push_back(i);
    } else {
      roots_.push_back(i);
    }
    for (const Frame& k : f.kids) add(k, i);
    for (const Frame& p : f.ports) next += size(p);
  };
  if (graph.root != nullptr) {
    for (const Frame& k : graph.root->kids) add(k, -1);
    for (const Frame& p : graph.root->ports) next += size(p);
  }
  count_ = graph.root != nullptr ? next : 0;
  nodeOf_.assign(count_, -1);
  for (size_t i = 0; i < nodes_.size(); ++i) nodeOf_[number_[i]] = static_cast<int>(i);
  rel_.assign(nodes_.size(), Rect{});
}

Placement Framer::run(const IRouter& router) {
  lanes_ = router.lanes();
  frame(-1, maxWidth_);
  Placement out;
  std::vector<Rect> abs(nodes_.size());
  std::map<int, Point> origin;  // content origin of every frame, -1 = the page
  origin[-1] = {0, 0};
  std::function<void(int, int, int)> place = [&](int node, int ox, int oy) {
    abs[node] = rel_[node];
    abs[node].x += ox;
    abs[node].y += oy;
    const int cx = abs[node].x + S.containerPad;
    const int cy = abs[node].y + S.containerLabelH + S.containerPad / 2;
    origin[node] = {cx, cy};
    for (int c : children_[node]) place(c, cx, cy);
  };
  for (int r : roots_) place(r, 0, 0);
  out.boxes.assign(count_, Box{});
  for (size_t i = 0; i < nodes_.size(); ++i)
    out.boxes[number_[i]] =
        Box{static_cast<double>(abs[i].x), static_cast<double>(abs[i].y), static_cast<double>(abs[i].w), static_cast<double>(abs[i].h)};
  for (auto& w : ways_) {
    const Point o = origin[wayFrame_[w.first]];
    for (Waypoint& p : w.second) {
      p.x += o.x;
      p.top += o.y;
      p.bottom += o.y;
    }
  }
  for (auto& b : bands_) {
    const Point o = origin[bandFrame_[b.first]];
    b.second.top += o.y;
    b.second.bottom += o.y;
  }
  Routing routing;
  routing.boxes = std::move(abs);
  routing.ends.reserve(edges_.size());
  for (const Edge& e : edges_) routing.ends.push_back({nodeAt(e.from), nodeAt(e.to)});
  routing.ways = std::move(ways_);
  routing.bands = std::move(bands_);
  out.routes = router.route(routing);
  return out;
}

int Framer::nodeAt(std::size_t number) const { return number < nodeOf_.size() ? nodeOf_[number] : -1; }

int Framer::ancestorIn(int node, int frameParent) const {
  while (node >= 0 && parent_[node] != frameParent) node = parent_[node];
  return node;
}

void Framer::frame(int parent, int available) {
  const std::vector<int>& kids = parent < 0 ? roots_ : children_[parent];
  const int inner = available > 0 ? std::max(S.channel * 8, available - 2 * S.containerPad) : 0;
  for (int k : kids) {
    if (nodes_[k]->arrangement == Frame::Arrangement::Layered) {
      frame(k, inner);
    } else {
      rel_[k] = Rect{0, 0, static_cast<int>(nodes_[k]->w), static_cast<int>(nodes_[k]->h)};
    }
  }
  if (kids.empty()) {
    if (parent >= 0) {
      rel_[parent].w = static_cast<int>(nodes_[parent]->label) + 2 * S.containerPad + 20;
      rel_[parent].h = S.containerLabelH + 2 * S.containerPad;
    }
    return;
  }
  std::map<int, int> local;
  for (size_t i = 0; i < kids.size(); ++i) local[kids[i]] = static_cast<int>(i);
  std::vector<int> comp(kids.size());
  for (size_t i = 0; i < kids.size(); ++i) comp[i] = static_cast<int>(i);
  std::function<int(int)> find = [&](int i) { return comp[i] == i ? i : comp[i] = find(comp[i]); };
  std::vector<Projected> projected;
  std::set<int> nearTop, nearBottom;
  for (size_t i = 0; i < edges_.size(); ++i) {
    const Edge& e = edges_[i];
    const int f = nodeAt(e.from);
    const int t = nodeAt(e.to);
    if (f < 0 || t < 0) continue;
    const int a = ancestorIn(f, parent);
    const int b = ancestorIn(t, parent);
    if (a >= 0 && b >= 0 && a != b) {
      // bases above subtypes; wholes above parts; a note above what it annotates
      projected.push_back(e.upward ? Projected{b, a, i} : Projected{a, b, i});
      comp[find(local[a])] = find(local[b]);
    } else if (a >= 0 && b < 0) {
      // the other end lives outside this frame: keep this end near the border it will be drawn towards
      (e.upward ? nearTop : nearBottom).insert(a);
    } else if (b >= 0 && a < 0) {
      (e.upward ? nearBottom : nearTop).insert(b);
    }
  }
  std::map<int, Component> components;
  std::vector<int> order;
  for (size_t i = 0; i < kids.size(); ++i) {
    const int r = find(static_cast<int>(i));
    if (!components.count(r)) order.push_back(r);
    components[r].nodes.push_back(kids[i]);
  }
  for (const auto& e : projected) components[find(local[e.upper])].edges.push_back(e);
  for (auto& pair : components) {
    Component& c = pair.second;
    bool source = false, sink = false;
    for (int k : c.nodes) {
      if (nearTop.count(k)) {
        c.edges.push_back({Component::source, k, std::numeric_limits<size_t>::max()});
        source = true;
      }
      if (nearBottom.count(k) && !nearTop.count(k)) {
        c.edges.push_back({k, Component::sink, std::numeric_limits<size_t>::max()});
        sink = true;
      }
    }
    if (source) c.nodes.push_back(Component::source);
    if (sink) c.nodes.push_back(Component::sink);
    layered(c, available);
  }

  // pack the components in rows towards a 1.6 aspect
  long area = 0;
  int widest = 0;
  for (int r : order) {
    area += static_cast<long>(components[r].w) * components[r].h;
    widest = std::max(widest, components[r].w);
  }
  int target = std::max(widest, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(area) * S.aspect))));
  if (available > 0) target = std::max(widest, available);  // a known screen is filled before growing downwards
  int x = 0, y = 0, rowH = 0, contentW = 0;
  for (int r : order) {
    Component& c = components[r];
    if (x > 0 && x + c.w > target) {
      x = 0;
      y += rowH + S.gapY;
      rowH = 0;
    }
    for (int k : c.nodes) {
      if (k < 0) continue;
      rel_[k].x += x;
      rel_[k].y += y;
      if (c.bands.count(k)) {
        bands_[k] = {c.bands[k].top + y, c.bands[k].bottom + y};
        bandFrame_[k] = parent;
      }
    }
    for (auto& w : c.ways) {
      for (Waypoint& p : w.second) {
        p.x += x;
        p.top += y;
        p.bottom += y;
      }
      ways_[w.first] = w.second;
      wayFrame_[w.first] = parent;
    }
    x += c.w + S.gapX;
    rowH = std::max(rowH, c.h);
    contentW = std::max(contentW, x - S.gapX);
  }
  const int contentH = y + rowH;
  if (parent >= 0) {
    const int labelW = static_cast<int>(nodes_[parent]->label) + 20;
    rel_[parent].w = std::max(contentW, labelW) + 2 * S.containerPad;
    rel_[parent].h = contentH + S.containerLabelH + S.containerPad / 2 + S.containerPad;
  }
}

void Framer::layered(Component& c, int available) {
  Layering L = Layerer(S).of(c, rel_);
  Orderer::order(L);
  const Positioner positioner(S, lanes_);
  positioner.place(L, available);
  positioner.apply(L, c, rel_);
}

}  // namespace layout
}  // namespace ds
