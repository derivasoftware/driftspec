#include "ds/layout/CurvedRouter.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace ds {
namespace layout {

namespace {

using P = std::pair<double, double>;

P centreOf(const Rect& r) { return {r.x + r.w / 2.0, r.y + r.h / 2.0}; }

/// Where the ray from the centre of `r` towards `to` leaves `r`, in whole units.
P borderTowards(const Rect& r, const P& to) {
  const P c = centreOf(r);
  const double dx = to.first - c.first;
  const double dy = to.second - c.second;
  if (dx == 0 && dy == 0) return {std::round(c.first), std::round(c.second + r.h / 2.0)};
  const double sx = dx == 0 ? 1e18 : (r.w / 2.0) / std::abs(dx);
  const double sy = dy == 0 ? 1e18 : (r.h / 2.0) / std::abs(dy);
  const double s = std::min(sx, sy);
  return {std::round(c.first + dx * s), std::round(c.second + dy * s)};
}

/// Whether `outer` holds all of `inner` and is bigger: a frame around it.
bool encloses(const Rect& outer, const Rect& inner) {
  return outer.x <= inner.x && outer.y <= inner.y && outer.x + outer.w >= inner.x + inner.w && outer.y + outer.h >= inner.y + inner.h &&
         (outer.w > inner.w || outer.h > inner.h);
}

/// How far a label's text rises above the point it is set at: a line of edge label, its baseline a little above the
/// point (DS-454). The text runs from the point to a line above it.
constexpr int kLabelRise = 16;

/// Whether the text of a label set at `p`, a line tall above it, would cross `r` or come within `margin` of it.
bool crosses(const Rect& r, const P& p, int margin) {
  const bool across = p.first > r.x - margin && p.first < r.x + r.w + margin;
  const bool within = p.second - kLabelRise < r.y + r.h + margin && p.second > r.y - margin;
  return across && within;
}

/// The point halfway along a polyline.
P halfway(const std::vector<P>& points) {
  double total = 0;
  for (size_t i = 1; i < points.size(); ++i)
    total += std::hypot(points[i].first - points[i - 1].first, points[i].second - points[i - 1].second);
  double left = total / 2;
  for (size_t i = 1; i < points.size(); ++i) {
    const double len = std::hypot(points[i].first - points[i - 1].first, points[i].second - points[i - 1].second);
    if (len >= left && len > 0) {
      const double t = left / len;
      return {std::round(points[i - 1].first + t * (points[i].first - points[i - 1].first)),
              std::round(points[i - 1].second + t * (points[i].second - points[i - 1].second))};
    }
    left -= len;
  }
  return points.front();
}

}  // namespace

CurvedRouter::CurvedRouter(const Spacing& spacing) : spacing_(spacing) {}

std::map<std::size_t, Route> CurvedRouter::route(const Routing& routing) const {
  const Spacing& S = spacing_;
  const int boxes = static_cast<int>(routing.boxes.size());
  std::map<std::size_t, Route> out;
  // edges between the same two boxes, either way, bow apart; loops of one box nest, one outside the other
  std::map<std::pair<int, int>, std::vector<std::size_t>> between;
  std::map<int, int> loops;
  for (std::size_t i = 0; i < routing.ends.size(); ++i) {
    const auto [f, t] = routing.ends[i];
    if (f < 0 || t < 0 || f >= boxes || t >= boxes || f == t) continue;
    between[{std::min(f, t), std::max(f, t)}].push_back(i);
  }
  std::map<std::size_t, std::pair<int, int>> bow;  // by edge: its place among the edges of its pair, and how many
  for (const auto& b : between)
    for (size_t k = 0; k < b.second.size(); ++k) bow[b.second[k]] = {static_cast<int>(k), static_cast<int>(b.second.size())};

  for (std::size_t i = 0; i < routing.ends.size(); ++i) {
    const auto [f, t] = routing.ends[i];
    if (f < 0 || t < 0 || f >= boxes || t >= boxes) continue;
    Route r;
    r.shape = Route::Shape::curve;
    if (f == t) {  // a loop beside the right side: out a quarter down, back a quarter up, wider for each one before
      const Rect& b = routing.boxes[f];
      const int nth = loops[f]++;
      const double x = b.x + b.w;
      const double reach = 3.0 * S.lane * (nth + 1);
      const double top = b.y + b.h / 4.0, bottom = b.y + 3.0 * b.h / 4.0;
      r.points = {{x, std::round(top)},
                  {x + reach, std::round(top - S.lane / 2.0)},
                  {x + reach, std::round(bottom + S.lane / 2.0)},
                  {x, std::round(bottom)}};
      r.label = P{x + reach + 4, std::round(b.y + b.h / 2.0)};
      out[i] = r;
      continue;
    }
    const Rect& a = routing.boxes[f];
    const Rect& b = routing.boxes[t];
    std::vector<P> through;
    const auto way = routing.ways.find(i);
    if (way != routing.ways.end())
      for (const Waypoint& w : way->second) through.push_back({static_cast<double>(w.x), (w.top + w.bottom) / 2.0});
    if (centreOf(a).second > centreOf(b).second) std::reverse(through.begin(), through.end());  // the waypoints run upper to lower
    const auto [k, n] = bow[i];
    if (through.empty() && n > 1) {  // bow it off the straight line, the pair's edges spread evenly around it
      const P ca = centreOf(f < t ? a : b), cb = centreOf(f < t ? b : a);
      const double dx = cb.first - ca.first, dy = cb.second - ca.second, len = std::max(1.0, std::hypot(dx, dy));
      const double off = (k - (n - 1) / 2.0) * 4.0 * S.lane;  // far enough apart that each label keeps to its own curve
      const P mid = {(ca.first + cb.first) / 2.0, (ca.second + cb.second) / 2.0};
      through.push_back({std::round(mid.first - dy / len * off), std::round(mid.second + dx / len * off)});
    }
    const P start = borderTowards(a, through.empty() ? centreOf(b) : through.front());
    const P end = borderTowards(b, through.empty() ? centreOf(a) : through.back());
    r.points.push_back(start);
    for (const P& p : through) r.points.push_back({std::round(p.first), std::round(p.second)});
    r.points.push_back(end);
    // the label halfway along, moved down and then up a lane at a time until its text crosses no box (DS-454)
    const P mid = halfway(r.points);
    P at = mid;
    for (int step = 1; step <= 8; ++step) {
      bool clear = true;
      for (const Rect& box : routing.boxes)  // a frame around either end is where the edge runs, not in its way
        if (!encloses(box, a) && !encloses(box, b)) clear = clear && !crosses(box, at, 2);
      if (clear) break;
      const int sign = step % 2 ? 1 : -1;
      at = {mid.first, mid.second + sign * ((step + 1) / 2) * S.lane};
    }
    r.label = at;
    out[i] = r;
  }
  return out;
}

}  // namespace layout
}  // namespace ds
