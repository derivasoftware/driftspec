#include "ds/scene/Layers.hpp"

namespace ds {
namespace scene {

// the one place that names the layers, in the order they decorate: the order the document is written in
std::vector<std::unique_ptr<ILayer>> LayerFactory::of(const model::Model& model, const model::Metamodel& metamodel, const Map& map) {
  std::vector<std::unique_ptr<ILayer>> out;
  out.push_back(std::make_unique<StructureLayer>(model, metamodel, map));
  out.push_back(std::make_unique<BehaviourLayer>(model, map));
  out.push_back(std::make_unique<TraceLayer>(model, map));
  out.push_back(std::make_unique<RulesLayer>(model, metamodel, map));
  out.push_back(std::make_unique<WalkthroughLayer>(model, map));
  return out;
}

}  // namespace scene
}  // namespace ds
