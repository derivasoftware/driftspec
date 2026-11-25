#ifndef DRIFTSPEC_LAYOUT_SEQUENCESPACING_HPP
#define DRIFTSPEC_LAYOUT_SEQUENCESPACING_HPP

namespace ds {
namespace layout {

/// The distances a sequence is placed with (DS-464), in the sink's units. Each is a proportion written once, in the
/// advance of a character across and the height of a line down, scaled by the sink's measures and rounded; the
/// proportions are the SVG's, so its 8 px advance and 18 px line give exactly the pixels it always had. A frame's
/// label is measured in the small text it is written in.
struct SequenceSpacing {
  // across, in characters
  int laneGap = 0;         ///< between two heads
  int headPad = 0;         ///< a box head around its label
  int actorPad = 0;        ///< an actor's head around its label
  int crossPad = 0;        ///< around a label that crosses lanes
  int selfWidth = 0;       ///< how far a message to its own lane loops out
  int selfLead = 0;        ///< between the loop and its label
  int selfTail = 0;        ///< after its label, before the next head
  int edge = 0;            ///< between the outer heads and the margin
  int marginX = 0;         ///< around the drawing, across
  int frameInset = 0;      ///< how much further out a frame is per frame nested in it
  int frameSide = 0;       ///< between a frame and the lanes it wraps
  int selfFrameTail = 0;   ///< after a message to its own lane's label, inside its frame
  int frameLabelLead = 0;  ///< before a frame's kind
  int frameLabelGap = 0;   ///< between its kind and its condition
  int frameLabelTail = 0;  ///< after its condition
  int advance = 0;         ///< a character
  int smallAdvance = 0;    ///< a character of the small text a frame's label is written in
  // down, in lines
  int marginY = 0;        ///< above the heads
  int headHeight = 0;     ///< a head
  int headGap = 0;        ///< between the heads and the first row
  int rowHeight = 0;      ///< a message between two lanes
  int selfHeight = 0;     ///< a message to its own lane
  int lineOffset = 0;     ///< from a row's top to the line its arrow runs on
  int frameLead = 0;      ///< above a row, for each frame that opens at it
  int frameGap = 0;       ///< between a frame's edge and its rows
  int dividerLead = 0;    ///< above a row, for each else that comes before it
  int frameBottom = 0;    ///< below a frame's last row
  int bottomPad = 0;      ///< below the last row
  int dividerOffset = 0;  ///< an else's line below where its lead begins

  /// The distances in a sink that measures a character `advance` across, a line `lineHeight` down and a character of
  /// small text `smallAdvance` across.
  static SequenceSpacing of(int advance, int lineHeight, int smallAdvance);
  /// The SVG's proportions: across in characters of its 8 px advance, down in lines of its 18 px line (DS-477).
  static constexpr double kLine = 18.0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_SEQUENCESPACING_HPP
