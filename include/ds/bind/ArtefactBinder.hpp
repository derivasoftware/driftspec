#ifndef DRIFTSPEC_BIND_ARTEFACTBINDER_HPP
#define DRIFTSPEC_BIND_ARTEFACTBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/INodeBinder.hpp"

namespace ds {
namespace bind {

/// Binds the process artefacts (DS-355): requirements, verifications and constraints, identified by their id (R5).
/// Nesting gives the relation (R3), and the artefacts nested in one are bound in it.
class ArtefactBinder : public INodeBinder {
 public:
  ArtefactBinder(const FieldBinder& fields, const AnnotationBinder& annotations, const ExtensionBinder& extensions)
      : fields_(fields), annotations_(annotations), extensions_(extensions) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  const FieldBinder& fields_;
  const AnnotationBinder& annotations_;
  const ExtensionBinder& extensions_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_ARTEFACTBINDER_HPP
