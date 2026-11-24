#ifndef DRIFTSPEC_RENDER_TEXT_HPP
#define DRIFTSPEC_RENDER_TEXT_HPP

#include <cstdint>
#include <map>
#include <string>

#include "ds/emit/Sink.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace render {

/// Draws a document on a character grid with box-drawing glyphs: the same
/// layout as the SVG, run with character metrics (Metrics::text()), so a
/// box sits in the terminal where it sits on the page.
/// What a change a diff marks is drawn as (DS-419): its mark and its terminal colour, the same for a box, a relation and
/// a message.
struct ChangeMark {
  std::string mark;
  std::string colour;
};

/// What the terminal drawing draws with (DS-393): the metrics in cells and each role's colour, read from the theme once.
struct TextPen {
  Metrics M;
  std::string muted;
  std::string name;
  std::string type;
  std::string publicMember;
  std::string privateMember;
  std::string protectedMember;
  std::string badge;
  std::string note;
  std::string container;
  std::string edge;
  std::string against;
  ChangeMark added;
  ChangeMark removed;
  ChangeMark renamed;
  ChangeMark changed;
  std::map<std::string, std::uint32_t> cellColours;  ///< what a host that paints cells gives each colour

  explicit TextPen(const theme::Theme& t)
      : M(Metrics::text(t.measures(theme::Sink::text))),
        muted(t.sgr(theme::Role::muted)),
        name(t.sgr(theme::Role::name)),
        type(t.sgr(theme::Role::type)),
        publicMember(t.sgr(theme::Role::publicMember)),
        privateMember(t.sgr(theme::Role::privateMember)),
        protectedMember(t.sgr(theme::Role::protectedMember)),
        badge(t.sgr(theme::Role::badge)),
        note(t.sgr(theme::Role::note)),
        container(t.sgr(theme::Role::container)),
        edge(t.sgr(theme::Role::edge)),
        against(t.sgr(theme::Role::against)),
        added{"+", t.sgr(theme::Role::added)},
        removed{"-", t.sgr(theme::Role::removed)},
        renamed{">", t.sgr(theme::Role::renamed)},
        changed{"~", t.sgr(theme::Role::changed)},
        cellColours(t.cellColours()) {}

  /// A change the pen does not name is drawn as changed.
  const ChangeMark& changeMarkOf(const std::string& change) const {
    return change == "added" ? added : change == "removed" ? removed : change == "renamed" ? renamed : changed;
  }
};

class Text {
 public:
  /// Draws in `theme`: its measures in cells and its terminal palette (DS-393).
  explicit Text(const theme::Theme& theme);

  /// With `color`, ANSI colours follow the SVG palette roles (name, type, visibility).
  /// `maxWidth` in columns (0 = unbounded): the drawing wraps to it.
  std::string render(const IrDocument& doc, bool color, int maxWidth = 0) const;
  std::string render(const IrDocument& doc, const LayoutResult& layout, bool color) const;

  /// The same drawing as a cell grid for hosts that paint cells themselves
  /// (a Claude Code mod's Raster, a TUI): JSON {columns, rows, cells, text}
  /// where cells is base64 of little-endian u32 triplets [codepoint, fg, bg]
  /// per cell, row-major, 0x01000000 meaning the terminal's default colour; written as one value where the
  /// sink it is lent stands (DS-397).
  void cells(const IrDocument& doc, int maxWidth, emit::ISink& out_sink) const;

 private:
  const TextPen pen_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_TEXT_HPP
