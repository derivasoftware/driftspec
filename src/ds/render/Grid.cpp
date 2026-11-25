#include "ds/render/Grid.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ds {
namespace render {

std::string Grid::encode(const std::map<std::string, std::uint32_t>& rgb) const {
  std::string bytes;
  bytes.reserve(cells_.size() * 12);
  auto put32 = [&](uint32_t v) {
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
  };
  // each colour of the palette told once, not looked up again for every cell that has it
  std::vector<uint32_t> foreground;
  foreground.reserve(palette_.size());
  for (const std::string& colour : palette_) {
    const auto found = rgb.find(colour);
    foreground.push_back(found == rgb.end() ? 0x01000000u : found->second);
  }
  for (const Cell& c : cells_) {
    uint32_t cp = 0x20;
    const std::string_view glyph = glyphOf(c);
    const unsigned char b0 = glyph.empty() ? 0 : static_cast<unsigned char>(glyph[0]);
    if (b0 < 0x80)
      cp = b0;
    else if (b0 >= 0xF0 && glyph.size() >= 4)
      cp = ((b0 & 0x07u) << 18) | ((glyph[1] & 0x3fu) << 12) | ((glyph[2] & 0x3fu) << 6) | (glyph[3] & 0x3fu);
    else if (b0 >= 0xE0 && glyph.size() >= 3)
      cp = ((b0 & 0x0fu) << 12) | ((glyph[1] & 0x3fu) << 6) | (glyph[2] & 0x3fu);
    else if (b0 >= 0xC0 && glyph.size() >= 2)
      cp = ((b0 & 0x1fu) << 6) | (glyph[1] & 0x3fu);
    put32(cp);
    put32(foreground[c.colour]);
    put32(0x01000000u);
  }
  static const char* const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  size_t i = 0;
  for (; i + 2 < bytes.size(); i += 3) {
    const uint32_t n = (static_cast<unsigned char>(bytes[i]) << 16) | (static_cast<unsigned char>(bytes[i + 1]) << 8) |
                       static_cast<unsigned char>(bytes[i + 2]);
    out += alphabet[(n >> 18) & 63];
    out += alphabet[(n >> 12) & 63];
    out += alphabet[(n >> 6) & 63];
    out += alphabet[n & 63];
  }
  if (i < bytes.size()) {
    uint32_t n = static_cast<unsigned char>(bytes[i]) << 16;
    if (i + 1 < bytes.size()) n |= static_cast<unsigned char>(bytes[i + 1]) << 8;
    out += alphabet[(n >> 18) & 63];
    out += alphabet[(n >> 12) & 63];
    out += i + 1 < bytes.size() ? alphabet[(n >> 6) & 63] : '=';
    out += '=';
  }
  return out;
}

void Grid::paste(const Grid& other, int row) {
  // the other grid's palette and long glyphs are its own: each cell is told again in this grid's
  std::vector<std::uint8_t> colours;
  for (const auto& c : other.palette_) colours.push_back(paint(c));
  for (int y = 0; y < other.h_ && row + y < h_; ++y)
    for (int x = 0; x < other.w_ && x < w_; ++x) {
      const Cell& from = other.cells_[static_cast<size_t>(y) * other.w_ + x];
      Cell& to = cells_[static_cast<size_t>(row + y) * w_ + x];
      to = from;
      to.colour = colours[from.colour];
      if (from.size == Cell::kLong) setGlyph(to, std::string(other.glyphOf(from)));
    }
}

std::string Grid::dump(bool color) const {
  std::string out;
  for (int y = 0; y < h_; ++y) {
    std::string row;
    std::string current;
    for (int x = 0; x < w_; ++x) {
      const Cell& c = cells_[static_cast<size_t>(y) * w_ + x];
      const std::string& colour = palette_[c.colour];
      if (color && colour != current) {
        row += colour.empty() ? "\x1b[0m" : "\x1b[" + colour + "m";
        current = colour;
      }
      row += glyphOf(c);
    }
    if (color && !current.empty()) row += "\x1b[0m";
    // trailing blanks are noise in a buffer
    const auto end = row.find_last_not_of(' ');
    out += (end == std::string::npos ? "" : row.substr(0, end + 1)) + "\n";
  }
  return out;
}

std::size_t Grid::sized(int w, int h) {
  const std::size_t cells = static_cast<std::size_t>(std::max(w, 0)) * static_cast<std::size_t>(std::max(h, 0));
  if (cells > kMaxCells)
    throw std::length_error("the drawing would take " + std::to_string(w) + " x " + std::to_string(h) +
                            " characters, too large for text: draw it as svg, or select less");
  return cells;
}

}  // namespace render
}  // namespace ds
