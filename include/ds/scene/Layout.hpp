#ifndef DRIFTSPEC_SCENE_LAYOUT_HPP
#define DRIFTSPEC_SCENE_LAYOUT_HPP

#include "ds/model/Model.hpp"
#include "ds/scene/Document.hpp"

namespace ds {
namespace scene {

/// Where every box goes (REQ-00069-1): each package, module, interface, unit and row placed inside
/// its parent, once, with and without private members; the base document the layers draw over.
class Layout {
 public:
  explicit Layout(const model::Model& model);
  SceneDocument place() const;

 private:
  const model::Model& model_;
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_LAYOUT_HPP
