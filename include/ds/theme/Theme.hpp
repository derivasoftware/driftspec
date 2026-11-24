#ifndef DRIFTSPEC_THEME_THEME_HPP
#define DRIFTSPEC_THEME_THEME_HPP

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ds {
namespace theme {

/// How text measures on one sink (DS-393): the advance of a character and the height of a line, in that sink's units.
/// A plain value: who sizes a box from its text takes it, and geometry never sees the font (DS-392).
struct Measures {
  int advance = 0;
  int lineHeight = 0;
};

/// Where a drawing goes: a page (pixels) or a terminal (cells).
enum class Sink { svg, text };

/// The font every drawing is written in: its family, its size, and how it measures on each sink.
struct Font {
  std::string family;
  int sizePx = 0;
  Measures svg;
  Measures text;
};

/// One token of the theme: its name in the --pr-* contract (without --pr-), its light value and its dark one ("" when
/// a dark page uses the light value).
struct Token {
  std::string name;
  std::string light;
  std::string dark;
};

/// What a terminal colours text as: the roles of the text drawing.
enum class Role {
  muted,
  name,
  type,
  publicMember,
  privateMember,
  protectedMember,
  badge,
  note,
  container,
  edge,
  added,
  removed,
  renamed,
  changed,
  against
};

/// A role's terminal colour (an SGR parameter, "" for the terminal's own) and the token whose light value a host that
/// paints cells uses for it ("" for none).
struct Terminal {
  Role role;
  std::string sgr;
  std::string token;
};

/// The one source of style (DS-393): every colour, the font and its measures, light and dark, the terminal's palette and
/// the variables of each page. Made once where the engine starts and handed by const reference to what draws; no colour is written
/// anywhere else. The --pr-* names are the theming contract shared with plantuml-render.
class Theme {
 public:
  Theme();

  const std::vector<Token>& tokens() const { return tokens_; }
  /// The tokens of a page, in its own vocabulary (DS-393): viewer, viewer-night, live, playground, design-page. A page the
  /// theme lacks is a logic error.
  const std::vector<Token>& page(const std::string& name) const;
  /// A token by its name; a name the theme lacks is a logic error.
  const Token& token(const std::string& name) const;
  const Font& font() const { return font_; }
  Measures measures(Sink sink) const { return sink == Sink::svg ? font_.svg : font_.text; }
  const std::vector<Terminal>& terminal() const { return terminal_; }
  /// A role's SGR parameter.
  const std::string& sgr(Role role) const;
  /// The colour a host that paints cells gives each SGR parameter: the light value of the first role that names it.
  const std::map<std::string, std::uint32_t>& cellColours() const { return cellColours_; }

 private:
  std::vector<Token> tokens_;
  std::map<std::string, std::vector<Token>> pages_;
  Font font_;
  std::vector<Terminal> terminal_;
  std::map<std::string, std::uint32_t> cellColours_;
};

}  // namespace theme
}  // namespace ds

#endif  // DRIFTSPEC_THEME_THEME_HPP
