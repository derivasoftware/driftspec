#include "ds/layout/TableSpacing.hpp"

#include <cmath>

namespace ds {
namespace layout {

namespace {
// the SVG's proportions: across in characters of its 8 px advance, down in lines of its 18 px line
constexpr double kAdvance = 8.0;
constexpr double kLine = 18.0;
}  // namespace

TableSpacing TableSpacing::of(int advance, int lineHeight) {
  const auto across = [advance](double pixels) { return static_cast<int>(std::lround(pixels / kAdvance * advance)); };
  const auto down = [lineHeight](double pixels) { return static_cast<int>(std::lround(pixels / kLine * lineHeight)); };
  TableSpacing s;
  s.advance = advance;
  s.padX = across(14);
  s.headHeight = down(38);
  s.rowHeight = down(34);
  s.lineHeight = down(17);
  s.linePad = down(17);
  s.wrap = 40;  // DS-268: a cell is as wide as this many characters; what is longer goes on more lines
  return s;
}

}  // namespace layout
}  // namespace ds
