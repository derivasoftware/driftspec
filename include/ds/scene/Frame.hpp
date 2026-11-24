#ifndef DRIFTSPEC_SCENE_FRAME_HPP
#define DRIFTSPEC_SCENE_FRAME_HPP

#include <string>
#include <utility>
#include <vector>

namespace ds {
namespace scene {

/// A box to pack (DS-389): its identity, how its children are arranged and their frames. It is all the packer is
/// given: what the box stands for stays with the projection that made it.
struct Frame {
  /// How the children of a box are arranged.
  enum class Arrangement {
    Fixed,    ///< the box has its size and each child sits where it is told
    Shelves,  ///< the children fill shelves left to right, no wider than a target, with room around them
    Tree,     ///< the first child is centred over the row of the others, with room between them for a trunk
  };

  std::string id;                                ///< what the box is known by, to whoever reads where it lands
  Arrangement arrangement = Arrangement::Fixed;  ///< how its children are arranged
  std::vector<Frame> kids;                       ///< its children, in the order they are arranged
  double w = 0;                                  ///< Fixed: its width
  double h = 0;                                  ///< Fixed: its height
  std::vector<std::pair<double, double>> at;     ///< Fixed: where each child sits, from the top left corner
  double gap = 0;                                ///< Shelves: between children and shelves; Tree: in the row
  double atLeast = 0;                            ///< Shelves: the target is never narrower
  double factor = 1;                             ///< Shelves: the target is the side of the children's area by it
  double left = 0;                               ///< Shelves: the room left of the shelves
  double top = 0;                                ///< Shelves: the room above them
  double right = 0;                              ///< Shelves: the room right of them
  double bottom = 0;                             ///< Shelves: the room below them
  std::vector<Frame> ports;                      ///< Shelves: boxes on the left border, half outside
  double portTop = 0;                            ///< Shelves: where the first port sits
  double portGap = 0;                            ///< Shelves: between ports
  double drop = 0;                               ///< Tree: between the first child and the row
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_FRAME_HPP
