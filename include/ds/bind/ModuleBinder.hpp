#ifndef DRIFTSPEC_BIND_MODULEBINDER_HPP
#define DRIFTSPEC_BIND_MODULEBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/INodeBinder.hpp"

namespace ds {
namespace bind {

/// A module (DS-356): a box qualified in its package. Reopened, its fields add up and no relation is added twice;
/// what it holds is bound in it.
class ModuleBinder : public INodeBinder {
 public:
  ModuleBinder(const FieldBinder& fields, const AnnotationBinder& annotations) : fields_(fields), annotations_(annotations) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  const FieldBinder& fields_;
  const AnnotationBinder& annotations_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_MODULEBINDER_HPP
