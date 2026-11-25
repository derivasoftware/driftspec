#ifndef DRIFTSPEC_RENDER_EDGEMARKUP_HPP
#define DRIFTSPEC_RENDER_EDGEMARKUP_HPP

#include <string>

namespace ds {
namespace render {

/// An edge drawn in SVG (DS-371): its path and its label, written apart so the labels go above every path.
struct EdgeMarkup {
  std::string path;
  std::string label;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_EDGEMARKUP_HPP
