#ifndef DRIFTSPEC_RENDER_SEQUENCEROW_HPP
#define DRIFTSPEC_RENDER_SEQUENCEROW_HPP

namespace ds {
namespace render {

/// Where one message of a sequence lands (DS-400), in the units of the technology that draws it: the top of its row,
/// the line its arrow runs on, and the bottom of its row.
struct SequenceRow {
  int top = 0;
  int line = 0;
  int bottom = 0;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SEQUENCEROW_HPP
