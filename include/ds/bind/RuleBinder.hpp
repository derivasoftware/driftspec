#ifndef DRIFTSPEC_BIND_RULEBINDER_HPP
#define DRIFTSPEC_BIND_RULEBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/FormulaBinder.hpp"
#include "ds/bind/INodeBinder.hpp"

namespace ds {
namespace bind {

/// A rule or a definition (DS-94, DS-356): a design piece with a name, in the box, class or member it applies to,
/// with the formula its clauses say.
class RuleBinder : public INodeBinder {
 public:
  RuleBinder(const FieldBinder& fields, const AnnotationBinder& annotations, const FormulaBinder& formulas)
      : fields_(fields), annotations_(annotations), formulas_(formulas) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  const FieldBinder& fields_;
  const AnnotationBinder& annotations_;
  const FormulaBinder& formulas_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_RULEBINDER_HPP
