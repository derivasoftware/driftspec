#ifndef DRIFTSPEC_RENDER_CLASSPROJECTOR_HPP
#define DRIFTSPEC_RENDER_CLASSPROJECTOR_HPP

#include <nlohmann/json_fwd.hpp>

#include "ds/axis/Cut.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/render/IProjector.hpp"

namespace ds {
namespace render {

/// A class diagram (DS-369): the boxes a diagram takes, and the cut it is lifted onto.
class ClassProjector : public IProjector {
 public:
  ClassProjector(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}
  nlohmann::json project(const model::Entity& diagram) const override;

 private:
  /// The boxes lifted onto a cut written: what it closes stands for what it holds (DS-229).
  nlohmann::json closeBoxes(nlohmann::json doc, const axis::Cut& cut) const;
  /// What a class diagram draws as a box: a class, an enum, or a function of a module (one box each, «function»).
  static bool isBox(const model::Entity& e);
  /// `Repository<1>` → `Repository<T>`, from the params field.
  static std::string displayName(const model::Entity& e);
  static std::string memberLine(const model::Entity& m);
  /// What a box is drawn as, the word its badge is read from: the same for a box of its own and for a row of the
  /// closed box that holds it (DS-244).
  static std::string classifierOf(const model::Entity& e);
  static std::string edgeKind(const std::string& relation);

  const model::Model& model_;
  const model::Metamodel& metamodel_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_CLASSPROJECTOR_HPP
