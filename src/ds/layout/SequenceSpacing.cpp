#include "ds/layout/SequenceSpacing.hpp"

#include <cmath>

namespace ds {
namespace layout {

SequenceSpacing SequenceSpacing::of(int advance, int lineHeight, int smallAdvance) {
  const auto across = [advance](double characters) { return static_cast<int>(std::lround(characters * advance)); };
  const auto down = [lineHeight](double pixels) { return static_cast<int>(std::lround(pixels / kLine * lineHeight)); };
  SequenceSpacing s;
  s.laneGap = across(3);
  s.headPad = across(2.5);
  s.actorPad = across(1);
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
  s.headHeight = down(38);
  s.headGap = down(16);
  s.rowHeight = down(30);
  s.selfHeight = down(42);
  s.lineOffset = down(20);
  s.frameLead = down(30);
  s.frameGap = down(8);
  s.dividerLead = down(26);
  s.frameBottom = down(2);
  s.bottomPad = down(16);
  s.dividerOffset = down(6);
  return s;
}

}  // namespace layout
}  // namespace ds
