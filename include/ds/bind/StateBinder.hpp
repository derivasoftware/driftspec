#ifndef DRIFTSPEC_BIND_STATEBINDER_HPP
#define DRIFTSPEC_BIND_STATEBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/INodeBinder.hpp"

namespace ds {
namespace bind {

/// A state machine (DS-356): the behaviour of a class, with its initial state, its rules, its views and its
/// transitions; the events and actions it names are kept for the checker.
class StateBinder : public INodeBinder {
 public:
  StateBinder(const FieldBinder& fields, const AnnotationBinder& annotations) : fields_(fields), annotations_(annotations) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  const FieldBinder& fields_;
  const AnnotationBinder& annotations_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_STATEBINDER_HPP
