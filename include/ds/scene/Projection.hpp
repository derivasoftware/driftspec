#ifndef DRIFTSPEC_SCENE_PROJECTION_HPP
#define DRIFTSPEC_SCENE_PROJECTION_HPP

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/scene/Document.hpp"
#include "ds/scene/Frame.hpp"

namespace ds {
namespace scene {

/// What the scene places (DS-389): the model as nested frames, each package, module, interface, unit and row sized by
/// what it shows and its children in the order they are read, with what each box drawn says it is. How the frames are
/// packed is the Packer's; the projection decides only what goes in each box and how much room it asks for.
class Projection {
 public:
  /// The frames of `model`, written in `metamodel`; `lean` leaves out private members.
  Projection(const model::Model& model, const model::Metamodel& metamodel, bool lean);

  /// One frame that holds the model's packages, in the order they are read.
  const Frame& root() const;
  /// What each box drawn says it is, by its identity, still without a place. A frame that only groups, a tree of a
  /// taxonomy or the root, has none.
  const std::map<std::string, Node>& nodes() const;

 private:
  Frame package(const model::Entity& e, const std::string& parent);
  Frame module(const model::Entity& e, const std::string& parent);
  Frame interface(const model::Entity& e, const std::string& parent);
  Frame unit(const model::Entity& e, const std::string& parent);
  /// A tree of a taxonomy as one frame: the base, then a frame for each of what inherits from it.
  Frame tree(Frame base, std::map<std::string, Frame>& units, const std::map<std::string, std::vector<std::string>>& below);
  /// The taxonomy of a module: each unit under the first base it has in the same module.
  std::map<std::string, std::string> taxonomy(const std::vector<Frame>& units) const;
  /// A package's boxes, a box whose classes are inherited from before the boxes that inherit (DS-388).
  void order(std::vector<Frame>& boxes) const;
  const std::vector<const model::Entity*>& children(const std::string& scope) const;
  Node& node(const std::string& id, const std::string& kind, const std::string& parent, const std::string& label,
             const std::string& stereotype, const std::string& vis, const std::string& linkedFrom);
  /// What a box says it is: abstract is derived by the checker (R14), never written.
  std::string stereotypeOf(const model::Entity& e) const;
  /// A box of a size with nothing arranged in it yet.
  static Frame leaf(const std::string& id, double w, double h);
  static std::string visibility(const model::Entity& e);
  static std::string signature(const std::string& name, const std::vector<model::Param>& params, const std::string& returns);

  const model::Metamodel& metamodel_;
  bool lean_;
  std::map<std::string, std::vector<const model::Entity*>> byScope_;
  std::map<std::string, std::vector<std::string>> linked_;  // requirements and constraints, by the entity linked to them
  std::map<std::string, std::vector<std::string>> bases_;   // what each class inherits from or realises
  std::map<std::string, Node> nodes_;
  Frame root_;
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_PROJECTION_HPP
