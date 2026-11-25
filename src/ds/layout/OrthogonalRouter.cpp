#include "ds/layout/OrthogonalRouter.hpp"

#include <algorithm>
#include <utility>

#include "ds/layout/Point.hpp"

namespace ds {
namespace layout {

OrthogonalRouter::OrthogonalRouter(const Spacing& spacing) : spacing_(spacing) {}

std::map<std::size_t, Route> OrthogonalRouter::route(const Routing& routing) const {
  const Spacing& S = spacing_;
  std::map<size_t, std::vector<Point>> out;
  struct End {
    size_t edge;
    bool from;
    int side;   // 0 top, 1 bottom, 2 left, 3 right
    int other;  // the coordinate of the opposite end, to sort anchors
  };

  std::map<int, std::vector<End>> ends;
  std::vector<std::pair<int, int>> pairs(routing.ends.size(), {-1, -1});
  std::vector<bool> verticalEdge(routing.ends.size(), true);
  for (size_t i = 0; i < routing.ends.size(); ++i) {
    const int f = routing.ends[i].first;
    const int t = routing.ends[i].second;
    if (f < 0 || t < 0 || f == t) continue;
    const Rect& a = routing.boxes[f];
    const Rect& b = routing.boxes[t];
    pairs[i] = {f, t};
    const auto way = routing.ways.find(i);
    const int firstX = way == routing.ways.end() ? b.x + b.w / 2 : way->second.front().x;
    const int lastX = way == routing.ways.end() ? a.x + a.w / 2 : way->second.back().x;
    if (b.y + b.h <= a.y) {
      ends[f].push_back({i, true, 0, way == routing.ways.end() ? firstX : way->second.back().x});
      ends[t].push_back({i, false, 1, way == routing.ways.end() ? lastX : way->second.front().x});
    } else if (a.y + a.h <= b.y) {
      ends[f].push_back({i, true, 1, firstX});
      ends[t].push_back({i, false, 0, lastX});
    } else if (b.x + b.w / 2 >= a.x + a.w / 2) {
      verticalEdge[i] = false;
      ends[f].push_back({i, true, 3, b.y + b.h / 2});
      ends[t].push_back({i, false, 2, a.y + a.h / 2});
    } else {
      verticalEdge[i] = false;
      ends[f].push_back({i, true, 2, b.y + b.h / 2});
      ends[t].push_back({i, false, 3, a.y + a.h / 2});
    }
  }
  // a relation of a box with itself: out by its right side and back, one nested inside the other
  std::map<int, int> loops;
  for (size_t i = 0; i < routing.ends.size(); ++i) {
    const int f = routing.ends[i].first;
    const int t = routing.ends[i].second;
    if (f < 0 || t < 0 || f != t) continue;
    const Rect& r = routing.boxes[f];
    const int nth = loops[f]++;
    const int reach = (nth + 1) * 3 * S.lane;
    const int top = r.y + r.h / 4;
    const int bottom = r.y + r.h - r.h / 4;
    out[i] = {{r.x + r.w, top}, {r.x + r.w + reach, top}, {r.x + r.w + reach, bottom}, {r.x + r.w, bottom}};
  }
  std::map<std::pair<size_t, bool>, Point> anchors;
  for (auto& pair : ends) {
    const Rect& r = routing.boxes[pair.first];
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
  for (size_t i = 0; i < routing.ends.size(); ++i) {
    if (pairs[i].first < 0) continue;
    const Point a = anchors[{i, true}];
    const Point b = anchors[{i, false}];
    std::vector<Point> pts;
    const auto way = routing.ways.find(i);
    auto jog = [&](int fromX, int toX, int bandTop, int bandBottom) {
      if (fromX == toX) return;
      jogs.push_back({i, pts.size(), fromX, toX, bandTop, bandBottom});
      pts.push_back({fromX, -1});
      pts.push_back({toX, -1});
    };
    // the first jog lives in the gap under the upper node's layer; later ones under each dummy's layer
    const int upperNode = a.y <= b.y ? pairs[i].first : pairs[i].second;
    const auto band = routing.bands.find(upperNode);
    if (way != routing.ways.end() && verticalEdge[i]) {
      const bool downwards = a.y <= b.y;
      const Point top = downwards ? a : b;
      const Point bottom = downwards ? b : a;
      pts.push_back(top);
      int prevX = top.x;
      int bandTop = band != routing.bands.end() ? band->second.top : top.y;
      int bandBottom = band != routing.bands.end() ? band->second.bottom : way->second.front().top;
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
      if (band != routing.bands.end() && band->second.bottom <= bottom.y) {
        jog(top.x, bottom.x, band->second.top, band->second.bottom);
      } else {
        jog(top.x, bottom.x, top.y, std::min(bottom.y, top.y + 2 * S.gapY));
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
  for (auto& d : drafts) out[d.first] = d.second;
  std::map<std::size_t, Route> routes;  // the corners in whole units, carried exactly by doubles
  for (const auto& r : out) {
    Route& to = routes[r.first];
    for (const Point& p : r.second) to.points.push_back({static_cast<double>(p.x), static_cast<double>(p.y)});
  }
  return routes;
}

}  // namespace layout
}  // namespace ds
