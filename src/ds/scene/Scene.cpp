#include "ds/scene/Scene.hpp"

#include "ds/scene/Layout.hpp"

namespace ds {
namespace scene {

// placed once, the map built once from what was placed, the layers taken once: kept for every build
Scene::Scene(const model::Model& model, const model::Metamodel& metamodel)
    : base_(Layout(model, metamodel).place()), map_(base_.nodes), layers_(LayerFactory::of(model, metamodel, map_)) {
  // what each kind is part of, as the metamodel says it (DS-427): the layers keep what each perspective draws
  for (const auto& k : metamodel.kinds())
    base_.categories.push_back({metamodel.nameOf(k.kind), model::Metamodel::categoryName(k.category)});
}

// the base document passed through every layer, in order
SceneDocument Scene::build() const {
  SceneDocument document = base_;
  for (const auto& layer : layers_) layer->decorate(document);
  return document;
}

}  // namespace scene
}  // namespace ds
