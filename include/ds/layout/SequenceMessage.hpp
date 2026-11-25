#ifndef DRIFTSPEC_LAYOUT_SEQUENCEMESSAGE_HPP
#define DRIFTSPEC_LAYOUT_SEQUENCEMESSAGE_HPP

#include <cstddef>

namespace ds {
namespace layout {

/// A message of a sequence graph (DS-464): the lanes it runs from and to (the same for a message to itself), how long
/// its label is, in characters, and the row it is drawn on.
struct SequenceMessage {
  std::size_t from = 0;
  std::size_t to = 0;
  std::size_t labelLength = 0;
  int row = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_SEQUENCEMESSAGE_HPP
