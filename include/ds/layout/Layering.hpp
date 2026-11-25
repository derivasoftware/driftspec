#ifndef DRIFTSPEC_LAYOUT_LAYERING_HPP
#define DRIFTSPEC_LAYOUT_LAYERING_HPP

#include <map>
#include <utility>
#include <vector>

namespace ds {
namespace layout {

/// One component layered (DS-392): what the layerer works out and the orderer and the positioner read and add to.
/// A node is numbered in the component, its real nodes first and the dummies of long edges after them.
struct Layering {
  int real = 0;                                           ///< how many nodes are real, pseudo ones included
  std::map<int, int> local;                               ///< each node's number in the component, by its kid
  std::vector<int> widths;                                ///< by node; a dummy is a channel
  std::vector<int> heights;                               ///< by node; a dummy and a pseudo node take none
  std::vector<int> layer;                                 ///< by node
  int depth = 0;                                          ///< the last layer
  std::vector<std::vector<int>> succA;                    ///< by node: the nodes below it, through the dummies
  std::vector<std::vector<int>> predA;                    ///< by node: the nodes above it
  std::map<std::pair<int, int>, std::vector<int>> chain;  ///< the dummies of each long edge, top to bottom
  int total = 0;                                          ///< how many nodes, dummies included
  std::vector<std::vector<int>> rows;                     ///< the nodes of each layer, in order
  std::vector<int> rowY;                                  ///< by layer: where it starts
  std::vector<int> rowH;                                  ///< by layer: how tall it is
  std::vector<int> gapBelow;                              ///< by layer: the gap under it, its lanes included
  std::vector<int> x;                                     ///< by node
  int minX = 0;                                           ///< how far left the component reached before it was moved to 0
  std::vector<int> lineOf;                                ///< by node: the line of its layer it wrapped onto
  std::vector<int> lineTop;                               ///< by node: where that line starts in the layer
  std::vector<int> lineHeight;                            ///< by node: how tall that line is
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_LAYERING_HPP
