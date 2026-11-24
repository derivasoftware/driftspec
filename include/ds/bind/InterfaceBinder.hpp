#ifndef DRIFTSPEC_BIND_INTERFACEBINDER_HPP
#define DRIFTSPEC_BIND_INTERFACEBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/INodeBinder.hpp"
#include "ds/bind/SignatureBinder.hpp"

namespace ds {
namespace bind {

/// An interface of a box and its operations (DS-356): each operation's signature, and the use case it realises (DS-116).
class InterfaceBinder : public INodeBinder {
 public:
  InterfaceBinder(const FieldBinder& fields, const AnnotationBinder& annotations, const SignatureBinder& signatures)
      : fields_(fields), annotations_(annotations), signatures_(signatures) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  const FieldBinder& fields_;
  const AnnotationBinder& annotations_;
  const SignatureBinder& signatures_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_INTERFACEBINDER_HPP
