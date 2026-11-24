#ifndef DRIFTSPEC_LAYOUT_GRAPH_HPP
#define DRIFTSPEC_LAYOUT_GRAPH_HPP

#include <map>
#include <string>
#include <vector>

#include "ds/layout/Edge.hpp"
#include "ds/layout/Frame.hpp"

namespace ds {
namespace layout {

/// What an engine is given to place (DS-392): nested boxes of given sizes, the edges between them and how wide the
/// drawing may be. It refers to the boxes and owns none of them; whoever builds it keeps them.
struct Graph {
  const Frame* root = nullptr;           ///< the boxes, nested, each sized and arranged
  std::vector<Edge> edges;               ///< the edges between them, for an engine that routes
  double maxWidth = 0;                   ///< the widest the drawing may be; 0 is unbounded
  std::map<std::string, double> widths;  ///< the width an earlier placement kept for each shelved box
  bool following = false;                ///< whether to fill those shelves no wider than they were kept
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_GRAPH_HPP
