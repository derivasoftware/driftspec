#ifndef DRIFTSPEC_LAYOUT_BAND_HPP
#define DRIFTSPEC_LAYOUT_BAND_HPP

namespace ds {
namespace layout {

/// The gap under a node's layer (DS-392): the band its outgoing jogs live in.
struct Band {
  int top = 0;
  int bottom = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_BAND_HPP
