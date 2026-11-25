#ifndef DRIFTSPEC_RENDER_PROJECTORFACTORY_HPP
#define DRIFTSPEC_RENDER_PROJECTORFACTORY_HPP

#include <memory>
#include <string>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/render/IProjector.hpp"

namespace ds {
namespace render {

/// The one place that names the projectors (DS-369), by the kind of diagram they draw: a class diagram for any kind
/// without a projector of its own.
class ProjectorFactory {
 public:
  static std::unique_ptr<IProjector> of(const std::string& kind, const model::Model& model, const model::Metamodel& metamodel);
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_PROJECTORFACTORY_HPP
