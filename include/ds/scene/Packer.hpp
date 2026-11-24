#ifndef DRIFTSPEC_SCENE_PACKER_HPP
#define DRIFTSPEC_SCENE_PACKER_HPP

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/scene/Box.hpp"
#include "ds/scene/Frame.hpp"
#include "ds/scene/Packed.hpp"

namespace ds {
namespace scene {

/// Where nested boxes go (DS-389): frames in, a box for each out. It knows no model, only sizes, arrangements and
/// identities; a shelf can be held to the width it came out at in an earlier packing, so a second packing of the same
/// boxes, some of them shorter, keeps every box as narrow as the first one made it.
class Packer {
 public:
  /// A packer that keeps in `widths` the width each shelved frame's shelves come out at or, `following` them, fills
  /// those shelves no wider than they were kept.
  Packer(std::map<std::string, double>* widths, bool following);

  /// Where every frame under `root`, the root too, lands, by its identity, from the root's top left corner.
  std::map<std::string, Box> pack(const Frame& root) const;

 private:
  Packed size(const Frame& frame) const;
  Packed fixed(const Frame& frame) const;
  Packed shelves(const Frame& frame) const;
  Packed tree(const Frame& frame) const;
  /// The width a frame's shelves fill: the one computed or, following, the one kept for the frame.
  double width(const std::string& id, double computed) const;
  /// Places the children left to right in shelves no wider than `target`, into `at`; the width and height used.
  static std::pair<double, double> shelf(const std::vector<Packed>& kids, double gap, double target,
                                         std::vector<std::pair<double, double>>& at);
  /// The width to aim at: the widest child, at least `atLeast`, or the side of the children's area by `factor`.
  static double target(const std::vector<Packed>& kids, double atLeast, double factor);
  /// Writes where a frame and everything under it land, the frame at `x`, `y`.
  static void place(const Frame& frame, const Packed& packed, double x, double y, std::map<std::string, Box>& out);

  std::map<std::string, double>* widths_;
  bool following_;
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_PACKER_HPP
