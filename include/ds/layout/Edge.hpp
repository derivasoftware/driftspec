#ifndef DRIFTSPEC_LAYOUT_EDGE_HPP
#define DRIFTSPEC_LAYOUT_EDGE_HPP

#include <string>

namespace ds {
namespace layout {

/// An edge between two boxes (DS-392): by their identities, with the kind of relation it stands for, for an engine
/// that orders its layers by it and routes it.
struct Edge {
  std::string from;
  std::string to;
  std::string kind;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_EDGE_HPP
