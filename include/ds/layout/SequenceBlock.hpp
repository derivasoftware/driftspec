#ifndef DRIFTSPEC_LAYOUT_SEQUENCEBLOCK_HPP
#define DRIFTSPEC_LAYOUT_SEQUENCEBLOCK_HPP

#include <cstddef>
#include <vector>

namespace ds {
namespace layout {

/// A frame of a sequence graph (DS-464): the rows it spans, how long its kind and its condition are, in characters, and
/// the row each of its elses comes before.
struct SequenceBlock {
  int first = 0;
  int last = 0;
  std::size_t classifierLength = 0;
  std::size_t labelLength = 0;
  std::vector<int> dividers;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_SEQUENCEBLOCK_HPP
