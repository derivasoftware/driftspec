#ifndef DRIFTSPEC_LAYOUT_SEQUENCELEAST_HPP
#define DRIFTSPEC_LAYOUT_SEQUENCELEAST_HPP

namespace ds {
namespace layout {

/// The least room a sink's marks need in a sequence (DS-464), in its units: around a head's label, a box head or an
/// actor's alike, a head's height, a message to its own lane, and between a frame's edge and the next, so two frames
/// that open or close at one message keep an edge each. The
/// spacing takes the larger of it and its proportion, so a sink whose marks need more than the proportions give, as a
/// terminal's boxed head does, still draws them whole.
struct SequenceLeast {
  int headPad = 0;
  int headHeight = 0;
  int selfHeight = 0;
  int frameGap = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_SEQUENCELEAST_HPP
