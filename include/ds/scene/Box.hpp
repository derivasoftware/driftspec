#ifndef DRIFTSPEC_SCENE_BOX_HPP
#define DRIFTSPEC_SCENE_BOX_HPP

namespace ds {
namespace scene {

/// A rectangle in the coordinates of the scene: one unit is a pixel at zoom 1.
struct Box {
  double x = 0;
  double y = 0;
  double w = 0;
  double h = 0;
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_BOX_HPP
