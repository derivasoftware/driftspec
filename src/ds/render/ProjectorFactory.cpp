#include "ds/render/ProjectorFactory.hpp"

#include "ds/render/ClassProjector.hpp"
#include "ds/render/RequirementProjector.hpp"
#include "ds/render/SequenceProjector.hpp"
#include "ds/render/StateProjector.hpp"
#include "ds/render/TableProjector.hpp"
#include "ds/render/TourProjector.hpp"

namespace ds {
namespace render {

std::unique_ptr<IProjector> ProjectorFactory::of(const std::string& kind, const model::Model& model, const model::Metamodel& metamodel) {
  if (kind == "sequence") return std::make_unique<SequenceProjector>(model, metamodel);
  if (kind == "tour") return std::make_unique<TourProjector>(model, metamodel);
  if (kind == "requirement") return std::make_unique<RequirementProjector>(model, metamodel);
  if (kind == "state") return std::make_unique<StateProjector>(model, metamodel);
  if (kind == "table") return std::make_unique<TableProjector>(model, metamodel);
  return std::make_unique<ClassProjector>(model, metamodel);
}

}  // namespace render
}  // namespace ds
