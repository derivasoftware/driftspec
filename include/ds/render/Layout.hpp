#ifndef DRIFTSPEC_RENDER_LAYOUT_HPP
#define DRIFTSPEC_RENDER_LAYOUT_HPP

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "ds/render/Ir.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace render {

/// Fixed metrics: sizing never measures fonts. `svg()` is plantuml-render's
/// pixel table; `text()` is the same table in character cells, so the same
/// layout lands on a terminal grid.
struct Metrics {
  int CHAR_W = 0;   ///< the advance of a character: the theme's measures, never written here (DS-393)
  int LINE_H = 0;   ///< the height of a line: the theme's measures
  int SMALL_W = 0;  ///< the advance of a character of small text: the theme's measures (DS-464)
  int PAD_X = 10;
  int PAD_Y = 10;
  int SECTION_GAP = 4;
  int BADGE_W = 20;
  int DOT_W = 12;
  int NOTE_FOLD = 9;
  int GAP_X = 40;
  int GAP_Y = 56;
  int LANE = 12;      ///< one row per edge crossing a gap between layers
  int LANE_PAD = 20;  ///< air above the first lane and below the last
  int CONTAINER_PAD = 18;
  int CONTAINER_LABEL_H = 22;
  double ASPECT = 1.6;  ///< width/height the packing aims at, in this table's units

  /// The pixel table, its text measures read from the theme (DS-393, DS-392).
  static Metrics svg(const theme::Measures& text) {
    Metrics m;
    m.CHAR_W = text.advance;
    m.LINE_H = text.lineHeight;
    m.SMALL_W = text.smallAdvance;
    return m;
  }
  /// The same table in character cells, its text measures read from the theme.
  static Metrics text(const theme::Measures& text) {
    Metrics m;
    m.CHAR_W = text.advance;
    m.LINE_H = text.lineHeight;
    m.SMALL_W = text.smallAdvance;
    m.PAD_X = 2;        // border and one space
    m.PAD_Y = 1;        // border
    m.SECTION_GAP = 1;  // the separator row
    m.BADGE_W = 2;
    m.DOT_W = 2;
    m.NOTE_FOLD = 1;
    m.GAP_X = 4;
    m.GAP_Y = 3;  // stub, channel, head
    m.LANE = 1;   // a row per lane
    m.LANE_PAD = 2;
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
  bool curve = false;          ///< a smooth curve through the points, not a polyline (DS-443)
  std::optional<Point> label;  ///< where its label goes, when the router chose it
};

struct LayoutResult {
  std::vector<Placed> placed;      ///< every node of the document that could be placed
  std::map<size_t, Route> routes;  ///< by edge index; absent when an end was not placed
};

/// The layout of a document in one table of metrics (DS-185).
class Layout {
 public:
  explicit Layout(const Metrics& m) : m_(m) {}

  /// The size a node takes, from its text alone.
  Rect sizeOf(const IrNode& node) const;

  /// Layered layout (Sugiyama) per container: bases above subtypes, wholes above
  /// parts, components packed towards a readable aspect, orthogonal routes.
  /// With `maxWidth` (in the metrics' units; 0 = unbounded) a layer that does not
  /// fit wraps onto further lines and components pack to that width, so the
  /// drawing adapts to the screen instead of asking the reader to pan.
  /// Deterministic: same document, metrics and width, same coordinates.
  /// With `asItsKind` the edges run as the document's kind is drawn, the router RouterFactory gives that kind: a
  /// state diagram's as curves (DS-443). Otherwise orthogonal, as a terminal draws every kind.
  LayoutResult place(const IrDocument& doc, int maxWidth = 0, bool asItsKind = false) const;

 private:
  Metrics m_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_LAYOUT_HPP
