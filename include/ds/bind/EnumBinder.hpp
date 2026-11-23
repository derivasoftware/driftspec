#ifndef DRIFTSPEC_BIND_ENUMBINDER_HPP
#define DRIFTSPEC_BIND_ENUMBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/INodeBinder.hpp"

namespace ds {
namespace bind {

/// An enum and its values (DS-356).
class EnumBinder : public INodeBinder {
 public:
  explicit EnumBinder(const AnnotationBinder& annotations) : annotations_(annotations) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  const AnnotationBinder& annotations_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_ENUMBINDER_HPP
