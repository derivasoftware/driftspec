#ifndef DRIFTSPEC_RENDER_SEQUENCEDIVIDER_HPP
#define DRIFTSPEC_RENDER_SEQUENCEDIVIDER_HPP

#include <string>

namespace ds {
namespace render {

/// An else of a frame (DS-400): the message it comes before, where its line lands, and the condition it says.
struct SequenceDivider {
  int row = 0;
  int y = 0;
  std::string label;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SEQUENCEDIVIDER_HPP
