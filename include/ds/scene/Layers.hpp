#ifndef DRIFTSPEC_SCENE_LAYERS_HPP
#define DRIFTSPEC_SCENE_LAYERS_HPP

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Model.hpp"
#include "ds/scene/Document.hpp"
#include "ds/scene/Map.hpp"

namespace ds {
namespace scene {

/// What one perspective draws over the map (DS-126): a layer is built with the model and the map,
/// keeps them, and adds its part of the document; it knows no other layer.
class ILayer {
 public:
  virtual ~ILayer() = default;
  virtual void decorate(SceneDocument& out_document) = 0;
};

/// Structure: the relations between boxes, lifted to what the map draws, what each diagram shows, and
/// the titles of what the nodes answer to (REQ-00072-2). What a diagram shows is read in `metamodel` (DS-351).
class StructureLayer : public ILayer {
 public:
  StructureLayer(const model::Model& model, const model::Metamodel& metamodel, const Map& map);
  void decorate(SceneDocument& out_document) override;

 private:
  void links(SceneDocument& out_document) const;
  void diagrams(SceneDocument& out_document) const;
  void titles(SceneDocument& out_document) const;
  const model::Model& model_;
  const model::Metamodel& metamodel_;
  const Map& map_;
};

/// Behaviour: every public behaviour, each written once, and the tours that replay them (REQ-00073-2).
class BehaviourLayer : public ILayer {
 public:
  BehaviourLayer(const model::Model& model, const Map& map);
  void decorate(SceneDocument& out_document) override;

 private:
  void tours(SceneDocument& out_document);
  int behaviourOf(const std::string& sequence, const std::string& scenario, SceneDocument& out_document);
  std::map<std::pair<std::string, std::string>, int> index_;
  std::map<std::string, std::string> calls_;  // message -> the operation it calls
  const model::Model& model_;
  const Map& map_;
};

/// Trace: requirements, constraints and use cases, what answers to them and what proves them
/// (REQ-00078-1, REQ-00083-1).
class TraceLayer : public ILayer {
 public:
  TraceLayer(const model::Model& model, const Map& map);
  void decorate(SceneDocument& out_document) override;

 private:
  void requirements(SceneDocument& out_document);
  void verifications(SceneDocument& out_document);
  void answers(SceneDocument& out_document) const;
  std::map<std::string, size_t> at_;
  std::map<std::string, size_t> verificationAt_;
  const model::Model& model_;
  const Map& map_;
};

/// Rules: each rule and definition where it is written, and what breaks it, placed on the map (REQ-00099-1); the
/// kind of each is named by `metamodel` (DS-351).
class RulesLayer : public ILayer {
 public:
  RulesLayer(const model::Model& model, const model::Metamodel& metamodel, const Map& map);
  void decorate(SceneDocument& out_document) override;

 private:
  std::string place(std::string id) const;
  const model::Model& model_;
  const model::Metamodel& metamodel_;
  const Map& map_;
};

/// The one place that names the layers, in the order they decorate; those that read the language are handed
/// `metamodel` (DS-351).
std::vector<std::unique_ptr<ILayer>> layers(const model::Model& model, const model::Metamodel& metamodel, const Map& map);

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_LAYERS_HPP
