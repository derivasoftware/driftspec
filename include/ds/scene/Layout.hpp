#ifndef DRIFTSPEC_SCENE_LAYOUT_HPP
#define DRIFTSPEC_SCENE_LAYOUT_HPP

#include "ds/model/Model.hpp"
#include "ds/scene/Document.hpp"

namespace ds {
namespace scene {

/// Where every box goes (REQ-00069-2): each package, module, interface, unit and row placed inside
/// its parent, once, with and without private members; the base document the layers draw over.
class Layout {
 public:
  /// Places the boxes of `model`, written in `metamodel`, which names the kind of each (DS-351).
  Layout(const model::Model& model, const model::Metamodel& metamodel);
  SceneDocument place() const;

 private:
  const model::Model& model_;
  const model::Metamodel& metamodel_;
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_LAYOUT_HPP
