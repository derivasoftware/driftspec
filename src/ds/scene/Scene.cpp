#include "ds/scene/Scene.hpp"

#include "ds/scene/Layout.hpp"

namespace ds {
namespace scene {

// placed once, the map built once from what was placed, the layers taken once: kept for every build
Scene::Scene(const model::Model& model) : base_(Layout(model).place()), map_(base_.nodes), layers_(layers(model, map_)) {}

// the base document passed through every layer, in order
SceneDocument Scene::build() const {
  SceneDocument document = base_;
  for (const auto& layer : layers_) layer->decorate(document);
  return document;
}

}  // namespace scene
}  // namespace ds
