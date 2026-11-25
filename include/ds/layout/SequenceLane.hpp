#ifndef DRIFTSPEC_LAYOUT_SEQUENCELANE_HPP
#define DRIFTSPEC_LAYOUT_SEQUENCELANE_HPP

#include <cstddef>

namespace ds {
namespace layout {

/// A lane of a sequence graph (DS-464): how long its head's label is, in characters, and whether it is an actor, whose
/// head is a figure rather than a box.
struct SequenceLane {
  std::size_t labelLength = 0;
  bool actor = false;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_SEQUENCELANE_HPP
