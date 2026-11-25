#ifndef DRIFTSPEC_RENDER_SEQLINE_HPP
#define DRIFTSPEC_RENDER_SEQLINE_HPP

#include <string>

namespace ds {
namespace render {

/// A lifeline of a sequence drawn as text (DS-371): its id, its label, the column of its centre and its width.
struct SeqLine {
  std::string id;
  std::string label;
  int centre = 0;
  int width = 0;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SEQLINE_HPP
