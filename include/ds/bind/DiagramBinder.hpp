#ifndef DRIFTSPEC_BIND_DIAGRAMBINDER_HPP
#define DRIFTSPEC_BIND_DIAGRAMBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/FormulaBinder.hpp"
#include "ds/bind/INodeBinder.hpp"

namespace ds {
namespace bind {

/// A diagram (DS-356): its fields, and the table its clauses say (DS-268).
class DiagramBinder : public INodeBinder {
 public:
  DiagramBinder(const FieldBinder& fields, const FormulaBinder& formulas) : fields_(fields), formulas_(formulas) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  /// A walkthrough's steps, each an entity numbered in the order written, with what it shows and lights (DS-468).
  void bindSteps(TSNode node, const BindContext& ctx, const model::Entity& diagram) const;

  const FieldBinder& fields_;
  const FormulaBinder& formulas_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_DIAGRAMBINDER_HPP
