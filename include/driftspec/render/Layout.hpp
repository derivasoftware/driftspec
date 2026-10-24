#ifndef DRIFTSPEC_RENDER_LAYOUT_HPP
#define DRIFTSPEC_RENDER_LAYOUT_HPP

#include <map>
#include <string>
#include <vector>

#include "driftspec/render/Ir.hpp"

namespace driftspec {
namespace render {

/// Fixed metrics, the same as plantuml-render's: sizing never measures fonts.
struct Metrics {
  static constexpr int CHAR_W = 8;
  static constexpr int LINE_H = 18;
  static constexpr int PAD = 10;
  static constexpr int SECTION_GAP = 4;
  static constexpr int BADGE_W = 20;
  static constexpr int DOT_W = 12;
  static constexpr int NOTE_FOLD = 9;
  static constexpr int GAP_X = 40;
  static constexpr int GAP_Y = 56;
  static constexpr int CONTAINER_PAD = 18;
  static constexpr int CONTAINER_LABEL_H = 22;
  static constexpr int LABEL_CHAR_W = 7;
  static constexpr int LABEL_H = 14;
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
Rect sizeOf(const IrNode& node);

/// Layered layout (Sugiyama) per container: bases above subtypes, wholes above
/// parts, components packed towards a readable aspect, orthogonal routes.
/// Deterministic: same document, same coordinates.
LayoutResult layout(const IrDocument& doc);

}  // namespace render
}  // namespace driftspec

#endif  // DRIFTSPEC_RENDER_LAYOUT_HPP
