#include "ds/layout/Positioner.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace ds {
namespace layout {

Positioner::Positioner(const Spacing& spacing) : spacing_(spacing) {}

void Positioner::place(Layering& L, int available) const {
  const Spacing& S = spacing_;
  const int real = L.real;
  const auto& widths = L.widths;
  const auto& heights = L.heights;
  const int depth = L.depth;
  const auto& succA = L.succA;
  const auto& predA = L.predA;
  const int total = L.total;
  auto& rows = L.rows;
  // y: each layer as tall as its tallest real node; a layer of pseudo nodes takes no room.
  // The gap below a layer holds one lane per edge crossing it (succA counts the chains), so
  // every horizontal jog has a row of its own (S.lane apart) instead of piling on the middle.
  std::vector<int> rowY(depth + 1), rowH(depth + 1, 0), gapBelow(depth + 1, S.gapY);
  for (int l = 0; l < depth; ++l) {
    int crossing = 0;
    for (int v : rows[l]) crossing += static_cast<int>(succA[v].size());
    gapBelow[l] = std::max(S.gapY, S.lane * (crossing + 1) + S.lanePad);
  }
  for (int l = 0; l <= depth; ++l) {
    for (int v : rows[l]) rowH[l] = std::max(rowH[l], heights[v]);
    rowY[l] = l == 0 ? 0 : rowY[l - 1] + rowH[l - 1] + (rowH[l - 1] > 0 ? gapBelow[l - 1] : 0);
  }
  // x: sequential, then pulled under the barycenter of the neighbours without overlapping
  std::vector<int> x(total);
  auto gapAfter = [&](int v) { return widths[v] > 0 ? widths[v] + S.gapX : 0; };
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
      top += h + (k + 1 < lines.size() ? S.gapY / 2 : 0);
    }
    if (lines.size() > 1) {
      // the layer grew: every layer below moves down
      const int grown = top - rowH[l];
      rowH[l] = top;
      for (int m = l + 1; m <= depth; ++m) rowY[m] += grown;
    }
  }
  // DS-350 part 2: now that every node and channel has its x, a gap keeps a lane only for an edge that turns in it:
  // a segment that leaves or enters a real node (its ends are spread along the side), or joins two channels that
  // are not aligned. A long edge running straight down its channel takes no row, so the drawing is no longer as
  // tall as the sum of its edges' lengths
  for (int l = 0; l < depth; ++l) {
    int turning = 0;
    for (int v : rows[l])
      for (int w : succA[v])
        if (v < real || w < real || centre(v) != centre(w)) ++turning;
    gapBelow[l] = std::max(S.gapY, S.lane * (turning + 1) + S.lanePad);
  }
  for (int l = 1; l <= depth; ++l) rowY[l] = rowY[l - 1] + rowH[l - 1] + (rowH[l - 1] > 0 ? gapBelow[l - 1] : 0);
  L.rowY = std::move(rowY);
  L.rowH = std::move(rowH);
  L.gapBelow = std::move(gapBelow);
  L.x = std::move(x);
  L.minX = minX;
  L.lineOf = std::move(lineOf);
  L.lineTop = std::move(lineTop);
  L.lineHeight = std::move(lineHeight);
}

void Positioner::apply(Layering& L, Component& c, std::vector<Rect>& rel) const {
  const Spacing& S = spacing_;
  const int real = L.real;
  auto& local = L.local;
  const auto& widths = L.widths;
  const auto& layer = L.layer;
  const int depth = L.depth;
  auto& chain = L.chain;
  const int total = L.total;
  const auto& rowY = L.rowY;
  const auto& rowH = L.rowH;
  const auto& gapBelow = L.gapBelow;
  const auto& x = L.x;
  int minX = L.minX;
  const auto& lineTop = L.lineTop;
  const auto& lineHeight = L.lineHeight;
  c.w = 0;
  c.h = 0;
  for (int v = 0; v < real; ++v) {
    if (c.nodes[v] < 0) continue;
    Rect& r = rel[c.nodes[v]];
    r.x = x[v];
    r.y = rowY[layer[v]] + lineTop[v];
    c.w = std::max(c.w, r.x + r.w);
    c.h = std::max(c.h, r.y + r.h);
    const int l = layer[v];
    c.bands[c.nodes[v]] = {rowY[l] + rowH[l], rowY[l] + rowH[l] + (l < depth ? gapBelow[l] : S.gapY)};
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
          {x[d] - minX + widths[d] / 2, rowY[l] + lineTop[d], rowY[l] + lineTop[d] + lineHeight[d], l < depth ? gapBelow[l] : S.gapY});
    }
    if (it->first.first != key.first) std::reverse(way.begin(), way.end());
    if (!way.empty()) c.ways[e.edge] = way;
  }
}

}  // namespace layout
}  // namespace ds
