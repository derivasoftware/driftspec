#ifndef DRIFTSPEC_LAYOUT_ROWPLACE_HPP
#define DRIFTSPEC_LAYOUT_ROWPLACE_HPP

namespace ds {
namespace layout {

/// Where a row lands (DS-464): its top, the line its message's arrow runs on, and its bottom, in the sink's units.
struct RowPlace {
  int top = 0;
  int line = 0;
  int bottom = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ROWPLACE_HPP
