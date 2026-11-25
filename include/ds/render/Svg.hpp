#ifndef DRIFTSPEC_RENDER_SVG_HPP
#define DRIFTSPEC_RENDER_SVG_HPP

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/render/Figure.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace render {

struct EdgeMarkup;

/// Emits the SVG of a laid-out document: the same stable ids, CSS classes,
/// tokens and markers as plantuml-render's engine, so a host page styles
/// both the same way and a drawing from either engine is comparable.
class Svg {
 public:
  /// Draws in `theme`: its measures size the text and its stylesheets go into every drawing (DS-393).
  explicit Svg(const theme::Theme& theme);

  /// `maxWidth` in pixels (0 = unbounded): the drawing wraps to it.
  std::string render(const IrDocument& doc, int maxWidth = 0) const;
  /// A figure composed for SVG (DS-400): drawn as it is placed, nothing placed here.
  std::string draw(const Figure& figure) const;

 private:
  /// FNV-1a of the title in base 36, as shared.ts idPrefix: several SVGs share one page.
  static std::string idPrefix(const std::string& title);
  /// The arrowheads and diamonds every drawing defines once, their ids under `px`.
  static std::string markers(const std::string& px);
  /// Split on top-level commas: generics and parentheses stay whole.
  static std::vector<std::string> splitParams(const std::string& text);
  /// A type as the engine colours it.
  static std::string typed(const std::string& type);
  /// One member line of `ds render` (`+ abstract run(model: Model) -> list<Finding>`,
  /// `- items: list<T> = 0`) tokenised into the engine's syntax-coloured tspans.
  static std::pair<std::string, std::string> memberMarkup(const std::string& line);
  /// A number written in whole pixels.
  static std::string num(int v) { return std::to_string(v); }
  /// A position computed in doubles, drawn in whole pixels.
  static std::string num(double v) { return num(static_cast<int>(v)); }
  /// A box, a container or a note where the layout placed it, in the pixel metrics.
  std::string emitNode(const Placed& p, const std::string& px) const;
  /// An edge along its route: its path, and its label apart.
  static EdgeMarkup emitEdge(const IrEdge& e, const Route& route, const std::string& px);
  /// A node's or an edge's named references as data-ref-<key> attributes.
  static std::string dataAttrs(const std::map<std::string, std::string>& refs);
  /// A sequence: lifelines as lanes, messages as rows, and its interaction in the SVG. The drawing carries its own
  /// usability: a tooltip on everything, links by identity (a lifeline to its class, a message to the behaviour it
  /// expands to), focus on hover, and expansions that fold on click with every row below moving up. A small script is
  /// embedded so this works wherever the SVG is inline or opened on its own; as an <img> it is simply static.
  std::string sequenceSvg(const IrDocument& doc) const;
  /// A drawing that is a table (DS-251): the headings across and down, and each cell drawn by its kind.
  std::string tableSvg(const IrDocument& doc) const;
  /// Several drawings in one (DS-245): each keeps its own drawing and is placed beside the last, or under it when
  /// the width asked for does not hold them side by side. Each is a nested svg that says which part it is.
  static std::string besideEachOther(const std::vector<std::pair<std::string, std::string>>& drawings, int maxWidth);

  static const std::map<std::string, std::string> kMarkerByKind;
  static const char* const kSeqScript;

  const Metrics metrics_;
  const std::string style_;          ///< the stylesheet every drawing carries, written from the theme
  const std::string sequenceStyle_;  ///< what a sequence adds to it
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SVG_HPP
