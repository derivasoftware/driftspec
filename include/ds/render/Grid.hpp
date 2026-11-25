#ifndef DRIFTSPEC_RENDER_GRID_HPP
#define DRIFTSPEC_RENDER_GRID_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "ds/render/Cell.hpp"
#include "ds/render/Layout.hpp"

namespace ds {
namespace render {

/// The character grid the terminal drawing fills, cell by cell (DS-371): each cell a glyph, its colour and what an
/// edge drew there, written out as text or as cells.
class Grid {
 public:
  Grid(int minX, int minY, int w, int h) : minX_(minX), minY_(minY), w_(w), h_(h), cells_(sized(w, h)) {}

  /// The cell at (x, y), none outside the grid.
  Cell* at(int x, int y);

  /// One glyph at (x, y), in `color`.
  void put(int x, int y, std::string_view glyph, const std::string& color = "");

  /// A text from (x, y) on, one code point per cell.
  void text(int x, int y, const std::string& s, const std::string& color = "");

  /// An edge segment cell: joins with what another edge already drew.
  void line(int x, int y, bool horizontal, bool dashed, const std::string& color);

  /// base64 of u32 little-endian triplets per cell: codepoint, foreground, background
  std::string encode(const std::map<std::string, std::uint32_t>& rgb) const;

  int columns() const { return w_; }
  int rows() const { return h_; }

  /// Another grid copied onto this one, its first row at `row`: the parts of a drawing, one under the other.
  void paste(const Grid& other, int row);

  /// The grid as lines of text, coloured with ANSI codes when asked.
  std::string dump(bool color) const;

  /// The border of `r`, its corners rounded or square (a box rounded, a note or a container square).
  void frame(const Rect& r, bool rounded, const std::string& color);
  /// Blanks `r`: what a box covers is not drawn through.
  void clear(const Rect& r);

 private:
  /// A drawing too large to hold as text is refused with what it would take, instead of exhausting the machine
  /// (DS-350): some 128 million characters, about 0.9 GB of cells, is the most it holds.
  static std::size_t sized(int w, int h);
  static constexpr std::size_t kMaxCells = std::size_t{128} * 1024 * 1024;
  /// The index of `color` in the palette, added the first time it is asked.
  std::uint8_t paint(const std::string& color);
  void setGlyph(Cell& c, std::string_view glyph);
  std::string_view glyphOf(const Cell& c) const;

  int minX_;
  int minY_;
  int w_;
  int h_;
  std::vector<Cell> cells_;
  std::vector<std::string> palette_{""};  ///< the colours the cells name, "" first: the default
  std::vector<std::string> long_;         ///< the glyphs longer than one code point
};

// what is called for every cell, kept where every drawing that fills the grid inlines it

inline Cell* Grid::at(int x, int y) {
  x -= minX_;
  y -= minY_;
  if (x < 0 || y < 0 || x >= w_ || y >= h_) return nullptr;
  return &cells_[static_cast<size_t>(y) * w_ + x];
}

inline void Grid::put(int x, int y, std::string_view glyph, const std::string& color) {
  if (Cell* c = at(x, y)) {
    setGlyph(*c, glyph);
    c->colour = paint(color);
    c->line = 0;
  }
}

inline void Grid::text(int x, int y, const std::string& s, const std::string& color) {
  int col = x;
  for (size_t i = 0; i < s.size();) {
    // one UTF-8 code point per cell
    size_t len = 1;
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c >= 0xF0)
      len = 4;
    else if (c >= 0xE0)
      len = 3;
    else if (c >= 0xC0)
      len = 2;
    put(col++, y, std::string_view(s).substr(i, len), color);
    i += len;
  }
}

inline void Grid::line(int x, int y, bool horizontal, bool dashed, const std::string& color) {
  Cell* c = at(x, y);
  if (!c) return;
  const int bit = horizontal ? 1 : 2;
  if (c->line && (c->line | bit) == 3 && c->line != bit) {
    setGlyph(*c, "\xE2\x94\xBC");  // ┼
  } else if (c->line == bit) {
    return;
  } else {
    setGlyph(*c, horizontal ? (dashed ? "\xE2\x94\x84" : "\xE2\x94\x80") : (dashed ? "\xE2\x94\x86" : "\xE2\x94\x82"));  // ┄ ─ ┆ │
  }
  c->line = static_cast<std::uint8_t>(c->line | bit);
  c->colour = paint(color);
}

inline std::uint8_t Grid::paint(const std::string& color) {
  for (std::size_t i = 0; i < palette_.size(); ++i)
    if (palette_[i] == color) return static_cast<std::uint8_t>(i);
  if (palette_.size() >= Cell::kLong) throw std::length_error("a text drawing of more than 254 colours");
  palette_.push_back(color);
  return static_cast<std::uint8_t>(palette_.size() - 1);
}

inline void Grid::setGlyph(Cell& c, std::string_view glyph) {
  if (glyph.size() <= c.glyph.size()) {
    std::copy(glyph.begin(), glyph.end(), c.glyph.begin());
    c.size = static_cast<std::uint8_t>(glyph.size());
    return;
  }
  const auto index = static_cast<std::uint32_t>(long_.size());
  long_.emplace_back(glyph);
  std::memcpy(c.glyph.data(), &index, sizeof index);
  c.size = Cell::kLong;
}

inline std::string_view Grid::glyphOf(const Cell& c) const {
  if (c.size != Cell::kLong) return std::string_view(c.glyph.data(), c.size);
  std::uint32_t index = 0;
  std::memcpy(&index, c.glyph.data(), sizeof index);
  return long_[index];
}

inline void Grid::frame(const Rect& r, bool rounded, const std::string& color) {
  const char* tl = rounded ? "\xE2\x95\xAD" : "\xE2\x94\x8C";  // ╭ ┌
  const char* tr = rounded ? "\xE2\x95\xAE" : "\xE2\x94\x90";  // ╮ ┐
  const char* bl = rounded ? "\xE2\x95\xB0" : "\xE2\x94\x94";  // ╰ └
  const char* br = rounded ? "\xE2\x95\xAF" : "\xE2\x94\x98";  // ╯ ┘
  for (int x = r.x + 1; x < r.x + r.w - 1; ++x) {
    put(x, r.y, "\xE2\x94\x80", color);
    put(x, r.y + r.h - 1, "\xE2\x94\x80", color);
  }
  for (int y = r.y + 1; y < r.y + r.h - 1; ++y) {
    put(r.x, y, "\xE2\x94\x82", color);
    put(r.x + r.w - 1, y, "\xE2\x94\x82", color);
  }
  put(r.x, r.y, tl, color);
  put(r.x + r.w - 1, r.y, tr, color);
  put(r.x, r.y + r.h - 1, bl, color);
  put(r.x + r.w - 1, r.y + r.h - 1, br, color);
}

inline void Grid::clear(const Rect& r) {
  for (int y = r.y; y < r.y + r.h; ++y) {
    for (int x = r.x; x < r.x + r.w; ++x) put(x, y, " ");
  }
}

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_GRID_HPP
