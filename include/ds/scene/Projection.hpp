#ifndef DRIFTSPEC_SCENE_PROJECTION_HPP
#define DRIFTSPEC_SCENE_PROJECTION_HPP

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/layout/Frame.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/scene/Document.hpp"

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
  const layout::Frame& root() const;
  /// What each box drawn says it is, by its identity, still without a place. A frame that only groups, a tree of a
  /// taxonomy or the root, has none.
  const std::map<std::string, Node>& nodes() const;

 private:
  // Sizes in scene units (DS-477: members, no hidden namespace). A row is one member; a unit is sized by its longest row.
  static constexpr double kRow = 11;
  static constexpr double kHead = 22;
  static constexpr double kUnitPadBottom = 6;
  static constexpr double kCharWidth = 3.95;
  static constexpr double kUnitMinW = 150;
  static constexpr double kUnitMaxW = 430;
  static constexpr double kModulePad = 24;
  static constexpr double kModuleHead = 46;
  static constexpr double kUnitGap = 22;
  static constexpr double kPackagePad = 60;
  static constexpr double kPackageHead = 90;
  static constexpr double kBoxGap = 70;
  static constexpr double kLeftForInterfaces = 300;
  static constexpr double kInterfaceW = 360;
  static constexpr double kOperationRow = 30;
  static constexpr double kTopGap = 120;
  static constexpr double kTreeGap = 44;  // between a base and the row of what inherits from it: room for the trunk

  layout::Frame package(const model::Entity& e, const std::string& parent);
  layout::Frame module(const model::Entity& e, const std::string& parent);
  layout::Frame interface(const model::Entity& e, const std::string& parent);
  layout::Frame unit(const model::Entity& e, const std::string& parent);
  /// A tree of a taxonomy as one frame: the base, then a frame for each of what inherits from it.
  layout::Frame tree(layout::Frame base, std::map<std::string, layout::Frame>& units,
                     const std::map<std::string, std::vector<std::string>>& below);
  /// The taxonomy of a module: each unit under the first base it has in the same module.
  std::map<std::string, std::string> taxonomy(const std::vector<layout::Frame>& units) const;
  /// A package's boxes, a box whose classes are inherited from before the boxes that inherit (DS-388).
  void order(std::vector<layout::Frame>& boxes) const;
  const std::vector<const model::Entity*>& children(const std::string& scope) const;
  Node& node(const std::string& id, const std::string& kind, const std::string& parent, const std::string& label,
             const std::string& stereotype, const std::string& vis, const std::string& linkedFrom);
  /// What a box says it is: abstract is derived by the checker (R14), never written.
  std::string stereotypeOf(const model::Entity& e) const;
  /// A box of a size with nothing arranged in it yet.
  static layout::Frame leaf(const std::string& id, double w, double h);
  static std::string visibility(const model::Entity& e);
  static std::string signature(const std::string& name, const std::vector<model::Param>& params, const std::string& returns);

  const model::Metamodel& metamodel_;
  bool lean_;
  std::map<std::string, std::vector<const model::Entity*>> byScope_;
  std::map<std::string, std::vector<std::string>> linked_;  // requirements and constraints, by the entity linked to them
  std::map<std::string, std::vector<std::string>> bases_;   // what each class inherits from or realises
  std::map<std::string, Node> nodes_;
  layout::Frame root_;
};

}  // namespace scene
}  // namespace ds

#endif  // DRIFTSPEC_SCENE_PROJECTION_HPP
