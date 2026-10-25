#ifndef DRIFTSPEC_RENDER_LAYOUT_HPP
#define DRIFTSPEC_RENDER_LAYOUT_HPP

#include <map>
#include <string>
#include <vector>

#include "driftspec/render/Ir.hpp"

namespace driftspec {
namespace render {

/// Fixed metrics: sizing never measures fonts. `svg()` is plantuml-render's
/// pixel table; `text()` is the same table in character cells, so the same
/// layout lands on a terminal grid.
struct Metrics {
  int CHAR_W = 8;
  int LINE_H = 18;
  int PAD_X = 10;
  int PAD_Y = 10;
  int SECTION_GAP = 4;
  int BADGE_W = 20;
  int DOT_W = 12;
  int NOTE_FOLD = 9;
  int GAP_X = 40;
  int GAP_Y = 56;
  int CONTAINER_PAD = 18;
  int CONTAINER_LABEL_H = 22;
  double ASPECT = 1.6;  ///< width/height the packing aims at, in this table's units

  static Metrics svg() { return Metrics{}; }
  static Metrics text() {
    Metrics m;
    m.CHAR_W = 1;
    m.LINE_H = 1;
    m.PAD_X = 2;  // border and one space
    m.PAD_Y = 1;  // border
    m.SECTION_GAP = 1;  // the separator row
    m.BADGE_W = 2;
    m.DOT_W = 2;
    m.NOTE_FOLD = 1;
    m.GAP_X = 4;
    m.GAP_Y = 3;  // stub, channel, head
    m.CONTAINER_PAD = 2;
    m.CONTAINER_LABEL_H = 1;
    m.ASPECT = 0.8;  // a terminal cell is about twice as tall as wide: 1.6 on screen is 0.8 in cells
    return m;
  }
};

struct Point {
  int x = 0;
  int y = 0;
};

struct Rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

/// A node with its absolute rectangle.
struct Placed {
  const IrNode* node = nullptr;
  Rect rect;
};

/// The polyline of an edge, orthogonal, from the IR `from` to the IR `to`.
struct Route {
  std::vector<Point> points;
};

struct LayoutResult {
  std::vector<Placed> placed;   ///< every node of the document that could be placed
  std::map<size_t, Route> routes;  ///< by edge index; absent when an end was not placed
};

/// The size a node takes, from its text alone.
Rect sizeOf(const IrNode& node, const Metrics& m);

/// Layered layout (Sugiyama) per container: bases above subtypes, wholes above
/// parts, components packed towards a readable aspect, orthogonal routes.
/// With `maxWidth` (in the metrics' units; 0 = unbounded) a layer that does not
/// fit wraps onto further lines and components pack to that width, so the
/// drawing adapts to the screen instead of asking the reader to pan.
/// Deterministic: same document, metrics and width, same coordinates.
LayoutResult layout(const IrDocument& doc, const Metrics& m = Metrics::svg(), int maxWidth = 0);

}  // namespace render
}  // namespace driftspec

#endif  // DRIFTSPEC_RENDER_LAYOUT_HPP
