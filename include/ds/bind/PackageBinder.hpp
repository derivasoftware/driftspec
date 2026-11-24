#ifndef DRIFTSPEC_BIND_PACKAGEBINDER_HPP
#define DRIFTSPEC_BIND_PACKAGEBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/INodeBinder.hpp"

namespace ds {
namespace bind {

/// A package (R4, DS-356): its id is its scope and its name joined by @, and the flat form `package app@render {`
/// reopens render inside app. A package reopened adds no relation; what it holds is bound in it.
class PackageBinder : public INodeBinder {
 public:
  PackageBinder(const FieldBinder& fields, const AnnotationBinder& annotations) : fields_(fields), annotations_(annotations) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  const FieldBinder& fields_;
  const AnnotationBinder& annotations_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_PACKAGEBINDER_HPP
