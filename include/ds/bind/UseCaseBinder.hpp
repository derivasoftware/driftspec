#ifndef DRIFTSPEC_BIND_USECASEBINDER_HPP
#define DRIFTSPEC_BIND_USECASEBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/INodeBinder.hpp"
#include "ds/bind/SignatureBinder.hpp"
#include "ds/bind/TypeBinder.hpp"

namespace ds {
namespace bind {

/// A use case (DS-77, DS-356): a contract of what goes in, what comes out and the outcomes it may end in, a
/// specific one keeping the signature and the outcomes of the generic one it specialises.
class UseCaseBinder : public INodeBinder {
 public:
  UseCaseBinder(const FieldBinder& fields, const AnnotationBinder& annotations, const TypeBinder& types, const SignatureBinder& signatures)
      : fields_(fields), annotations_(annotations), types_(types), signatures_(signatures) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  const FieldBinder& fields_;
  const AnnotationBinder& annotations_;
  const TypeBinder& types_;
  const SignatureBinder& signatures_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_USECASEBINDER_HPP
