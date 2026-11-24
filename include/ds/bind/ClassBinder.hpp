#ifndef DRIFTSPEC_BIND_CLASSBINDER_HPP
#define DRIFTSPEC_BIND_CLASSBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/INodeBinder.hpp"
#include "ds/bind/MemberBinder.hpp"

namespace ds {
namespace bind {

/// A class or a dataclass (DS-356): its bases, its header operators and its members; written with annotations only,
/// it extends the class defined elsewhere.
class ClassBinder : public INodeBinder {
 public:
  ClassBinder(const FieldBinder& fields, const AnnotationBinder& annotations, const ExtensionBinder& extensions,
              const MemberBinder& members)
      : fields_(fields), annotations_(annotations), extensions_(extensions), members_(members) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  /// An operator of the class's header set, declared in it.
  void declareOperator(const BindContext& ctx, const model::Entity& cls, const std::string& symbol, const model::Location& location) const;
  const FieldBinder& fields_;
  const AnnotationBinder& annotations_;
  const ExtensionBinder& extensions_;
  const MemberBinder& members_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_CLASSBINDER_HPP
