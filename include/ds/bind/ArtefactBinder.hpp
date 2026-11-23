#ifndef DRIFTSPEC_BIND_ARTEFACTBINDER_HPP
#define DRIFTSPEC_BIND_ARTEFACTBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"
#include "ds/bind/INodeBinder.hpp"

namespace ds {
namespace bind {

/// Binds the process artefacts (DS-355): requirements, verifications and constraints, identified by their id (R5).
/// Nesting gives the relation (R3), and the artefacts nested in one are bound in it.
class ArtefactBinder : public INodeBinder {
 public:
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_ARTEFACTBINDER_HPP
