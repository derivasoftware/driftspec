#include "ds/layout/SequenceSpacing.hpp"

#include <algorithm>
#include <cmath>

namespace ds {
namespace layout {

namespace {
// the SVG's proportions: across in characters of its 8 px advance, down in lines of its 18 px line
constexpr double kLine = 18.0;
}  // namespace

SequenceSpacing SequenceSpacing::of(int advance, int lineHeight, int smallAdvance, const SequenceLeast& least) {
  const auto across = [advance](double characters) { return static_cast<int>(std::lround(characters * advance)); };
  const auto down = [lineHeight](double pixels) { return static_cast<int>(std::lround(pixels / kLine * lineHeight)); };
  SequenceSpacing s;
  s.laneGap = across(3);
  s.headPad = std::max(across(2.5), least.headPad);
  s.actorPad = std::max(across(1), least.headPad);  // where the sink draws an actor as a box, it needs a box's room
  s.crossPad = across(4);
  s.selfWidth = across(4);
  s.selfLead = across(0.75);
  s.selfTail = across(1);
  s.edge = across(2);
  s.marginX = across(2.5);
  s.frameInset = across(2);
  s.frameSide = across(5.5);
  s.selfFrameTail = across(1.5);
  s.frameLabelLead = across(1);
  s.frameLabelGap = across(1.25);
  s.frameLabelTail = across(2.5);
  s.advance = advance;
  s.smallAdvance = smallAdvance;
  s.marginY = down(20);
  s.headHeight = std::max(down(38), least.headHeight);
  s.headGap = down(16);
  s.rowHeight = down(30);
  s.selfHeight = std::max(down(42), least.selfHeight);
  s.lineOffset = down(20);
  s.frameLead = down(30);
  s.frameGap = std::max(down(8), least.frameGap);
  s.dividerLead = down(26);
  s.frameBottom = down(2);
  s.bottomPad = down(16);
  s.dividerOffset = down(6);
  return s;
}

}  // namespace layout
}  // namespace ds
