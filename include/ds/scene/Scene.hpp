#ifndef DRIFTSPEC_SCENE_SCENE_HPP
#define DRIFTSPEC_SCENE_SCENE_HPP

#include <memory>
#include <vector>

#include "ds/model/Model.hpp"
#include "ds/scene/Document.hpp"
#include "ds/scene/Layers.hpp"
#include "ds/scene/Map.hpp"

namespace ds {
namespace scene {

/// The scene module's entry point (DS-126): what the rest of ds asks of the scene, the document of
/// the whole model. Writing it (JSON, the viewer page) is a format's job (ds::format).
class IScene {
 public:
  virtual ~IScene() = default;
  virtual SceneDocument build() const = 0;
};

/// The scene of a model (DS-126): placed once, the map built once from what was placed, and the
/// layers each perspective draws over the same boxes, taken once; build passes the base document
/// through every layer in order. Every box sits inside its parent, once, so a viewer can zoom from the
/// system down to a member without anything moving.
class Scene : public IScene {
 public:
  /// The scene of `model`, written in `metamodel`, which names the kinds it draws (DS-351).
  Scene(const model::Model& model, const model::Metamodel& metamodel);
  SceneDocument build() const override;

 private:
  SceneDocument base_;
  Map map_;
  std::vector<std::unique_ptr<ILayer>> layers_;
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_SCENE_HPP
