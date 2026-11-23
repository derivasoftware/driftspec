#include "ds/render/Layout.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <set>

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

/// An edge projected onto the kids of a frame: which kid is drawn above which.
struct Projected {
  int upper;
  int lower;
  size_t edge;  ///< index in the document, or SIZE_MAX for a pseudo edge
};

/// A waypoint an edge passes through between two layers: the x of a dummy
/// node and the vertical band of its layer.
struct Waypoint {
  int x;
  int top;
  int bottom;
  int gapBelow;  ///< the height of the gap under its layer: where the next jog lives
};

/// The gap under a node's layer: the band its outgoing jogs live in.
struct Band {
  int top;
  int bottom;
};

/// The layered placement of one connected component: nodes get x/y relative to the component.
struct Component {
  std::vector<int> nodes;  // kid indices; -1 = pseudo source, -2 = pseudo sink
  std::vector<Projected> edges;
  std::map<size_t, std::vector<Waypoint>> ways;  // by edge index, upper to lower
  std::map<int, Band> bands;                     // by kid: the gap under its layer
  int w = 0;
  int h = 0;
};

constexpr int kSource = -1;
constexpr int kSink = -2;

class Engine {
 public:
  Engine(const IrDocument& doc, const Metrics& m, int maxWidth) : doc_(doc), M(m), maxWidth_(maxWidth) {
    const size_t n = doc.nodes.size();
    rel_.assign(n, Rect{});
    parent_.assign(n, -1);
    children_.assign(n, {});
    for (size_t i = 0; i < n; ++i) index_[doc.nodes[i].id] = static_cast<int>(i);
    for (size_t i = 0; i < n; ++i) {
      const auto p = doc.nodes[i].parent.empty() ? index_.end() : index_.find(doc.nodes[i].parent);
      if (p != index_.end() && p->second != static_cast<int>(i)) {
        parent_[i] = p->second;
        children_[p->second].push_back(static_cast<int>(i));
      } else {
        roots_.push_back(static_cast<int>(i));
      }
    }
  }

  LayoutResult run() {
    frame(-1, maxWidth_);
    LayoutResult out;
    std::vector<Rect> abs(doc_.nodes.size());
    std::map<int, Point> origin;  // content origin of every frame, -1 = the page
    origin[-1] = {0, 0};
    std::function<void(int, int, int)> place = [&](int node, int ox, int oy) {
      abs[node] = rel_[node];
      abs[node].x += ox;
      abs[node].y += oy;
      const int cx = abs[node].x + M.CONTAINER_PAD;
      const int cy = abs[node].y + M.CONTAINER_LABEL_H + M.CONTAINER_PAD / 2;
      origin[node] = {cx, cy};
      for (int c : children_[node]) place(c, cx, cy);
    };
    for (int r : roots_) place(r, 0, 0);
    for (size_t i = 0; i < doc_.nodes.size(); ++i) out.placed.push_back({&doc_.nodes[i], abs[i]});
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
    route(out, abs);
    return out;
  }

 private:
  // ---- frames: every container lays out its own children ----------------------
  int ancestorIn(int node, int frameParent) const {
    while (node >= 0 && parent_[node] != frameParent) node = parent_[node];
    return node;
  }

  void frame(int parent, int available) {
    const std::vector<int>& kids = parent < 0 ? roots_ : children_[parent];
    const int inner = available > 0 ? std::max(M.CHAR_W * 8, available - 2 * M.CONTAINER_PAD) : 0;
    for (int k : kids) {
      if (doc_.nodes[k].kind == "container") {
        frame(k, inner);
      } else {
        rel_[k] = Layout(M).sizeOf(doc_.nodes[k]);
      }
    }
    if (kids.empty()) {
      if (parent >= 0) {
        rel_[parent].w = static_cast<int>(doc_.nodes[parent].label.size()) * M.CHAR_W + 2 * M.CONTAINER_PAD + 20;
        rel_[parent].h = M.CONTAINER_LABEL_H + 2 * M.CONTAINER_PAD;
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
    for (size_t i = 0; i < doc_.edges.size(); ++i) {
      const IrEdge& e = doc_.edges[i];
      const auto f = index_.find(e.from);
      const auto t = index_.find(e.to);
      if (f == index_.end() || t == index_.end()) continue;
      const int a = ancestorIn(f->second, parent);
      const int b = ancestorIn(t->second, parent);
      if (a >= 0 && b >= 0 && a != b) {
        // bases above subtypes; wholes above parts; a note above what it annotates
        projected.push_back(isHierarchy(e.kind) ? Projected{b, a, i} : Projected{a, b, i});
        comp[find(local[a])] = find(local[b]);
      } else if (a >= 0 && b < 0) {
        // the other end lives outside this frame: keep this end near the border it will be drawn towards
        (isHierarchy(e.kind) ? nearTop : nearBottom).insert(a);
      } else if (b >= 0 && a < 0) {
        (isHierarchy(e.kind) ? nearBottom : nearTop).insert(b);
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
          c.edges.push_back({kSource, k, std::numeric_limits<size_t>::max()});
          source = true;
        }
        if (nearBottom.count(k) && !nearTop.count(k)) {
          c.edges.push_back({k, kSink, std::numeric_limits<size_t>::max()});
          sink = true;
        }
      }
      if (source) c.nodes.push_back(kSource);
      if (sink) c.nodes.push_back(kSink);
      layered(c, available);
    }

    // pack the components in rows towards a 1.6 aspect
    long area = 0;
    int widest = 0;
    for (int r : order) {
      area += static_cast<long>(components[r].w) * components[r].h;
      widest = std::max(widest, components[r].w);
    }
    int target = std::max(widest, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(area) * M.ASPECT))));
    if (available > 0) target = std::max(widest, available);  // a known screen is filled before growing downwards
    int x = 0, y = 0, rowH = 0, contentW = 0;
    for (int r : order) {
      Component& c = components[r];
      if (x > 0 && x + c.w > target) {
        x = 0;
        y += rowH + M.GAP_Y;
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
      x += c.w + M.GAP_X;
      rowH = std::max(rowH, c.h);
      contentW = std::max(contentW, x - M.GAP_X);
    }
    const int contentH = y + rowH;
    if (parent >= 0) {
      const int labelW = static_cast<int>(doc_.nodes[parent].label.size()) * M.CHAR_W + 20;
      rel_[parent].w = std::max(contentW, labelW) + 2 * M.CONTAINER_PAD;
      rel_[parent].h = contentH + M.CONTAINER_LABEL_H + M.CONTAINER_PAD / 2 + M.CONTAINER_PAD;
    }
  }

  // ---- one component: layers, dummies for long edges, order, coordinates ----------
  void layered(Component& c, int available) {
    const int real = static_cast<int>(c.nodes.size());
    std::map<int, int> local;
    for (int i = 0; i < real; ++i) local[c.nodes[i]] = i;
    // sizes: pseudo nodes take no room
    std::vector<int> widths, heights;
    for (int k : c.nodes) {
      widths.push_back(k < 0 ? 0 : rel_[k].w);
      heights.push_back(k < 0 ? 0 : rel_[k].h);
    }
    // break cycles: an edge that closes a cycle in a DFS over model order is reversed
    std::vector<std::set<int>> adj(real);
    for (const auto& e : c.edges) adj[local[e.upper]].insert(local[e.lower]);
    std::vector<int> state(real, 0);
    std::set<std::pair<int, int>> dag;
    std::function<void(int)> dfs = [&](int v) {
      state[v] = 1;
      for (int w : adj[v]) {
        if (w == v) continue;
        if (state[w] == 1) {
          dag.insert({w, v});
        } else {
          dag.insert({v, w});
          if (state[w] == 0) dfs(w);
        }
      }
      state[v] = 2;
    };
    for (int v = 0; v < real; ++v) {
      if (state[v] == 0) dfs(v);
    }
    // layers by longest path from the sources
    std::vector<std::vector<int>> succ(real), pred(real);
    for (const auto& e : dag) {
      succ[e.first].push_back(e.second);
      pred[e.second].push_back(e.first);
    }
    std::vector<int> layer(real, 0), indeg(real, 0);
    for (int v = 0; v < real; ++v) indeg[v] = static_cast<int>(pred[v].size());
    std::vector<int> queue;
    for (int v = 0; v < real; ++v) {
      if (indeg[v] == 0) queue.push_back(v);
    }
    for (size_t head = 0; head < queue.size(); ++head) {
      const int v = queue[head];
      for (int w : succ[v]) {
        layer[w] = std::max(layer[w], layer[v] + 1);
        if (--indeg[w] == 0) queue.push_back(w);
      }
    }
    int depth = 0;
    for (int v = 0; v < real; ++v) depth = std::max(depth, layer[v]);
    // long edges get a dummy node in every layer they cross, so they are ordered and placed like nodes
    std::vector<std::vector<int>> succA(real), predA(real);
    std::map<std::pair<int, int>, std::vector<int>> chain;  // (upper, lower) → dummies, top to bottom
    int total = real;
    for (const auto& e : dag) {
      int prev = e.first;
      std::vector<int> dummies;
      for (int l = layer[e.first] + 1; l < layer[e.second]; ++l) {
        const int d = total++;
        succA.push_back({});
        predA.push_back({});
        layer.push_back(l);
        widths.push_back(M.CHAR_W);  // a channel one character wide
        heights.push_back(0);
        succA[prev].push_back(d);
        predA[d].push_back(prev);
        dummies.push_back(d);
        prev = d;
      }
      succA[prev].push_back(e.second);
      predA[e.second].push_back(prev);
      chain[e] = dummies;
    }
    std::vector<std::vector<int>> rows(depth + 1);
    for (int v = 0; v < total; ++v) rows[layer[v]].push_back(v);  // model order first, dummies after
    // crossing reduction: barycenter sweeps, stable so the model order breaks ties
    std::vector<double> pos(total);
    auto positions = [&]() {
      for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) pos[row[i]] = static_cast<double>(i);
      }
    };
    auto sweep = [&](const std::vector<std::vector<int>>& neighbours, bool down) {
      positions();
      for (int l = down ? 1 : depth - 1; down ? l <= depth : l >= 0; l += down ? 1 : -1) {
        std::vector<int>& row = rows[l];
        std::vector<std::pair<double, int>> keyed;
        for (int v : row) {
          double bary = pos[v];
          if (!neighbours[v].empty()) {
            bary = 0;
            for (int u : neighbours[v]) bary += pos[u];
            bary /= static_cast<double>(neighbours[v].size());
          }
          keyed.push_back({bary, v});
        }
        std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (size_t i = 0; i < row.size(); ++i) row[i] = keyed[i].second;
        positions();
      }
    };
    for (int it = 0; it < 4; ++it) {
      sweep(predA, true);
      sweep(succA, false);
    }
    // y: each layer as tall as its tallest real node; a layer of pseudo nodes takes no room.
    // The gap below a layer holds one lane per edge crossing it (succA counts the chains), so
    // every horizontal jog has a row of its own (M.LANE apart) instead of piling on the middle.
    std::vector<int> rowY(depth + 1), rowH(depth + 1, 0), gapBelow(depth + 1, M.GAP_Y);
    for (int l = 0; l < depth; ++l) {
      int crossing = 0;
      for (int v : rows[l]) crossing += static_cast<int>(succA[v].size());
      gapBelow[l] = std::max(M.GAP_Y, M.LANE * (crossing + 1) + M.LANE_PAD);
    }
    for (int l = 0; l <= depth; ++l) {
      for (int v : rows[l]) rowH[l] = std::max(rowH[l], heights[v]);
      rowY[l] = l == 0 ? 0 : rowY[l - 1] + rowH[l - 1] + (rowH[l - 1] > 0 ? gapBelow[l - 1] : 0);
    }
    // x: sequential, then pulled under the barycenter of the neighbours without overlapping
    std::vector<int> x(total);
    auto gapAfter = [&](int v) { return widths[v] > 0 ? widths[v] + M.GAP_X : 0; };
    for (int l = 0; l <= depth; ++l) {
      int cursor = 0;
      for (int v : rows[l]) {
        x[v] = cursor;
        cursor += gapAfter(v);
      }
    }
    auto centre = [&](int v) { return x[v] + widths[v] / 2; };
    auto align = [&](const std::vector<std::vector<int>>& neighbours, bool down) {
      for (int l = down ? 1 : depth - 1; down ? l <= depth : l >= 0; l += down ? 1 : -1) {
        const std::vector<int>& row = rows[l];
        std::vector<int> wanted(row.size());
        for (size_t i = 0; i < row.size(); ++i) {
          const int v = row[i];
          if (neighbours[v].empty()) {
            wanted[i] = x[v];
            continue;
          }
          long sum = 0;
          for (int u : neighbours[v]) sum += centre(u);
          wanted[i] = static_cast<int>(sum / static_cast<long>(neighbours[v].size())) - widths[v] / 2;
        }
        for (size_t i = 0; i < row.size(); ++i) {
          const int v = row[i];
          const int minX = i == 0 ? std::numeric_limits<int>::min() : x[row[i - 1]] + gapAfter(row[i - 1]);
          x[v] = std::max(wanted[i], minX);
        }
        for (size_t i = row.size(); i-- > 0;) {
          const int v = row[i];
          const int maxX = i + 1 == row.size() ? std::numeric_limits<int>::max() : x[row[i + 1]] - gapAfter(v);
          const int minX = i == 0 ? std::numeric_limits<int>::min() : x[row[i - 1]] + gapAfter(row[i - 1]);
          x[v] = std::max(std::min(wanted[i], maxX), minX);
        }
      }
    };
    for (int it = 0; it < 3; ++it) {
      align(predA, true);
      align(succA, false);
    }
    int minX = std::numeric_limits<int>::max();
    for (int v = 0; v < total; ++v) {
      if (widths[v] > 0) minX = std::min(minX, x[v]);
    }
    if (minX == std::numeric_limits<int>::max()) minX = 0;
    for (int v = 0; v < total; ++v) x[v] -= minX;
    // a layer wider than the screen wraps onto further lines, in its order
    std::vector<int> lineOf(total, 0), lineTop(total, 0), lineHeight(total, 0);
    for (int l = 0; l <= depth; ++l) {
      int extent = 0;
      for (int v : rows[l]) extent = std::max(extent, x[v] + widths[v]);
      std::vector<std::vector<int>> lines;
      if (available > 0 && extent > available) {
        // real nodes wrap; a dummy (a channel one character wide) stays on the first line past
        // its end rather than starting a line below, where its vertical would cut through a node
        int cursor = 0;
        lines.push_back({});
        std::vector<int> channels;
        int channelRoom = 0;  // the first line keeps room for the channels at its end
        for (int v : rows[l]) {
          if (v >= real) {
            channels.push_back(v);
            channelRoom += gapAfter(v);
          }
        }
        const int room = std::max(widths[rows[l].front()], available - channelRoom);
        for (int v : rows[l]) {
          if (v >= real) continue;
          if (cursor > 0 && cursor + widths[v] > room) {
            lines.push_back({});
            cursor = 0;
          }
          x[v] = cursor;
          lines.back().push_back(v);
          cursor += gapAfter(v);
        }
        int right = 0;
        for (int v : lines[0]) right = std::max(right, x[v] + gapAfter(v));
        for (int d : channels) {
          x[d] = right;
          lines[0].push_back(d);
          right += gapAfter(d);
        }
      } else {
        lines.push_back(rows[l]);
      }
      int top = 0;
      std::vector<int> tops, heightsOfLines;
      for (size_t k = 0; k < lines.size(); ++k) {
        int h = 0;
        for (int v : lines[k]) h = std::max(h, heights[v]);
        tops.push_back(top);
        heightsOfLines.push_back(h);
        for (int v : lines[k]) {
          lineOf[v] = static_cast<int>(k);
          lineTop[v] = top;
          lineHeight[v] = h;
        }
        top += h + (k + 1 < lines.size() ? M.GAP_Y / 2 : 0);
      }
      if (lines.size() > 1) {
        // the layer grew: every layer below moves down
        const int grown = top - rowH[l];
        rowH[l] = top;
        for (int m = l + 1; m <= depth; ++m) rowY[m] += grown;
      }
    }
    c.w = 0;
    c.h = 0;
    for (int v = 0; v < real; ++v) {
      if (c.nodes[v] < 0) continue;
      Rect& r = rel_[c.nodes[v]];
      r.x = x[v];
      r.y = rowY[layer[v]] + lineTop[v];
      c.w = std::max(c.w, r.x + r.w);
      c.h = std::max(c.h, r.y + r.h);
      const int l = layer[v];
      c.bands[c.nodes[v]] = {rowY[l] + rowH[l], rowY[l] + rowH[l] + (l < depth ? gapBelow[l] : M.GAP_Y)};
    }
    for (int d = real; d < total; ++d) c.w = std::max(c.w, x[d] + widths[d]);  // the channels count too
    minX = 0;
    for (const auto& e : c.edges) {
      if (e.edge == std::numeric_limits<size_t>::max()) continue;
      std::pair<int, int> key{local[e.upper], local[e.lower]};
      auto it = chain.find(key);
      if (it == chain.end()) {
        it = chain.find({key.second, key.first});  // the cycle breaker reversed it
        if (it == chain.end()) continue;
      }
      std::vector<Waypoint> way;
      for (int d : it->second) {
        const int l = layer[d];
        way.push_back(
            {x[d] - minX + widths[d] / 2, rowY[l] + lineTop[d], rowY[l] + lineTop[d] + lineHeight[d], l < depth ? gapBelow[l] : M.GAP_Y});
      }
      if (it->first.first != key.first) std::reverse(way.begin(), way.end());
      if (!way.empty()) c.ways[e.edge] = way;
    }
  }

  // ---- routes: orthogonal, through the dummies, anchors spread along the sides -----
  struct End {
    size_t edge;
    bool from;
    int side;   // 0 top, 1 bottom, 2 left, 3 right
    int other;  // the coordinate of the opposite end, to sort anchors
  };

  void route(LayoutResult& out, const std::vector<Rect>& abs) {
    std::map<int, std::vector<End>> ends;
    std::vector<std::pair<int, int>> pairs(doc_.edges.size(), {-1, -1});
    std::vector<bool> verticalEdge(doc_.edges.size(), true);
    for (size_t i = 0; i < doc_.edges.size(); ++i) {
      const auto f = index_.find(doc_.edges[i].from);
      const auto t = index_.find(doc_.edges[i].to);
      if (f == index_.end() || t == index_.end() || f->second == t->second) continue;
      const Rect& a = abs[f->second];
      const Rect& b = abs[t->second];
      pairs[i] = {f->second, t->second};
      const auto way = ways_.find(i);
      const int firstX = way == ways_.end() ? b.x + b.w / 2 : way->second.front().x;
      const int lastX = way == ways_.end() ? a.x + a.w / 2 : way->second.back().x;
      if (b.y + b.h <= a.y) {
        ends[f->second].push_back({i, true, 0, way == ways_.end() ? firstX : way->second.back().x});
        ends[t->second].push_back({i, false, 1, way == ways_.end() ? lastX : way->second.front().x});
      } else if (a.y + a.h <= b.y) {
        ends[f->second].push_back({i, true, 1, firstX});
        ends[t->second].push_back({i, false, 0, lastX});
      } else if (b.x + b.w / 2 >= a.x + a.w / 2) {
        verticalEdge[i] = false;
        ends[f->second].push_back({i, true, 3, b.y + b.h / 2});
        ends[t->second].push_back({i, false, 2, a.y + a.h / 2});
      } else {
        verticalEdge[i] = false;
        ends[f->second].push_back({i, true, 2, b.y + b.h / 2});
        ends[t->second].push_back({i, false, 3, a.y + a.h / 2});
      }
    }
    // a relation of a box with itself: out by its right side and back, one nested inside the other
    std::map<int, int> loops;
    for (size_t i = 0; i < doc_.edges.size(); ++i) {
      const auto f = index_.find(doc_.edges[i].from);
      const auto t = index_.find(doc_.edges[i].to);
      if (f == index_.end() || t == index_.end() || f->second != t->second) continue;
      const Rect& r = abs[f->second];
      const int nth = loops[f->second]++;
      const int reach = (nth + 1) * 3 * M.LANE;
      const int top = r.y + r.h / 4;
      const int bottom = r.y + r.h - r.h / 4;
      out.routes[i].points = {{r.x + r.w, top}, {r.x + r.w + reach, top}, {r.x + r.w + reach, bottom}, {r.x + r.w, bottom}};
    }
    std::map<std::pair<size_t, bool>, Point> anchors;
    for (auto& pair : ends) {
      const Rect& r = abs[pair.first];
      for (int side = 0; side < 4; ++side) {
        std::vector<End*> here;
        for (End& e : pair.second) {
          if (e.side == side) here.push_back(&e);
        }
        std::stable_sort(here.begin(), here.end(), [](const End* a, const End* b) { return a->other < b->other; });
        const int span = side < 2 ? r.w : r.h;
        const int count = static_cast<int>(here.size());
        for (int k = 0; k < count; ++k) {
          const int along = span * (k + 1) / (count + 1);
          Point p;
          if (side == 0) p = {r.x + along, r.y};
          if (side == 1) p = {r.x + along, r.y + r.h};
          if (side == 2) p = {r.x, r.y + along};
          if (side == 3) p = {r.x + r.w, r.y + along};
          anchors[{here[k]->edge, here[k]->from}] = p;
        }
      }
    }
    // every horizontal jog is a lane request in a band (the gap between two layers): the
    // band is keyed by its bottom edge (the top of the lower node), and the jogs of one band
    // get distinct rows, ordered by where they come from, so they neither pile up nor cross
    struct Jog {
      size_t edge;
      size_t index;  // which jog of the edge's polyline
      int fromX, toX;
      int bandTop, bandBottom;
    };
    std::vector<Jog> jogs;
    std::map<size_t, std::vector<Point>> drafts;  // polylines with the jog rows still unset (y = -1 marks a jog)
    for (size_t i = 0; i < doc_.edges.size(); ++i) {
      if (pairs[i].first < 0) continue;
      const Point a = anchors[{i, true}];
      const Point b = anchors[{i, false}];
      std::vector<Point> pts;
      const auto way = ways_.find(i);
      auto jog = [&](int fromX, int toX, int bandTop, int bandBottom) {
        if (fromX == toX) return;
        jogs.push_back({i, pts.size(), fromX, toX, bandTop, bandBottom});
        pts.push_back({fromX, -1});
        pts.push_back({toX, -1});
      };
      // the first jog lives in the gap under the upper node's layer; later ones under each dummy's layer
      const int upperNode = a.y <= b.y ? pairs[i].first : pairs[i].second;
      const auto band = bands_.find(upperNode);
      if (way != ways_.end() && verticalEdge[i]) {
        const bool downwards = a.y <= b.y;
        const Point top = downwards ? a : b;
        const Point bottom = downwards ? b : a;
        pts.push_back(top);
        int prevX = top.x;
        int bandTop = band != bands_.end() ? band->second.top : top.y;
        int bandBottom = band != bands_.end() ? band->second.bottom : way->second.front().top;
        for (const Waypoint& w : way->second) {
          jog(prevX, w.x, bandTop, bandBottom);
          prevX = w.x;
          bandTop = w.bottom;
          bandBottom = w.bottom + w.gapBelow;
        }
        jog(prevX, bottom.x, bandTop, bandBottom);
        pts.push_back(bottom);
        if (!downwards) {
          std::reverse(pts.begin(), pts.end());
          for (Jog& j : jogs) {
            if (j.edge == i) j.index = pts.size() - 2 - j.index;
          }
        }
      } else if (verticalEdge[i]) {
        // no waypoints: an edge across containers, or between adjacent layers; its jog
        // goes right under the upper end, never spread over whatever lies between
        const bool downwards = a.y <= b.y;
        const Point top = downwards ? a : b;
        const Point bottom = downwards ? b : a;
        pts.push_back(top);
        if (band != bands_.end() && band->second.bottom <= bottom.y) {
          jog(top.x, bottom.x, band->second.top, band->second.bottom);
        } else {
          jog(top.x, bottom.x, top.y, std::min(bottom.y, top.y + 2 * M.GAP_Y));
        }
        pts.push_back(bottom);
        if (!downwards) {
          std::reverse(pts.begin(), pts.end());
          for (Jog& j : jogs) {
            if (j.edge == i) j.index = pts.size() - 2 - j.index;
          }
        }
      } else {
        if (a.y == b.y) {
          pts = {a, b};
        } else {
          const int midX = (a.x + b.x) / 2;
          pts = {a, {midX, a.y}, {midX, b.y}, b};
        }
      }
      drafts[i] = pts;
    }
    // lanes: group the jogs by band, order them by their x, spread them over the band
    std::map<std::pair<int, int>, std::vector<Jog*>> bands;
    for (Jog& j : jogs) bands[{j.bandTop, j.bandBottom}].push_back(&j);
    for (auto& band : bands) {
      std::vector<Jog*>& list = band.second;
      std::stable_sort(list.begin(), list.end(),
                       [](const Jog* p, const Jog* q) { return std::min(p->fromX, p->toX) < std::min(q->fromX, q->toX); });
      const int top = band.first.first, bottom = band.first.second;
      const int n = static_cast<int>(list.size());
      for (int k = 0; k < n; ++k) {
        const int y = top + (bottom - top) * (k + 1) / (n + 1);
        std::vector<Point>& pts = drafts[list[k]->edge];
        pts[list[k]->index].y = y;
        pts[list[k]->index + 1].y = y;
      }
    }
    for (auto& d : drafts) {
      Route r;
      r.points = d.second;
      out.routes[d.first] = r;
    }
  }

  const IrDocument& doc_;
  const Metrics M;
  const int maxWidth_;
  std::map<std::string, int> index_;
  std::vector<Rect> rel_;
  std::vector<int> parent_;
  std::vector<std::vector<int>> children_;
  std::vector<int> roots_;
  std::map<size_t, std::vector<Waypoint>> ways_;
  std::map<size_t, int> wayFrame_;
  std::map<int, Band> bands_;  ///< by node, relative to its frame's content origin
  std::map<int, int> bandFrame_;
};

}  // namespace

Rect Layout::sizeOf(const IrNode& node) const {
  const Metrics& M = m_;
  Rect r;
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

LayoutResult Layout::place(const IrDocument& doc, int maxWidth) const { return Engine(doc, m_, maxWidth).run(); }

}  // namespace render
}  // namespace ds
