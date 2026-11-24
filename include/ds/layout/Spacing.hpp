#ifndef DRIFTSPEC_LAYOUT_SPACING_HPP
#define DRIFTSPEC_LAYOUT_SPACING_HPP

namespace ds {
namespace layout {

/// The room the layered engine leaves (DS-392), in the units of the drawing: pixels for an SVG, cells for text.
struct Spacing {
  int channel = 8;           ///< the width of the channel a long edge runs down; a container is never narrower than eight
  int gapX = 40;             ///< between two boxes of a layer
  int gapY = 56;             ///< between two layers, at least
  int lane = 12;             ///< one row per edge that turns in the gap between two layers
  int lanePad = 20;          ///< air above the first lane and below the last
  int containerPad = 18;     ///< around what a container holds
  int containerLabelH = 22;  ///< the height of a container's label
  double aspect = 1.6;       ///< the width over height the components of a container are packed towards
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_SPACING_HPP
