#ifndef DRIFTSPEC_BIND_LEGACYBINDER_HPP
#define DRIFTSPEC_BIND_LEGACYBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"
#include "ds/bind/INodeBinder.hpp"

namespace ds {
namespace bind {

/// The walk the binder had (DS-355). Since DS-356 every construct has a binder of its own and the walk takes no kind;
/// DS-357 removes it.
class LegacyBinder : public INodeBinder {
 public:
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_LEGACYBINDER_HPP
